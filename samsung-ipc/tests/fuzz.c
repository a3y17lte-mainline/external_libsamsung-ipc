/*
 * This file is part of libsamsung-ipc.
 *
 * Copyright (C) 2026 Martin Štrobl
 *
 * libsamsung-ipc is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * libsamsung-ipc is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libsamsung-ipc.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * A libFuzzer harness for everything that takes bytes from the modem: the
 * FMT and RFS framing and the helpers that read message data. Not part of
 * the automake build:
 *
 *   clang -g -O1 -fsanitize=fuzzer,address,undefined -Iinclude \
 *     -Isamsung-ipc -o fuzz \
 *     samsung-ipc/tests/fuzz.c \
 *     $(find samsung-ipc -name '*.c' -not -path '*tests*') -lcrypto
 *   ./fuzz -max_len=8192 corpus/
 *
 * The input: one byte that picks FMT or RFS and how the frame is split over
 * two reads, then the bytes the transport returns. The data of a received
 * message, and the raw input as well, then go through every helper.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <samsung-ipc.h>

#include <ipc.h>

struct fuzz_transport {
	const uint8_t *data;
	size_t size;
	size_t first;
	unsigned int reads;
};

static int fuzz_read(__attribute__((unused)) struct ipc_client *client,
		     void *data, void *buffer, size_t length)
{
	struct fuzz_transport *transport = data;
	size_t size;

	/* The first read ends where the input says, the second has the rest */
	size = transport->reads == 0 ? transport->first : transport->size;
	if (size > transport->size)
		size = transport->size;
	if (size > length)
		size = length;

	memcpy(buffer, transport->data, size);
	transport->data += size;
	transport->size -= size;
	transport->reads++;

	return (int) size;
}

static void fuzz_helpers(const void *data, size_t size)
{
	struct ipc_gprs_ip_configuration_ext config;
	struct ipc_gprs_call_status_ext_data status;
	struct ipc_net_regist_response_ext_data reg;
	struct ipc_sec_ims_auth_result auth;
	volatile unsigned char sink = 0;
	unsigned int i, j;
	size_t length;
	void *p;
	char *s;
	int count;

	/* Calls */
	count = ipc_call_list_count_extract(data, size);
	for (i = 0; i < (unsigned int) count + 1; i++) {
		struct ipc_call_list_entry *entry =
			ipc_call_list_entry_extract(data, size, i);

		if (entry == NULL)
			continue;

		s = ipc_call_list_entry_number_extract(entry);
		if (s != NULL)
			sink += strlen(s);
		free(s);
	}

	/* Identities and SMS */
	s = ipc_misc_me_imsi_imsi_extract(data, size);
	if (s != NULL)
		sink += strlen(s);
	free(s);

	length = ipc_sms_incoming_msg_pdu_size_extract(data, size);
	p = ipc_sms_incoming_msg_pdu_extract(data, size);
	if (p != NULL && length > 0)
		sink += ((unsigned char *) p)[length - 1];

	length = ipc_sms_svc_center_addr_smsc_size_extract(data, size);
	p = ipc_sms_svc_center_addr_smsc_extract(data, size);
	if (p != NULL && length > 0)
		sink += ((unsigned char *) p)[length - 1];

	/* SIM */
	length = ipc_sec_rsim_access_size_extract(data, size);
	p = ipc_sec_rsim_access_extract(data, size);
	if (p != NULL && length > 0)
		sink += ((unsigned char *) p)[length - 1];

	count = ipc_sec_sim_apps_info_count(data, size);
	for (i = 0; count > 0 && i < (unsigned int) count + 1; i++) {
		const struct ipc_sec_sim_app *app =
			ipc_sec_sim_apps_info_app(data, size, i);

		if (app != NULL)
			sink += ((const unsigned char *) app)[sizeof(*app) - 1];
	}

	if (ipc_sec_ims_auth_response_parse(data, size, &auth) == 0) {
		for (i = 0; i < auth.count; i++)
			for (j = 0; j < auth.length[i]; j++)
				sink += auth.value[i][j];
	}

	/* Network and service mode lists */
	count = ipc_net_plmn_list_count_extract(data, size);
	for (i = 0; i < (unsigned int) count + 1; i++) {
		struct ipc_net_plmn_list_entry *entry =
			ipc_net_plmn_list_entry_extract(data, size, i);

		if (entry != NULL)
			sink += entry->unknown[1];
	}

	count = ipc_svc_display_screen_count_extract(data, size);
	for (i = 0; i < (unsigned int) count + 1; i++) {
		struct ipc_svc_display_screen_entry *entry =
			ipc_svc_display_screen_extract(data, size, i);

		if (entry != NULL)
			sink += entry->line[sizeof(entry->line) - 1];
	}

	/* Data calls and IMS */
	if (ipc_gprs_ip_configuration_ext_parse(data, size, &config) == 0)
		sink += config.cid;
	if (ipc_gprs_call_status_ext_extract(data, size, &status) == 0)
		sink += status.cid;
	if (ipc_net_regist_response_ext_extract(data, size, &reg) == 0)
		sink += reg.volte;
	sink += ipc_ims_dedicated_bearer_filter_count(data, size);

	/* NV items the modem writes */
	length = ipc_rfs_nv_write_item_size_extract(data, size);
	p = ipc_rfs_nv_write_item_extract(data, size);
	if (p != NULL && length > 0)
		sink += ((unsigned char *) p)[length - 1];

	(void) sink;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	struct fuzz_transport transport;
	struct ipc_message message;
	struct ipc_client *client;

	if (size < 1)
		return 0;

	setenv("LIBSAMSUNG_IPC_DEVICE", "a3y17lte", 0);

	memset(&transport, 0, sizeof(transport));
	transport.data = data + 1;
	transport.size = size - 1;
	/* Whole in one read, or cut after 4 to 0x3fc bytes */
	transport.first = (data[0] & 0xfe) ? (size_t) (data[0] & 0xfe) * 2 :
					     size;

	client = ipc_client_create((data[0] & 1) ? IPC_CLIENT_TYPE_RFS :
						   IPC_CLIENT_TYPE_FMT);
	if (client == NULL)
		abort();

	ipc_client_transport_handlers_register(client, NULL, NULL, fuzz_read,
					       NULL, NULL, &transport);

	memset(&message, 0, sizeof(message));
	if (ipc_client_recv(client, &message) == 0) {
		ipc_command_string(message.command);
		ipc_response_type_string(message.type);
		if (message.data != NULL)
			fuzz_helpers(message.data, message.size);
		free(message.data);
	}

	ipc_client_destroy(client);

	/* The helpers on the raw input too, without a valid frame around it */
	if (size > 1) {
		/* An exact-size copy, so that reads past the end are seen */
		uint8_t *copy = malloc(size - 1);

		if (copy == NULL)
			abort();

		memcpy(copy, data + 1, size - 1);
		fuzz_helpers(copy, size - 1);
		free(copy);
	}

	return 0;
}
