/*
 * This file is part of libsamsung-ipc.
 *
 * Copyright (C) 2020 Tony Garnock-Jones <tonyg@leastfixedpoint.com>
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

#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

#include <samsung-ipc.h>

#include "ipc.h"
#include "partitions/android/android.h"

#include "toc.h"

struct firmware_toc_entry const *find_toc_entry(
	char const *name,
	struct firmware_toc_entry const *toc)
{
	unsigned int index;

	/* We don't know all the details of the TOC format yet; for now, we
	 * assume two things:
	 * 1. reading 512 bytes of TOC is enough, and
	 * 2. the first entry with an empty name field ends the list.
	 */
	for (index = 0; index < N_TOC_ENTRIES; index++) {
		if (toc[index].name[0] == '\0')
			break;
		if (strncmp(toc[index].name, name,
			    sizeof(toc[index].name)) == 0)
			return &toc[index];
	}
	return NULL;
}

int toc_open(struct ipc_client *client, const char *const *partitions,
	     struct firmware_toc_entry *toc)
{
	int fd;

	if (partitions == NULL) {
		errno = ENOENT;
		return -1;
	}

	fd = open_android_modem_partition(client, partitions);
	if (fd < 0) {
		ipc_client_log(client, "%s: no firmware partition: %s",
			       __func__, strerror(errno));
		return -1;
	}

	if (data_read(client, fd, toc, N_TOC_ENTRIES * sizeof(*toc)) !=
	    (ssize_t) (N_TOC_ENTRIES * sizeof(*toc))) {
		ipc_client_log(client, "%s: reading the TOC failed: %s",
			       __func__, strerror(errno));
		close(fd);
		return -1;
	}

	if (strncmp(toc[0].name, "TOC", sizeof(toc[0].name)) != 0) {
		ipc_client_log(client, "%s: no TOC at the start of the image",
			       __func__);
		close(fd);
		errno = EINVAL;
		return -1;
	}

	return fd;
}

uint32_t toc_nv_size(struct ipc_client *client,
		     const char *const *partitions)
{
	struct firmware_toc_entry toc[N_TOC_ENTRIES];
	const struct firmware_toc_entry *nv;
	int fd;

	fd = toc_open(client, partitions, toc);
	if (fd < 0)
		return 0;

	close(fd);

	nv = find_toc_entry("NV", toc);

	return nv != NULL ? nv->size : 0;
}
