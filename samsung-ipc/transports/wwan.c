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

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <samsung-ipc.h>

#include "ipc.h"
#include "transports/wwan.h"

/* The name of the port of a type (as in /sys/class/wwan), or -1 */
static int wwan_find_port(const char *type, char *name, size_t size)
{
	struct dirent *entry;
	char path[PATH_MAX];
	char value[32];
	DIR *dir;
	int rc = -1;

	dir = opendir(WWAN_PORT_CLASS);
	if (dir == NULL)
		return -1;

	while ((entry = readdir(dir)) != NULL) {
		ssize_t length;
		int fd;

		if (entry->d_name[0] == '.')
			continue;

		snprintf(path, sizeof(path), WWAN_PORT_CLASS "/%s/type",
			 entry->d_name);
		fd = open(path, O_RDONLY);
		if (fd < 0)
			continue;

		length = read(fd, value, sizeof(value) - 1);
		close(fd);
		if (length <= 0)
			continue;

		value[length] = '\0';
		value[strcspn(value, "\n")] = '\0';
		if (strcmp(value, type) == 0 && strlen(entry->d_name) < size) {
			strcpy(name, entry->d_name);
			rc = 0;
			break;
		}
	}

	closedir(dir);

	return rc;
}

int wwan_available(void)
{
	char name[64];

	return wwan_find_port(WWAN_PORT_TYPE_IPC, name, sizeof(name)) == 0;
}

int wwan_open(struct ipc_client *client, int type)
{
	char path[PATH_MAX];
	char name[64];
	const char *port_type;
	int fd;

	switch (type) {
	case IPC_CLIENT_TYPE_FMT:
		port_type = WWAN_PORT_TYPE_IPC;
		break;
	case IPC_CLIENT_TYPE_RFS:
		port_type = WWAN_PORT_TYPE_RFS;
		break;
	default:
		return -1;
	}

	if (wwan_find_port(port_type, name, sizeof(name)) < 0) {
		ipc_client_log(client, "No WWAN port of type %s", port_type);
		errno = ENOENT;
		return -1;
	}

	snprintf(path, sizeof(path), "/dev/%s", name);

	/* Opening the IPC port starts the modem: this can take seconds */
	fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
	if (fd < 0)
		ipc_client_log(client, "Opening %s failed: %s", path,
			       strerror(errno));

	return fd;
}
