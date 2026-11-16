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
 * Device matching, the firmware image's TOC and packet data interface
 * names.
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

static int index_of(const char *name)
{
	unsigned int i;

	for (i = 0; i < ipc_devices_count; i++)
		if (strcmp(ipc_devices[i].name, name) == 0)
			return (int) i;

	return -2;
}

int test_device_match(__attribute__((unused)) struct ipc_client *client)
{
	/* As in /proc/device-tree/compatible: NUL separated, NUL at the end */
	static const char a3[] = "samsung,a3y17lte\0samsung,exynos7870";
	static const char other[] = "samsung,j5y17lte\0samsung,exynos7870";
	static const char cut[] = "samsung,a3y17l";
	int rc = -1;

	CHECK(ipc_device_match("a3y17lte", NULL, 0) == index_of("a3y17lte"));
	CHECK(ipc_device_match("herolte", NULL, 0) == index_of("herolte"));
	/* A name given is final, even with a matching device tree */
	CHECK(ipc_device_match("i9300", a3, sizeof(a3)) == -1);

	CHECK(ipc_device_match(NULL, a3, sizeof(a3)) == index_of("a3y17lte"));
	/* Without the trailing NUL (a short read) */
	CHECK(ipc_device_match(NULL, a3, sizeof(a3) - 1) ==
	      index_of("a3y17lte"));
	/* The SoC alone says nothing about the phone */
	CHECK(ipc_device_match(NULL, other, sizeof(other)) == -1);
	CHECK(ipc_device_match(NULL, cut, sizeof(cut) - 1) == -1);
	CHECK(ipc_device_match(NULL, NULL, 0) == -1);

	rc = 0;
out:
	return rc;
}

static struct ipc_client *device_client(const char *name, int type)
{
	struct ipc_client *client;

	setenv("LIBSAMSUNG_IPC_DEVICE", name, 1);
	client = ipc_client_create(type);
	setenv("LIBSAMSUNG_IPC_DEVICE", "a3y17lte", 1);

	return client;
}

int test_device_gprs(__attribute__((unused)) struct ipc_client *client)
{
	struct ipc_client_gprs_capabilities capabilities;
	struct ipc_client *a3 = NULL, *hero = NULL;
	char *iface = NULL;
	int rc = -1;

	a3 = device_client("a3y17lte", IPC_CLIENT_TYPE_FMT);
	hero = device_client("herolte", IPC_CLIENT_TYPE_FMT);
	CHECK(a3 != NULL && hero != NULL);
	CHECK(device_client("nonexistent", IPC_CLIENT_TYPE_FMT) == NULL);

	CHECK(ipc_client_gprs_get_capabilities(a3, &capabilities) == 0 &&
	      capabilities.cid_count == 15);
	iface = ipc_client_gprs_get_iface(a3, 1);
	CHECK(iface != NULL && strcmp(iface, "rmnet0") == 0);
	free(iface);
	iface = ipc_client_gprs_get_iface(a3, 15);
	CHECK(iface != NULL && strcmp(iface, "rmnet14") == 0);
	free(iface);
	iface = NULL;
	CHECK(ipc_client_gprs_get_iface(a3, 0) == NULL);
	CHECK(ipc_client_gprs_get_iface(a3, 16) == NULL);
	CHECK(ipc_client_gprs_get_iface(hero, 4) == NULL);

	CHECK(ipc_client_nv_data_size(a3) == 0x100000);
	CHECK(ipc_client_nv_data_size(hero) == 0x200000);

	rc = 0;
out:
	free(iface);
	if (a3 != NULL)
		ipc_client_destroy(a3);
	if (hero != NULL)
		ipc_client_destroy(hero);
	return rc;
}

/* A firmware image with the A3's TOC (from its RADIO partition) */
int test_write_toc(const char *path, uint32_t nv_size, int with_toc)
{
	struct firmware_toc_entry toc[N_TOC_ENTRIES];
	FILE *file;
	size_t n;

	memset(toc, 0, sizeof(toc));
	strcpy(toc[0].name, with_toc ? "TOC" : "XYZ");
	toc[0].size = 0x200;
	toc[0].entryid = 1;
	strcpy(toc[1].name, "BOOT");
	toc[1].offset = 0x200;
	toc[1].loadaddr = 0x40000000;
	toc[1].size = 0x170c;
	strcpy(toc[2].name, "MAIN");
	toc[2].offset = 0x1920;
	toc[2].loadaddr = 0x40010000;
	toc[2].size = 0x2479508;
	toc[2].entryid = 2;
	strcpy(toc[3].name, "NV");
	toc[3].loadaddr = 0x44900000;
	toc[3].size = nv_size;
	toc[3].entryid = 3;

	file = fopen(path, "w");
	if (file == NULL)
		return -1;
	n = fwrite(toc, sizeof(toc), 1, file);
	fclose(file);

	return n == 1 ? 0 : -1;
}

int test_firmware_toc(struct ipc_client *client)
{
	char path[] = "/tmp/libsamsung-ipc-toc-XXXXXX";
	const char *partitions[] = { "/nonexistent/RADIO", path, NULL };
	const char *none[] = { "/nonexistent/RADIO", NULL };
	int fd;
	int rc = -1;

	fd = mkstemp(path);
	if (fd < 0)
		return -1;
	close(fd);

	CHECK(test_write_toc(path, 0x100000, 1) == 0);
	CHECK(toc_nv_size(client, partitions) == 0x100000);
	CHECK(toc_nv_size(client, none) == 0);

	/* Not a firmware image */
	CHECK(test_write_toc(path, 0x100000, 0) == 0);
	CHECK(toc_nv_size(client, partitions) == 0);

	/* Shorter than a TOC */
	CHECK(truncate(path, 100) == 0);
	CHECK(toc_nv_size(client, partitions) == 0);

	rc = 0;
out:
	unlink(path);
	return rc;
}
