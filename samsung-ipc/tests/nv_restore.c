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
 * When NV data is (and is not) restored from its backup, and how backups
 * are made.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <samsung-ipc.h>

#include <ipc.h>
#include "devices/ipc_devices.h"
#include "partitions/toc/toc.h"

#include "devices.h"

#define CHECK(condition)						\
	do {								\
		if (!(condition)) {					\
			printf("[ !! ] %s:%d: %s\n", __func__, __LINE__,	\
			       #condition);				\
			goto out;					\
		}							\
	} while (0)

/*
 * NV data in a temporary directory: the client's specs point there, and
 * its firmware image is a TOC in the same directory
 */

#define NV_SIZE		0x2000
#define NV_CHUNK_SIZE	0x400

struct nv_setup {
	char directory[64];
	char path[96], md5_path[96], backup_path[96], backup_md5_path[96];
	char bad_path[96], toc_path[96];
	const char *partitions[2];
	struct ipc_client_nv_data_specs specs;
	struct ipc_device_desc device;
	struct ipc_client *client;
};

static int nv_write(struct nv_setup *nv, const char *path,
		    const char *md5_path, unsigned char fill, size_t size)
{
	unsigned char *data;
	char *md5;
	int rc;

	data = malloc(size);
	if (data == NULL)
		return -1;
	memset(data, fill, size);
	unlink(path);
	rc = file_data_write(nv->client, path, data, size, NV_CHUNK_SIZE, 0);
	free(data);
	if (rc < 0 || md5_path == NULL)
		return rc;

	md5 = ipc_nv_data_md5_calculate(nv->client, path,
					nv->specs.nv_data_secret, size,
					NV_CHUNK_SIZE);
	if (md5 == NULL)
		return -1;
	unlink(md5_path);
	rc = file_data_write(nv->client, md5_path, md5, strlen(md5),
			     strlen(md5), 0);
	free(md5);

	return rc;
}

static int nv_first_byte(const char *path)
{
	unsigned char byte;
	FILE *file = fopen(path, "r");
	int rc = -1;

	if (file == NULL)
		return -1;
	if (fread(&byte, 1, 1, file) == 1)
		rc = byte;
	fclose(file);

	return rc;
}

static int nv_setup(struct nv_setup *nv)
{
	memset(nv, 0, sizeof(*nv));
	strcpy(nv->directory, "/tmp/libsamsung-ipc-nvr-XXXXXX");
	if (mkdtemp(nv->directory) == NULL)
		return -1;

	snprintf(nv->path, sizeof(nv->path), "%s/nv_data.bin", nv->directory);
	snprintf(nv->md5_path, sizeof(nv->md5_path), "%s/nv_data.bin.md5",
		 nv->directory);
	snprintf(nv->backup_path, sizeof(nv->backup_path), "%s/.nv_data.bak",
		 nv->directory);
	snprintf(nv->backup_md5_path, sizeof(nv->backup_md5_path),
		 "%s/.nv_data.bak.md5", nv->directory);
	snprintf(nv->bad_path, sizeof(nv->bad_path), "%s.bad", nv->path);
	snprintf(nv->toc_path, sizeof(nv->toc_path), "%s/RADIO",
		 nv->directory);

	nv->specs.nv_data_path = nv->path;
	nv->specs.nv_data_md5_path = nv->md5_path;
	nv->specs.nv_data_backup_path = nv->backup_path;
	nv->specs.nv_data_backup_md5_path = nv->backup_md5_path;
	nv->specs.nv_data_secret = "Samsung_Android_RIL";
	nv->specs.nv_data_size = NV_SIZE;
	nv->specs.nv_data_chunk_size = NV_CHUNK_SIZE;

	nv->partitions[0] = nv->toc_path;
	nv->partitions[1] = NULL;
	nv->device.name = "test";
	nv->device.firmware_partitions = nv->partitions;
	nv->device.nv_data_specs = nv->specs;

	nv->client = ipc_client_create(IPC_CLIENT_TYPE_RFS);
	if (nv->client == NULL)
		return -1;
	nv->client->nv_data_specs = &nv->specs;
	nv->client->device = &nv->device;

	return test_write_toc(nv->toc_path, NV_SIZE, 1);
}

static void nv_cleanup(struct nv_setup *nv)
{
	char tmp[128];
	const char *files[] = { nv->path, nv->md5_path, nv->backup_path,
				nv->backup_md5_path, nv->bad_path,
				nv->toc_path };
	unsigned int i;

	for (i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
		unlink(files[i]);
		snprintf(tmp, sizeof(tmp), "%s.tmp", files[i]);
		unlink(tmp);
	}
	rmdir(nv->directory);

	if (nv->client != NULL)
		ipc_client_destroy(nv->client);
}

int test_nv_data_restore(__attribute__((unused)) struct ipc_client *client)
{
	struct nv_setup nv;
	void *data = NULL;
	struct stat st;
	int rc = -1;

	CHECK(nv_setup(&nv) == 0);

	/* Good NV data without a backup: loaded, and a backup is made */
	CHECK(nv_write(&nv, nv.path, nv.md5_path, 0x11, NV_SIZE) == 0);
	data = ipc_nv_data_load(nv.client);
	CHECK(data != NULL && ((unsigned char *) data)[0] == 0x11);
	free(data);
	data = NULL;
	CHECK(ipc_nv_data_backup_check(nv.client) == 0);
	CHECK(nv_first_byte(nv.backup_path) == 0x11);

	/* Damaged NV data, good backup: restored, the damaged file kept */
	CHECK(nv_write(&nv, nv.path, NULL, 0x22, NV_SIZE) == 0);
	data = ipc_nv_data_load(nv.client);
	CHECK(data != NULL && ((unsigned char *) data)[0] == 0x11);
	free(data);
	data = NULL;
	CHECK(nv_first_byte(nv.path) == 0x11);
	CHECK(nv_first_byte(nv.bad_path) == 0x22);
	CHECK(ipc_nv_data_check(nv.client) == 0);

	/* Damaged NV data, backup that fails its check: nothing replaced */
	unlink(nv.bad_path);
	CHECK(nv_write(&nv, nv.path, NULL, 0x33, NV_SIZE) == 0);
	CHECK(nv_write(&nv, nv.backup_path, NULL, 0x44, NV_SIZE) == 0);
	CHECK(ipc_nv_data_load(nv.client) == NULL);
	CHECK(nv_first_byte(nv.path) == 0x33);
	CHECK(nv_first_byte(nv.backup_path) == 0x44);
	CHECK(stat(nv.bad_path, &st) < 0);

	/* NV data of another size: a wrong device, not damage. Not restored */
	CHECK(nv_write(&nv, nv.backup_path, nv.backup_md5_path, 0x55,
		       NV_SIZE) == 0);
	CHECK(nv_write(&nv, nv.path, nv.md5_path, 0x66, NV_SIZE * 2) == 0);
	CHECK(ipc_nv_data_load(nv.client) == NULL);
	CHECK(stat(nv.path, &st) == 0 && st.st_size == NV_SIZE * 2);
	CHECK(nv_first_byte(nv.path) == 0x66);

	/* A firmware built for another NV size: nothing is touched */
	CHECK(nv_write(&nv, nv.path, NULL, 0x77, NV_SIZE) == 0);
	CHECK(test_write_toc(nv.toc_path, NV_SIZE * 2, 1) == 0);
	CHECK(ipc_nv_data_load(nv.client) == NULL);
	CHECK(nv_first_byte(nv.path) == 0x77);

	/* Without a readable firmware image the TOC check is skipped */
	unlink(nv.toc_path);
	data = ipc_nv_data_load(nv.client);
	CHECK(data != NULL && ((unsigned char *) data)[0] == 0x55);
	free(data);
	data = NULL;

	/* A backup is only made from NV data that passes its check */
	CHECK(nv_write(&nv, nv.path, NULL, 0x88, NV_SIZE) == 0);
	CHECK(ipc_nv_data_backup(nv.client) < 0);
	CHECK(nv_first_byte(nv.backup_path) == 0x55);

	rc = 0;
out:
	free(data);
	nv_cleanup(&nv);
	return rc;
}
