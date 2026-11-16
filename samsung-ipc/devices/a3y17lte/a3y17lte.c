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
 * Galaxy A3 2017 (SM-A320F, Exynos7870 with a Shannon 315 modem).
 *
 * With the mainline kernel driver the modem is reached through two WWAN
 * ports: Samsung IPC (type "SIPC") and the remote file system channel (type
 * "SIPC-RFS"). A read returns one message, a write sends one, and the
 * kernel loads and starts the modem when the IPC port is first opened, so
 * there is no boot code here.
 *
 * The kernel can still provide the vendor driver's interface instead
 * (/dev/umts_*, exynos_modem.legacy_abi=1). Then the herolte code is used,
 * which boots the modem from userspace.
 *
 * Set LIBSAMSUNG_IPC_FRAME_LOG to a file name to append every message, in
 * both directions, as a line of hex to that file.
 */

#define _GNU_SOURCE
#include <dirent.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <samsung-ipc.h>

#include "ipc.h"
#include "devices/herolte/herolte.h"
#include "modems/xmm626/xmm626_kernel_smdk4412.h"
#include "a3y17lte.h"

#define WWAN_PORT_CLASS		"/sys/class/wwan"
#define WWAN_PORT_TYPE_IPC	"SIPC"
#define WWAN_PORT_TYPE_RFS	"SIPC-RFS"

struct a3y17lte_transport_data {
	int fd;
	int legacy;
	int type;
	struct herolte_transport_data herolte;
};

/* The port name for a WWAN port type, or -1 if there is none */
static int a3y17lte_find_port(const char *type, char *name, size_t size)
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
		if (strcmp(value, type) == 0 &&
		    strlen(entry->d_name) < size) {
			strcpy(name, entry->d_name);
			rc = 0;
			break;
		}
	}

	closedir(dir);

	return rc;
}

static int a3y17lte_have_wwan(void)
{
	char name[64];

	return a3y17lte_find_port(WWAN_PORT_TYPE_IPC, name, sizeof(name)) == 0;
}

static void a3y17lte_frame_log(int type, const char *direction,
			       const void *data, size_t length)
{
	const char *path = getenv("LIBSAMSUNG_IPC_FRAME_LOG");
	const unsigned char *p = data;
	struct timespec ts;
	FILE *file;
	size_t i;

	if (path == NULL || *path == '\0')
		return;

	file = fopen(path, "a");
	if (file == NULL)
		return;

	clock_gettime(CLOCK_REALTIME, &ts);
	fprintf(file, "%lld.%03ld %s %s ", (long long) ts.tv_sec,
		ts.tv_nsec / 1000000,
		type == IPC_CLIENT_TYPE_RFS ? "rfs" : "fmt", direction);
	for (i = 0; i < length; i++)
		fprintf(file, "%02x", p[i]);
	fputc('\n', file);
	fclose(file);
}

int a3y17lte_boot(struct ipc_client *client)
{
	/* The kernel starts the modem when the IPC port is opened */
	if (a3y17lte_have_wwan())
		return 0;

	return herolte_boot(client);
}

int a3y17lte_open(struct ipc_client *client, void *data, int type)
{
	struct a3y17lte_transport_data *transport_data = data;
	char name[64];
	char path[PATH_MAX];
	const char *port_type;

	if (transport_data == NULL)
		return -1;

	transport_data->type = type;

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

	if (a3y17lte_find_port(port_type, name, sizeof(name)) < 0) {
		transport_data->legacy = 1;
		return herolte_open(client, &transport_data->herolte, type);
	}

	transport_data->legacy = 0;
	snprintf(path, sizeof(path), "/dev/%s", name);

	/* Opening the IPC port starts the modem: this can take seconds */
	transport_data->fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (transport_data->fd < 0) {
		ipc_client_log(client, "Opening %s failed: %s", path,
			       strerror(errno));
		return -1;
	}

	return 0;
}

int a3y17lte_close(struct ipc_client *client, void *data)
{
	struct a3y17lte_transport_data *transport_data = data;

	if (transport_data == NULL)
		return -1;

	if (transport_data->legacy)
		return herolte_close(client, &transport_data->herolte);

	if (transport_data->fd >= 0)
		close(transport_data->fd);
	transport_data->fd = -1;

	return 0;
}

int a3y17lte_read(struct ipc_client *client, void *data, void *buffer,
		  size_t length)
{
	struct a3y17lte_transport_data *transport_data = data;
	int rc;

	if (transport_data == NULL || buffer == NULL || length == 0)
		return -1;

	if (transport_data->legacy)
		rc = herolte_read(client, &transport_data->herolte, buffer,
				  length);
	else
		rc = read(transport_data->fd, buffer, length);

	if (rc > 0)
		a3y17lte_frame_log(transport_data->type, "rx", buffer, rc);

	return rc;
}

int a3y17lte_write(struct ipc_client *client, void *data, const void *buffer,
		   size_t length)
{
	struct a3y17lte_transport_data *transport_data = data;
	int rc;

	if (transport_data == NULL || buffer == NULL || length == 0)
		return -1;

	if (transport_data->legacy)
		rc = herolte_write(client, &transport_data->herolte, buffer,
				   length);
	else
		rc = write(transport_data->fd, buffer, length);

	if (rc > 0)
		a3y17lte_frame_log(transport_data->type, "tx", buffer, rc);

	return rc;
}

int a3y17lte_poll(struct ipc_client *client, void *data,
		  struct ipc_poll_fds *fds, struct timeval *timeout)
{
	struct a3y17lte_transport_data *transport_data = data;

	if (transport_data == NULL)
		return -1;

	if (transport_data->legacy)
		return herolte_poll(client, &transport_data->herolte, fds,
				    timeout);

	return ipc_fds_poll(transport_data->fd, fds, timeout, NULL);
}

int a3y17lte_power_on(__attribute__((unused)) struct ipc_client *client,
		      __attribute__((unused)) void *data)
{
	return 0;
}

int a3y17lte_power_off(struct ipc_client *client, void *data)
{
	/* The kernel keeps the modem running; it stops it on shutdown */
	if (a3y17lte_have_wwan())
		return 0;

	return herolte_power_off(client, data);
}

int a3y17lte_data_create(__attribute__((unused)) struct ipc_client *client,
			 void **transport_data,
			 __attribute__((unused)) void **power_data,
			 __attribute__((unused)) void **gprs_data)
{
	struct a3y17lte_transport_data *data;

	if (transport_data == NULL)
		return -1;

	data = calloc(1, sizeof(*data));
	if (data == NULL)
		return -1;

	data->fd = -1;
	data->herolte.fd = -1;
	*transport_data = data;

	return 0;
}

int a3y17lte_data_destroy(__attribute__((unused)) struct ipc_client *client,
			  void *transport_data,
			  __attribute__((unused)) void *power_data,
			  __attribute__((unused)) void *gprs_data)
{
	if (transport_data == NULL)
		return -1;

	free(transport_data);

	return 0;
}

/* The modem activates contexts itself; nothing to do on the AP side */
int a3y17lte_gprs_activate(__attribute__((unused)) struct ipc_client *client,
			   __attribute__((unused)) void *data,
			   __attribute__((unused)) unsigned int cid)
{
	return 0;
}

int a3y17lte_gprs_deactivate(__attribute__((unused)) struct ipc_client *client,
			     __attribute__((unused)) void *data,
			     __attribute__((unused)) unsigned int cid)
{
	return 0;
}

/*
 * Packet data contexts are WWAN links on the mainline kernel. Userspace
 * creates them, named like the vendor kernel's interfaces:
 *   ip link add rmnet0 parentdev wwan0 type wwan linkid 0
 */
char *a3y17lte_gprs_get_iface(__attribute__((unused)) struct ipc_client *client,
			      unsigned int cid)
{
	char *iface = NULL;

	if (cid == 0 || cid > A3Y17LTE_GPRS_IFACE_COUNT)
		return NULL;

	if (asprintf(&iface, "rmnet%u", cid - 1) < 0)
		return NULL;

	return iface;
}

int a3y17lte_gprs_get_capabilities(
	__attribute__((unused)) struct ipc_client *client,
	struct ipc_client_gprs_capabilities *capabilities)
{
	if (capabilities == NULL)
		return -1;

	capabilities->cid_count = A3Y17LTE_GPRS_IFACE_COUNT;

	return 0;
}

struct ipc_client_ops a3y17lte_fmt_ops = {
	.boot = a3y17lte_boot,
	.send = xmm626_kernel_smdk4412_fmt_send,
	.recv = xmm626_kernel_smdk4412_fmt_recv,
};

struct ipc_client_ops a3y17lte_rfs_ops = {
	.boot = NULL,
	.send = xmm626_kernel_smdk4412_rfs_send,
	.recv = xmm626_kernel_smdk4412_rfs_recv,
};

struct ipc_client_handlers a3y17lte_handlers = {
	.read = a3y17lte_read,
	.write = a3y17lte_write,
	.open = a3y17lte_open,
	.close = a3y17lte_close,
	.poll = a3y17lte_poll,
	.transport_data = NULL,
	.power_on = a3y17lte_power_on,
	.power_off = a3y17lte_power_off,
	.power_data = NULL,
	.gprs_activate = a3y17lte_gprs_activate,
	.gprs_deactivate = a3y17lte_gprs_deactivate,
	.gprs_data = NULL,
	.data_create = a3y17lte_data_create,
	.data_destroy = a3y17lte_data_destroy,
};

struct ipc_client_gprs_specs a3y17lte_gprs_specs = {
	.gprs_get_iface = a3y17lte_gprs_get_iface,
	.gprs_get_capabilities = a3y17lte_gprs_get_capabilities,
};
