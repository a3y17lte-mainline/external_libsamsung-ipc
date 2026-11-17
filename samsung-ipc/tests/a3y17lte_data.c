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
 * Layouts of include/shannon.h, checked against the byte offsets worked out
 * on the Galaxy A3 2017. The offsets in the comments are those of the whole
 * frame (7-byte IPC header first), as in the notes they come from; the
 * payload offset is 7 less.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <samsung-ipc.h>

#include "a3y17lte_data.h"

#define CHECK(condition)						\
	do {								\
		if (!(condition)) {					\
			printf("[ !! ] %s:%d: check failed: %s\n",	\
			       __FILE__, __LINE__, #condition);		\
			return -1;					\
		}							\
	} while (0)

/* Payload offset of a frame offset */
#define AT(frame_offset)	((frame_offset) - 7)

int test_shannon_layout_sizes(__attribute__((unused)) struct ipc_client *client)
{
	CHECK(sizeof(struct ipc_sec_sim_app) == 24);
	CHECK(sizeof(struct ipc_sec_rsim_aid_request_header) == 0x2c - 7);
	CHECK(sizeof(struct ipc_sec_apdu_request_data) == 0x10e - 7);
	CHECK(sizeof(struct ipc_gprs_set_apn_info_data) == 0xcb - 7);
	CHECK(sizeof(struct ipc_ims_reg_state_data) == 0x0d - 7);
	CHECK(sizeof(struct ipc_ims_channel_data) == 0x34 - 7);
	CHECK(sizeof(struct ipc_ims_codec_data) == 0x41 - 7);
	CHECK(sizeof(struct ipc_ims_rtp_media_data) == 0x0b - 7);
	CHECK(sizeof(struct ipc_ims_dtmf_set_data) == 0x0d - 7);
	CHECK(sizeof(struct ipc_ims_dtmf_exec_data) == 0x0b - 7);
	CHECK(offsetof(struct ipc_ims_dtmf_set_data, rate) == 0x0a - 7);
	CHECK(offsetof(struct ipc_ims_dtmf_set_data, recv_payload_type) == 0x0c - 7);
	/* stock ImsSetCodec: packetization @0x18, mode set @0x21, rx type @0x23 */
	CHECK(offsetof(struct ipc_ims_codec_data, packetization) == 0x18 - 7);
	CHECK(offsetof(struct ipc_ims_codec_data, dtx) == 0x1d - 7);
	CHECK(offsetof(struct ipc_ims_codec_data, mode_set) == 0x21 - 7);
	CHECK(offsetof(struct ipc_ims_codec_data, rx_payload_type) == 0x23 - 7);
	CHECK(sizeof(struct ipc_ims_pdn_data) == 0x2d - 7);
	CHECK(sizeof(struct ipc_ims_engine_data) == 0x09 - 7);
	CHECK(sizeof(struct ipc_ims_option_data) == 0x4e - 7);
	CHECK(offsetof(struct ipc_ims_option_data, rtp_timeout) == AT(0x0c));
	CHECK(offsetof(struct ipc_ims_option_data, srtp_key) == AT(0x18));
	CHECK(offsetof(struct ipc_ims_option_data, av_sync) == AT(0x36));
	CHECK(offsetof(struct ipc_ims_option_data, rtcp_rs) == AT(0x3a));
	CHECK(offsetof(struct ipc_ims_option_data, xr_blocks) == AT(0x42));
	CHECK(offsetof(struct ipc_ims_option_data, xr_params) == AT(0x45));
	CHECK(offsetof(struct ipc_ims_option_data, echolocate) == AT(0x4d));
	CHECK(offsetof(struct ipc_net_regist_response_ext_data, tac) ==
	      AT(0x12));
	CHECK(offsetof(struct ipc_net_regist_response_ext_data, volte) ==
	      AT(0x14));
	CHECK(offsetof(struct ipc_net_regist_response_ext_data, pci) ==
	      AT(0x17));
	CHECK(sizeof(struct ipc_ims_dedicated_bearer_header) == 0x1a - 7);
	CHECK(sizeof(struct ipc_ims_bearer_filter) == 26);

	CHECK(IPC_IMS_REG_STATE == 0x1608);
	CHECK(IPC_IMS_DEDICATED_BEARER == 0x160B);
	CHECK(IPC_SEC_IMS_AUTH == 0x0509);

	return 0;
}

int test_shannon_sim_apps_info(__attribute__((unused))
			       struct ipc_client *client)
{
	unsigned char p[1 + 2 * 24];
	const struct ipc_sec_sim_app *app;
	static const unsigned char isim_aid[] = {
		0xa0, 0x00, 0x00, 0x00, 0x87, 0x10, 0x04
	};

	memset(p, 0, sizeof(p));
	p[AT(7)] = 2;
	/* Second record starts at frame offset 8 + 24 */
	p[AT(8 + 24)] = 5;
	p[AT(9 + 24)] = sizeof(isim_aid);
	memcpy(&p[AT(0x0a + 24)], isim_aid, sizeof(isim_aid));
	p[AT(0x1a + 24)] = 1;	/* PIN1 state */
	p[AT(0x1c + 24)] = 3;	/* first counter */

	CHECK(ipc_sec_sim_apps_info_count(p, sizeof(p)) == 2);
	CHECK(ipc_sec_sim_apps_info_count(p, sizeof(p) - 1) == 1);

	app = ipc_sec_sim_apps_info_app(p, sizeof(p), 1);
	CHECK(app != NULL);
	CHECK(app->type == 5);
	CHECK(app->aid_length == sizeof(isim_aid));
	CHECK(!memcmp(app->aid, isim_aid, sizeof(isim_aid)));
	CHECK(app->pin1_state == 1);
	CHECK(app->counters[0] == 3);
	CHECK(ipc_sec_sim_apps_info_app(p, sizeof(p), 2) == NULL);

	return 0;
}

int test_shannon_rsim_aid_setup(__attribute__((unused))
				struct ipc_client *client)
{
	struct ipc_sec_rsim_aid_request_header header;
	const unsigned char *b = (const unsigned char *) &header;
	static const unsigned char aid[] = { 0xa0, 0x00, 0x00, 0x00, 0x87 };
	/* MF, ADF of the application */
	static const unsigned short path[] = { 0x3f00, 0x7fff };

	CHECK(ipc_sec_rsim_aid_setup(&header, IPC_SEC_RSIM_COMMAND_READ_RECORD,
				     5, aid, sizeof(aid), 0x6f02, path, 2,
				     1, 4, 0x40) == 0);

	CHECK(b[AT(7)] == IPC_SEC_RSIM_COMMAND_READ_RECORD);
	CHECK(b[AT(8)] == 5);
	CHECK(b[AT(9)] == sizeof(aid));
	CHECK(!memcmp(&b[AT(0x0a)], aid, sizeof(aid)));
	CHECK(b[AT(0x1a)] == 0x02 && b[AT(0x1b)] == 0x6f);
	CHECK(b[AT(0x1c)] == 2);
	/* Path entries are little endian */
	CHECK(b[AT(0x1d)] == 0x00 && b[AT(0x1e)] == 0x3f);
	CHECK(b[AT(0x1f)] == 0xff && b[AT(0x20)] == 0x7f);
	CHECK(b[AT(0x29)] == 1 && b[AT(0x2a)] == 4 && b[AT(0x2b)] == 0x40);

	CHECK(ipc_sec_rsim_aid_setup(&header, 0, 0, aid, 17, 0, NULL, 0,
				     0, 0, 0) < 0);
	CHECK(ipc_sec_rsim_aid_setup(&header, 0, 0, NULL, 0, 0, path, 7,
				     0, 0, 0) < 0);

	return 0;
}

int test_shannon_apdu_setup(__attribute__((unused)) struct ipc_client *client)
{
	struct ipc_sec_apdu_request_data data;
	static const unsigned char open_channel[] = {
		0x00, 0x70, 0x00, 0x00, 0x01
	};

	CHECK(ipc_sec_apdu_setup(&data, open_channel,
				 sizeof(open_channel)) == 0);
	CHECK(data.length[0] == 0 && data.length[1] == 5);
	CHECK(!memcmp(data.apdu, open_channel, sizeof(open_channel)));
	CHECK(data.apdu[5] == 0);
	CHECK(ipc_sec_apdu_setup(&data, open_channel, IPC_SEC_APDU_MAX + 1) < 0);

	return 0;
}

int test_shannon_ims_auth(__attribute__((unused)) struct ipc_client *client)
{
	unsigned char rand[16], autn[16], request[64];
	struct ipc_sec_ims_auth_result result;
	unsigned char response[2 + 1 + 8 + 1 + 16 + 1 + 16];
	size_t size;

	memset(rand, 0x11, sizeof(rand));
	memset(autn, 0x22, sizeof(autn));

	size = ipc_sec_ims_auth_request_size(sizeof(rand), sizeof(autn));
	CHECK(size == 2 + 16 + 1 + 16);
	CHECK(ipc_sec_ims_auth_request_setup(request, sizeof(request),
					     IPC_SEC_IMS_AUTH_TYPE_IMS,
					     rand, sizeof(rand),
					     autn, sizeof(autn)) == 0);
	CHECK(request[AT(7)] == 0);
	CHECK(request[AT(8)] == 16);
	CHECK(request[AT(9)] == 0x11 && request[AT(9 + 15)] == 0x11);
	CHECK(request[AT(9 + 16)] == 16);
	CHECK(request[AT(9 + 17)] == 0x22);
	CHECK(ipc_sec_ims_auth_request_setup(request, size - 1, 0, rand, 16,
					     autn, 16) < 0);

	/* Success: RES (8 bytes), CK, IK */
	memset(response, 0, sizeof(response));
	response[0] = 0;
	response[1] = 0;
	response[2] = 8;
	memset(&response[3], 0xaa, 8);
	response[11] = 16;
	memset(&response[12], 0xbb, 16);
	response[28] = 16;
	memset(&response[29], 0xcc, 16);

	CHECK(ipc_sec_ims_auth_response_parse(response, sizeof(response),
					      &result) == 0);
	CHECK(result.count == 3);
	CHECK(result.length[0] == 8 && result.value[0][0] == 0xaa);
	CHECK(result.length[1] == 16 && result.value[1][15] == 0xbb);
	CHECK(result.length[2] == 16 && result.value[2][0] == 0xcc);

	/* USIM: four values, Kc last */
	{
		unsigned char usim[2 + 9 + 17 + 17 + 9];
		size_t n = 0;

		memset(usim, 0, sizeof(usim));
		usim[n++] = IPC_SEC_IMS_AUTH_TYPE_USIM;
		usim[n++] = 0;
		usim[n] = 8; n += 9;
		usim[n] = 16; n += 17;
		usim[n] = 16; n += 17;
		usim[n] = 8; usim[n + 8] = 0xee; n += 9;
		CHECK(ipc_sec_ims_auth_response_parse(usim, n, &result) == 0);
		CHECK(result.count == 4 && result.length[3] == 8);
		CHECK(result.value[3][7] == 0xee);
	}

	/* A value running past the end */
	CHECK(ipc_sec_ims_auth_response_parse(response, sizeof(response) - 1,
					      &result) < 0);

	/* Failure: no values */
	response[1] = 2;
	CHECK(ipc_sec_ims_auth_response_parse(response, 2, &result) == 0);
	CHECK(result.status == 2 && result.count == 0);

	/* Synchronisation failure: AUTS (14 bytes) alone, or with empty
	 * values after it as the stock RIL reads them */
	memset(response, 0, sizeof(response));
	response[1] = 4;
	response[2] = 14;
	memset(&response[3], 0xdd, 14);
	CHECK(ipc_sec_ims_auth_response_parse(response, 17, &result) == 0);
	CHECK(result.status == 4 && result.count == 1);
	CHECK(result.length[0] == 14 && result.value[0][13] == 0xdd);
	CHECK(ipc_sec_ims_auth_response_parse(response, 19, &result) == 0);
	CHECK(result.count == 3 && result.length[1] == 0);
	/* A success needs every value */
	response[1] = 0;
	CHECK(ipc_sec_ims_auth_response_parse(response, 17, &result) < 0);

	return 0;
}

int test_shannon_ip_configuration(__attribute__((unused))
				  struct ipc_client *client)
{
	unsigned char p[0x108 - 7];
	struct ipc_gprs_ip_configuration_ext config;
	static const unsigned char ip[4] = { 10, 1, 2, 3 };
	static const unsigned char pcscf1[4] = { 192, 0, 2, 1 };
	static const unsigned char pcscf2[4] = { 192, 0, 2, 2 };
	unsigned char pcscf6[16];

	memset(p, 0, sizeof(p));
	memset(pcscf6, 0, sizeof(pcscf6));
	pcscf6[0] = 0x20;
	pcscf6[1] = 0x01;
	pcscf6[15] = 0x05;

	p[AT(7)] = 1;
	p[AT(8)] = IPC_GPRS_IP_CONFIGURATION_FLAG_IPV4 |
		   IPC_GPRS_IP_CONFIGURATION_FLAG_IPV6;
	p[AT(9)] = IPC_GPRS_IP_CONFIGURATION_FLAG_MTU >> 8;
	memcpy(&p[AT(0x0a)], ip, 4);
	memcpy(&p[AT(0x1e)], pcscf1, 4);
	memcpy(&p[AT(0x22)], pcscf2, 4);
	p[AT(0x2a)] = 0x20;			/* IPv6 address */
	memcpy(&p[AT(0x3a)], pcscf6, 16);	/* first IPv6 P-CSCF */
	p[AT(0x9f)] = 0xdc;			/* MTU 1500 */
	p[AT(0xa0)] = 0x05;
	memcpy(&p[AT(0xa4)], pcscf1, 4);	/* fourth IPv4 P-CSCF */
	memcpy(&p[AT(0xb8)], pcscf6, 16);	/* fourth IPv6 P-CSCF */

	CHECK(ipc_gprs_ip_configuration_ext_parse(p, sizeof(p), &config) == 0);
	CHECK(config.cid == 1);
	CHECK(!memcmp(config.ipv4, ip, 4));
	CHECK(config.ipv6[0] == 0x20);
	CHECK(config.mtu == 1500);
	CHECK(config.pcscf_ipv4_count == 3);
	CHECK(!memcmp(config.pcscf_ipv4[0], pcscf1, 4));
	CHECK(!memcmp(config.pcscf_ipv4[1], pcscf2, 4));
	CHECK(!memcmp(config.pcscf_ipv4[2], pcscf1, 4));
	CHECK(config.pcscf_ipv6_count == 2);
	CHECK(!memcmp(config.pcscf_ipv6[1], pcscf6, 16));

	/* The short form only has the first three */
	CHECK(ipc_gprs_ip_configuration_ext_parse(p, AT(0xa4), &config) == 0);
	CHECK(config.pcscf_ipv4_count == 2);
	CHECK(config.pcscf_ipv6_count == 1);

	CHECK(ipc_gprs_ip_configuration_ext_parse(p, 0x20, &config) < 0);

	return 0;
}

int test_shannon_define_pdp_context(__attribute__((unused))
				    struct ipc_client *client)
{
	struct ipc_gprs_define_pdp_context_ext_data data;
	const unsigned char *b = (const unsigned char *) &data;
	char long_apn[IPC_GPRS_APN_MAX + 1];

	CHECK(sizeof(data) == 0x86 - 7);
	CHECK(sizeof(data) == sizeof(struct ipc_gprs_define_pdp_context_data));
	CHECK(ipc_gprs_define_pdp_context_ext_setup(
		&data, 3, IPC_GPRS_PDP_TYPE_IPV6, "ims",
		IPC_GPRS_PCSCF_REQUEST_IPV6) == 0);
	CHECK(b[AT(7)] == 1 && b[AT(8)] == 3 && b[AT(9)] == 5);
	CHECK(!strcmp((const char *) &b[AT(0x0a)], "ims"));
	CHECK(b[AT(0x6f)] == 0 && b[AT(0x7f)] == 0);
	CHECK(b[AT(0x85)] == IPC_GPRS_PCSCF_REQUEST_IPV6);

	memset(long_apn, 'a', sizeof(long_apn) - 1);
	long_apn[sizeof(long_apn) - 1] = '\0';
	CHECK(ipc_gprs_define_pdp_context_ext_setup(&data, 3, 5, long_apn,
						    0) < 0);

	return 0;
}

int test_shannon_net_regist(__attribute__((unused)) struct ipc_client *client)
{
	unsigned char p[0x19 - 7];
	struct ipc_net_regist_response_ext_data reg;

	memset(p, 0, sizeof(p));
	p[AT(7)] = 0x0a;		/* LTE */
	p[AT(9)] = 0x02;		/* home */
	p[AT(0x12)] = 0x34;
	p[AT(0x13)] = 0x12;
	p[AT(0x14)] = IPC_NET_LTE_SUPPORTED;
	p[AT(0x15)] = IPC_NET_LTE_NOT_SUPPORTED;
	p[AT(0x17)] = 0x2c;
	p[AT(0x18)] = 0x01;

	CHECK(ipc_net_regist_response_ext_extract(p, sizeof(p), &reg) == 0);
	CHECK(reg.act == 0x0a && reg.status == 0x02);
	CHECK(reg.tac == 0x1234);
	CHECK(reg.volte == IPC_NET_LTE_SUPPORTED);
	CHECK(reg.ims_emergency == IPC_NET_LTE_NOT_SUPPORTED);
	CHECK(reg.pci == 0x12c);

	/* The plain form: no LTE fields */
	CHECK(ipc_net_regist_response_ext_extract(
		p, sizeof(struct ipc_net_regist_response_data), &reg) == 0);
	CHECK(reg.volte == 0 && reg.tac == 0);
	CHECK(ipc_net_regist_response_ext_extract(p, 3, &reg) < 0);

	return 0;
}

int test_shannon_call_status(__attribute__((unused)) struct ipc_client *client)
{
	unsigned char p[9] = { 2, 1, 0, 0, 1, 0x10, 0x27, 0, 0 };
	struct ipc_gprs_call_status_ext_data status;

	CHECK(ipc_gprs_call_status_ext_extract(p, sizeof(p), &status) == 0);
	CHECK(status.cid == 2 && status.status == 1);
	CHECK(status.dedicated_bearer_changed == 1);
	CHECK(status.retry_time == 10000);

	/* Older modems send four bytes */
	CHECK(ipc_gprs_call_status_ext_extract(p, 4, &status) == 0);
	CHECK(status.dedicated_bearer_changed == 0 && status.retry_time == 0);

	return 0;
}

int test_shannon_set_apn_info(__attribute__((unused))
			      struct ipc_client *client)
{
	struct ipc_gprs_set_apn_info_data data;
	const unsigned char *b = (const unsigned char *) &data;

	CHECK(ipc_gprs_set_apn_info_setup(&data, IPC_GPRS_PROFILE_IMS,
					  "lte_ims", IPC_GPRS_PDP_TYPE_IPV6,
					  "ims", NULL, NULL,
					  IPC_GPRS_PROFILE_AUTH_NONE,
					  IPC_GPRS_PCSCF_REQUEST_IPV6) == 0);
	CHECK(b[AT(7)] == 1);
	CHECK(!strcmp((const char *) &b[AT(8)], "lte_ims"));
	CHECK(b[AT(0x1c)] == 5);
	CHECK(!strcmp((const char *) &b[AT(0x1d)], "ims"));
	CHECK(b[AT(0x82)] == 0 && b[AT(0xa2)] == 0);
	CHECK(b[AT(0xc2)] == 0 && b[AT(0xc3)] == 2 && b[AT(0xc4)] == 0);
	CHECK(ipc_gprs_set_apn_info_setup(&data, 0, "a_name_that_is_too_long",
					  3, "x", NULL, NULL, 0, 0) < 0);

	return 0;
}

int test_shannon_ims_channel(__attribute__((unused))
			     struct ipc_client *client)
{
	struct ipc_ims_channel_data data;
	const unsigned char *b = (const unsigned char *) &data;
	static const unsigned char local[4] = { 10, 0, 0, 1 };
	static const unsigned char remote[4] = { 10, 0, 0, 2 };

	CHECK(ipc_ims_channel_setup(&data, 0, IPC_IMS_CHANNEL_DELETE) == 0);
	CHECK(b[AT(7)] == 0 && b[AT(8)] == IPC_IMS_CHANNEL_DELETE);
	CHECK(b[AT(9)] == 0xff && b[AT(0x11)] == 0xff);

	CHECK(ipc_ims_channel_setup(&data, 1, IPC_IMS_CHANNEL_CREATE) == 0);
	CHECK(ipc_ims_channel_set_addresses(&data, IPC_IMS_ADDRESS_IPV4,
					    local, remote, 50000, 30000,
					    50001, 30001) == 0);
	CHECK(b[AT(9)] == (50001 & 0xff) && b[AT(0x0a)] == (50001 >> 8));
	CHECK(b[AT(0x0b)] == (30001 & 0xff));
	CHECK(b[AT(0x0d)] == (50000 & 0xff));
	CHECK(b[AT(0x0f)] == (30000 & 0xff));
	CHECK(b[AT(0x11)] == IPC_IMS_ADDRESS_IPV4);
	CHECK(!memcmp(&b[AT(0x12)], remote, 4));
	CHECK(b[AT(0x22)] == IPC_IMS_ADDRESS_IPV4);
	CHECK(!memcmp(&b[AT(0x23)], local, 4));

	return 0;
}

int test_shannon_ims_pdn(__attribute__((unused)) struct ipc_client *client)
{
	struct ipc_ims_pdn_data data;
	const unsigned char *b = (const unsigned char *) &data;
	static const unsigned char address[4] = { 10, 9, 8, 7 };

	CHECK(ipc_ims_pdn_setup(&data, IPC_IMS_ADDRESS_IPV4, address, "rmnet1",
				2) == 0);
	CHECK(b[AT(7)] == IPC_IMS_ADDRESS_IPV4);
	CHECK(!memcmp(&b[AT(8)], address, 4));
	CHECK(b[AT(0x18)] == 6);
	CHECK(!memcmp(&b[AT(0x1c)], "rmnet1", 6));
	CHECK(b[AT(0x2c)] == 2);

	return 0;
}

int test_shannon_dedicated_bearer(__attribute__((unused))
				  struct ipc_client *client)
{
	unsigned char p[0x1d - 7 + 2 * 26];

	memset(p, 0, sizeof(p));
	p[AT(7)] = 6;		/* bearer id */
	p[AT(8)] = 1;		/* QCI 1, conversational voice */
	p[AT(9)] = IPC_IMS_BEARER_ADDED;
	p[AT(0x1c)] = 2;	/* filter count */

	CHECK(ipc_ims_dedicated_bearer_filter_count(p, sizeof(p)) == 2);
	CHECK(ipc_ims_dedicated_bearer_filter_count(p, sizeof(p) - 1) == 1);
	CHECK(ipc_ims_dedicated_bearer_filter_count(p, AT(0x1a)) == 0);
	CHECK(((const struct ipc_ims_dedicated_bearer_header *) p)->qci == 1);

	return 0;
}
