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
 * Malformed input: messages shorter or longer than their own length fields
 * say, short reads, the end of a file, NV data requests outside the file.
 * Meant to run under AddressSanitizer, which also reports leaks.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <samsung-ipc.h>

#include <ipc.h>

#include "robustness.h"

#define CHECK(condition)						\
	do {								\
		if (!(condition)) {					\
			printf("[ !! ] %s:%d: %s\n", __func__, __LINE__,	\
			       #condition);				\
			return -1;					\
		}							\
	} while (0)

/*
 * Message helpers
 */

int test_helpers_message_bounds(__attribute__((unused)) struct ipc_client *client)
{
	/* Two calls; the second entry's number runs past the message */
	unsigned char list[1 + 2 * sizeof(struct ipc_call_list_entry) + 3];
	struct ipc_call_list_entry *entry;
	unsigned char imsi[] = { 15, '2', '3', '0' };
	unsigned char sms[sizeof(struct ipc_sms_incoming_msg_header)];
	char *string;
	size_t size;

	memset(list, 0, sizeof(list));
	list[0] = 2;
	entry = (struct ipc_call_list_entry *) (list + 1);
	entry->number_length = 3;
	memcpy(list + 1 + sizeof(*entry), "123", 3);
	entry = (struct ipc_call_list_entry *) (list + 1 + sizeof(*entry) + 3);
	entry->number_length = 200;

	CHECK(ipc_call_list_count_extract(list, sizeof(list)) == 2);
	entry = ipc_call_list_entry_extract(list, sizeof(list), 0);
	CHECK(entry != NULL);
	string = ipc_call_list_entry_number_extract(entry);
	CHECK(string != NULL && strcmp(string, "123") == 0);
	free(string);
	CHECK(ipc_call_list_entry_extract(list, sizeof(list), 1) == NULL);
	/* A count without the entries */
	CHECK(ipc_call_list_entry_extract(list, 1, 0) == NULL);
	CHECK(ipc_call_list_entry_extract(list, 3, 0) == NULL);

	/* The IMSI's length byte promises more than the message has */
	CHECK(ipc_misc_me_imsi_imsi_extract(imsi, sizeof(imsi)) == NULL);
	imsi[0] = 3;
	string = ipc_misc_me_imsi_imsi_extract(imsi, sizeof(imsi));
	CHECK(string != NULL && strcmp(string, "230") == 0);
	free(string);
	CHECK(ipc_misc_me_imsi_imsi_extract(imsi, 0) == NULL);

	memset(sms, 0xff, sizeof(sms));
	CHECK(ipc_sms_incoming_msg_pdu_size_extract(NULL, 0) == 0);
	CHECK(ipc_sms_incoming_msg_pdu_size_extract(sms, 1) == 0);
	CHECK(ipc_sms_incoming_msg_pdu_size_extract(sms, sizeof(sms)) == 0);

	/* An SMS that does not fit the 8 bit length is refused */
	{
		struct ipc_sms_send_msg_request_header header;
		unsigned char smsc[8] = { 0 };
		unsigned char pdu[250] = { 0 };

		memset(&header, 0, sizeof(header));
		CHECK(ipc_sms_send_msg_setup(&header, smsc, sizeof(smsc), pdu,
					     sizeof(pdu)) == NULL);
		size = ipc_sms_send_msg_size_setup(&header, smsc, sizeof(smsc),
						   pdu, 100);
		CHECK(size == sizeof(header) + 1 + sizeof(smsc) + 100);
	}

	return 0;
}

/*
 * Message framing, on a transport that plays back prepared reads
 */

#define FAKE_READS	4

struct fake_transport {
	const unsigned char *chunks[FAKE_READS];
	size_t sizes[FAKE_READS];
	unsigned int count;
	unsigned int next;
	unsigned char *written;
	size_t written_size;
	size_t write_limit;
};

static int fake_read(__attribute__((unused)) struct ipc_client *client,
		     void *data, void *buffer, size_t length)
{
	struct fake_transport *transport = data;
	size_t size;

	if (transport->next >= transport->count)
		return 0;

	size = transport->sizes[transport->next];
	if (size > length)
		size = length;

	memcpy(buffer, transport->chunks[transport->next], size);
	transport->next++;

	return (int) size;
}

static int fake_write(__attribute__((unused)) struct ipc_client *client,
		      void *data, const void *buffer, size_t length)
{
	struct fake_transport *transport = data;
	unsigned char *written;

	/* Take the data in pieces, as a transport may */
	if (transport->write_limit && length > transport->write_limit)
		length = transport->write_limit;

	written = realloc(transport->written,
			  transport->written_size + length);
	if (written == NULL)
		return -1;

	memcpy(written + transport->written_size, buffer, length);
	transport->written = written;
	transport->written_size += length;

	return (int) length;
}

static struct ipc_client *fake_client(int type,
				      struct fake_transport *transport)
{
	struct ipc_client *client;

	client = ipc_client_create(type);
	if (client == NULL)
		return NULL;

	if (ipc_client_transport_handlers_register(client, NULL, NULL,
						   fake_read, fake_write, NULL,
						   transport) < 0) {
		ipc_client_destroy(client);
		return NULL;
	}

	return client;
}

static int recv_one(int type, const unsigned char *first, size_t first_size,
		    const unsigned char *second, size_t second_size,
		    struct ipc_message *message)
{
	struct fake_transport transport;
	struct ipc_client *client;
	int rc;

	memset(&transport, 0, sizeof(transport));
	transport.chunks[0] = first;
	transport.sizes[0] = first_size;
	transport.count = 1;
	if (second != NULL) {
		transport.chunks[1] = second;
		transport.sizes[1] = second_size;
		transport.count = 2;
	}

	client = fake_client(type, &transport);
	if (client == NULL)
		return -2;

	memset(message, 0, sizeof(*message));
	rc = ipc_client_recv(client, message);

	ipc_client_destroy(client);

	return rc;
}

int test_frame_recv(__attribute__((unused)) struct ipc_client *client)
{
	static unsigned char big[5000];
	unsigned char frame[32];
	struct ipc_fmt_header *fmt = (struct ipc_fmt_header *) frame;
	struct ipc_rfs_header *rfs = (struct ipc_rfs_header *) frame;
	struct ipc_fmt_header *big_fmt = (struct ipc_fmt_header *) big;
	struct ipc_message message;
	size_t i;

	/* A whole FMT message in one read */
	memset(frame, 0, sizeof(frame));
	fmt->length = sizeof(*fmt) + 2;
	fmt->mseq = 0x11;
	fmt->aseq = 0x22;
	fmt->group = 0x05;
	fmt->index = 0x01;
	fmt->type = 0x02;
	frame[sizeof(*fmt)] = 0xaa;
	frame[sizeof(*fmt) + 1] = 0xbb;
	CHECK(recv_one(IPC_CLIENT_TYPE_FMT, frame, fmt->length, NULL, 0,
		       &message) == 0);
	CHECK(message.size == 2 && message.mseq == 0x11 &&
	      message.aseq == 0x22 && message.command == 0x0501 &&
	      message.type == 0x02);
	CHECK(((unsigned char *) message.data)[1] == 0xbb);
	free(message.data);

	/* More bytes in the read than the header's length: only the message */
	fmt->length = sizeof(*fmt) + 1;
	CHECK(recv_one(IPC_CLIENT_TYPE_FMT, frame, sizeof(frame), NULL, 0,
		       &message) == 0);
	CHECK(message.size == 1 &&
	      ((unsigned char *) message.data)[0] == 0xaa);
	free(message.data);

	/* No data at all */
	fmt->length = sizeof(*fmt);
	CHECK(recv_one(IPC_CLIENT_TYPE_FMT, frame, sizeof(*fmt), NULL, 0,
		       &message) == 0);
	CHECK(message.size == 0 && message.data == NULL);

	/* A length shorter than the header, and a read shorter than it */
	fmt->length = 3;
	CHECK(recv_one(IPC_CLIENT_TYPE_FMT, frame, sizeof(frame), NULL, 0,
		       &message) < 0);
	CHECK(message.data == NULL);
	fmt->length = sizeof(*fmt) + 2;
	CHECK(recv_one(IPC_CLIENT_TYPE_FMT, frame, 4, NULL, 0, &message) < 0);

	/* A message longer than one read arrives in two */
	for (i = 0; i < sizeof(big); i++)
		big[i] = (unsigned char) i;
	big_fmt->length = sizeof(big);
	CHECK(recv_one(IPC_CLIENT_TYPE_FMT, big, 4096, big + 4096,
		       sizeof(big) - 4096, &message) == 0);
	CHECK(message.size == sizeof(big) - sizeof(*big_fmt));
	CHECK(memcmp(message.data, big + sizeof(*big_fmt), message.size) == 0);
	free(message.data);

	/* ... and fails when the rest never comes */
	CHECK(recv_one(IPC_CLIENT_TYPE_FMT, big, 4096, NULL, 0,
		       &message) < 0);
	CHECK(message.data == NULL);

	/* RFS: a message, one beyond the size limit, one with extra bytes */
	memset(frame, 0, sizeof(frame));
	rfs->length = sizeof(*rfs) + 3;
	rfs->index = 0x02;
	rfs->id = 0x33;
	frame[sizeof(*rfs)] = 0xcc;
	CHECK(recv_one(IPC_CLIENT_TYPE_RFS, frame, rfs->length, NULL, 0,
		       &message) == 0);
	CHECK(message.size == 3 && message.aseq == 0x33 &&
	      message.command == IPC_COMMAND(IPC_GROUP_RFS, 0x02));
	free(message.data);

	rfs->length = 0x80001;
	CHECK(recv_one(IPC_CLIENT_TYPE_RFS, frame, sizeof(frame), NULL, 0,
		       &message) < 0);
	rfs->length = 0xffffffff;
	CHECK(recv_one(IPC_CLIENT_TYPE_RFS, frame, sizeof(frame), NULL, 0,
		       &message) < 0);
	rfs->length = sizeof(*rfs) + 1;
	CHECK(recv_one(IPC_CLIENT_TYPE_RFS, frame, sizeof(frame), NULL, 0,
		       &message) == 0);
	CHECK(message.size == 1);
	free(message.data);

	return 0;
}

int test_frame_send(__attribute__((unused)) struct ipc_client *client)
{
	static unsigned char data[0x10000];
	struct fake_transport transport;
	struct ipc_client *fmt_client;
	struct ipc_fmt_header *header;
	size_t max = UINT16_MAX - sizeof(struct ipc_fmt_header);

	memset(&transport, 0, sizeof(transport));
	transport.write_limit = 1000;
	memset(data, 0x5a, sizeof(data));

	fmt_client = fake_client(IPC_CLIENT_TYPE_FMT, &transport);
	CHECK(fmt_client != NULL);

	/* The largest message the 16 bit length allows, written in pieces */
	CHECK(ipc_client_send(fmt_client, 1, 0x0501, 2, data, max) == 0);
	CHECK(transport.written_size == UINT16_MAX);
	header = (struct ipc_fmt_header *) transport.written;
	CHECK(header->length == UINT16_MAX && header->group == 0x05 &&
	      header->index == 0x01);
	CHECK(transport.written[UINT16_MAX - 1] == 0x5a);

	/* One byte more would wrap the length */
	transport.written_size = 0;
	CHECK(ipc_client_send(fmt_client, 1, 0x0501, 2, data, max + 1) < 0);
	CHECK(transport.written_size == 0);

	/* A size without data */
	CHECK(ipc_client_send(fmt_client, 1, 0x0501, 2, NULL, 4) < 0);

	free(transport.written);
	ipc_client_destroy(fmt_client);

	return 0;
}

/*
 * File and descriptor helpers
 */

int test_data_read_write(__attribute__((unused)) struct ipc_client *client)
{
	unsigned char buffer[16];
	int fds[2];

	/* Two short reads make one whole one, in order */
	CHECK(pipe(fds) == 0);
	CHECK(write(fds[1], "abcd", 4) == 4);
	CHECK(write(fds[1], "efgh", 4) == 4);
	memset(buffer, 0, sizeof(buffer));
	CHECK(data_read(NULL, fds[0], buffer, 6) == 6);
	CHECK(memcmp(buffer, "abcdef", 6) == 0);

	/* The end of the data is an error, not a loop */
	close(fds[1]);
	errno = 0;
	CHECK(data_read(NULL, fds[0], buffer, 8) == -1);
	CHECK(errno == ENODATA);
	close(fds[0]);

	CHECK(pipe(fds) == 0);
	CHECK(data_write(NULL, fds[1], "0123456789", 10) == 10);
	CHECK(read(fds[0], buffer, sizeof(buffer)) == 10);
	CHECK(memcmp(buffer, "0123456789", 10) == 0);
	close(fds[0]);
	close(fds[1]);

	CHECK(data_read(NULL, -1, buffer, 1) == -1);
	CHECK(data_read(NULL, 0, NULL, 1) == -1);

	return 0;
}

int test_fds_poll(__attribute__((unused)) struct ipc_client *client)
{
	struct timeval timeout = { 0, 20000 };
	struct ipc_poll_fds poll_fds;
	int main_pipe[2];
	int other_pipe[2];
	int list[3];
	int ready;

	CHECK(pipe(main_pipe) == 0 && pipe(other_pipe) == 0);

	/* Nothing to read: a timeout, and every entry marked not ready */
	list[0] = other_pipe[0];
	list[1] = -1;
	list[2] = other_pipe[0];
	poll_fds.fds = list;
	poll_fds.count = 3;
	ready = 1;
	CHECK(ipc_fds_poll(main_pipe[0], &poll_fds, &timeout, &ready) == 0);
	CHECK(!ready && poll_fds.count == 0 && list[0] == -1);

	/* The main descriptor only */
	CHECK(write(main_pipe[1], "x", 1) == 1);
	CHECK(ipc_fds_poll(main_pipe[0], NULL, &timeout, &ready) == 1);
	CHECK(ready);

	/* Both */
	list[0] = other_pipe[0];
	list[1] = -1;
	poll_fds.count = 2;
	CHECK(write(other_pipe[1], "y", 1) == 1);
	CHECK(ipc_fds_poll(main_pipe[0], &poll_fds, &timeout, &ready) == 2);
	CHECK(ready && poll_fds.count == 1 && list[0] == other_pipe[0] &&
	      list[1] == -1);

	/* A closed peer counts as ready: the read then reports it */
	close(main_pipe[1]);
	CHECK(read(main_pipe[0], list, 1) == 1);
	CHECK(ipc_fds_poll(main_pipe[0], NULL, &timeout, &ready) == 1);
	CHECK(ready);

	CHECK(ipc_fds_poll(-1, NULL, &timeout, &ready) == -1);

	close(main_pipe[0]);
	close(other_pipe[0]);
	close(other_pipe[1]);

	return 0;
}

/*
 * NV data, in a temporary directory
 */

#define NV_SIZE		0x1000
#define NV_CHUNK_SIZE	0x400

static off_t file_size(const char *path)
{
	struct stat st;

	if (stat(path, &st) < 0)
		return -1;

	return st.st_size;
}

int test_nv_data(__attribute__((unused)) struct ipc_client *client)
{
	char directory[] = "/tmp/libsamsung-ipc-nv-XXXXXX";
	char path[64], md5_path[64], backup_path[64], backup_md5_path[64];
	char tmp_path[80];
	struct ipc_client_nv_data_specs specs;
	struct ipc_client *rfs_client;
	unsigned char *nv_data;
	unsigned char value[4] = { 1, 2, 3, 4 };
	unsigned char *data;
	char *md5;
	int rc = -1;

	if (mkdtemp(directory) == NULL)
		return -1;

	snprintf(path, sizeof(path), "%s/nv_data.bin", directory);
	snprintf(md5_path, sizeof(md5_path), "%s/nv_data.bin.md5", directory);
	snprintf(backup_path, sizeof(backup_path), "%s/.nv_data.bak",
		 directory);
	snprintf(backup_md5_path, sizeof(backup_md5_path),
		 "%s/.nv_data.bak.md5", directory);
	snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", md5_path);

	memset(&specs, 0, sizeof(specs));
	specs.nv_data_path = path;
	specs.nv_data_md5_path = md5_path;
	specs.nv_data_backup_path = backup_path;
	specs.nv_data_backup_md5_path = backup_md5_path;
	specs.nv_data_secret = "Samsung_Android_RIL";
	specs.nv_data_size = NV_SIZE;
	specs.nv_data_chunk_size = NV_CHUNK_SIZE;

	rfs_client = ipc_client_create(IPC_CLIENT_TYPE_RFS);
	if (rfs_client == NULL)
		goto cleanup;
	rfs_client->nv_data_specs = &specs;

	nv_data = calloc(1, NV_SIZE);
	if (nv_data == NULL)
		goto cleanup;
	rc = file_data_write(rfs_client, path, nv_data, NV_SIZE, NV_CHUNK_SIZE,
			     0);
	free(nv_data);
	if (rc < 0)
		goto cleanup;
	rc = -1;

	md5 = ipc_nv_data_md5_calculate(rfs_client, path, specs.nv_data_secret,
					NV_SIZE, NV_CHUNK_SIZE);
	if (md5 == NULL)
		goto cleanup;
	file_data_write(rfs_client, md5_path, md5, strlen(md5), strlen(md5), 0);
	free(md5);

#define NV_CHECK(condition)						\
	do {								\
		if (!(condition)) {					\
			printf("[ !! ] %s:%d: %s\n", __func__, __LINE__,	\
			       #condition);				\
			goto cleanup;					\
		}							\
	} while (0)

	NV_CHECK(ipc_nv_data_check(rfs_client) == 0);

	/* A write inside the file: the data and a matching md5 */
	NV_CHECK(ipc_nv_data_write(rfs_client, value, sizeof(value), 0x10) ==
		 0);
	NV_CHECK(ipc_nv_data_check(rfs_client) == 0);
	NV_CHECK(file_size(md5_path) == 32 && file_size(tmp_path) == -1);
	data = ipc_nv_data_read(rfs_client, sizeof(value), 0x10);
	NV_CHECK(data != NULL && memcmp(data, value, sizeof(value)) == 0);
	free(data);

	/* The last bytes of the file */
	NV_CHECK(ipc_nv_data_write(rfs_client, value, sizeof(value),
				   NV_SIZE - sizeof(value)) == 0);

	/* Past the end, at the end, and an offset that would wrap */
	NV_CHECK(ipc_nv_data_write(rfs_client, value, sizeof(value),
				   NV_SIZE - 2) < 0);
	NV_CHECK(ipc_nv_data_write(rfs_client, value, sizeof(value),
				   NV_SIZE) < 0);
	NV_CHECK(ipc_nv_data_write(rfs_client, value, sizeof(value),
				   0xffffffff) < 0);
	NV_CHECK(ipc_nv_data_write(rfs_client, value, 0, 0) < 0);
	NV_CHECK(file_size(path) == NV_SIZE);
	NV_CHECK(ipc_nv_data_check(rfs_client) == 0);

	NV_CHECK(ipc_nv_data_read(rfs_client, sizeof(value), NV_SIZE - 2) ==
		 NULL);
	NV_CHECK(ipc_nv_data_read(rfs_client, (size_t) 1 << 40, 0) == NULL);
	NV_CHECK(ipc_nv_data_read(rfs_client, 0, 0) == NULL);

	rc = 0;

cleanup:
	if (rfs_client != NULL)
		ipc_client_destroy(rfs_client);
	unlink(path);
	unlink(md5_path);
	unlink(backup_path);
	unlink(backup_md5_path);
	unlink(tmp_path);
	rmdir(directory);

	return rc;
}
