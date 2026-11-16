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
 * The code every Shannon phone shares. The transport is picked when a
 * client opens: the mainline kernel's WWAN ports if the kernel has them (it
 * starts the modem itself), the vendor kernel's /dev/umts_* otherwise (the
 * client boots the modem). Set LIBSAMSUNG_IPC_FRAME_LOG to a file name to
 * append every message, both ways, as a line of hex to that file.
 */

#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <samsung-ipc.h>

#include "devices/ipc_devices.h"
#include "framing.h"
#include "ipc.h"
#include "transports/vendor.h"
#include "transports/wwan.h"

struct shannon_transport_data {
	int fd;
	int type;
	int vendor;
};

static void shannon_frame_log(int type, const char *direction,
			      const void *data, size_t length)
{
	const char *path = getenv("LIBSAMSUNG_IPC_FRAME_LOG");
	const unsigned char *p = data;
	struct timespec ts;
	FILE *file;
	size_t i;

	if (path == NULL || *path == '\0')
		return;

	file = fopen(path, "ae");
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

static int shannon_boot(struct ipc_client *client)
{
	/* The kernel starts the modem when the IPC port is opened */
	if (wwan_available())
		return 0;

	if (client->device == NULL)
		return -1;

	return vendor_boot(client, client->device->firmware_partitions);
}

static int shannon_open(struct ipc_client *client, void *data, int type)
{
	struct shannon_transport_data *transport_data = data;

	if (transport_data == NULL)
		return -1;

	transport_data->type = type;

	if (wwan_available()) {
		transport_data->vendor = 0;
		transport_data->fd = wwan_open(client, type);
	} else if (vendor_available()) {
		transport_data->vendor = 1;
		transport_data->fd = vendor_open(client, type);
	} else {
		ipc_client_log(client, "No modem interface in this kernel");
		return -1;
	}

	return transport_data->fd < 0 ? -1 : 0;
}

static int shannon_close(__attribute__((unused)) struct ipc_client *client,
			 void *data)
{
	struct shannon_transport_data *transport_data = data;

	if (transport_data == NULL)
		return -1;

	if (transport_data->fd >= 0)
		close(transport_data->fd);
	transport_data->fd = -1;

	return 0;
}

static int shannon_read(struct ipc_client *client, void *data, void *buffer,
			size_t length)
{
	struct shannon_transport_data *transport_data = data;
	int rc;

	if (transport_data == NULL || transport_data->fd < 0 ||
	    buffer == NULL || length == 0) {
		return -1;
	}

	if (transport_data->vendor)
		rc = vendor_read(client, transport_data->fd, buffer, length);
	else
		rc = read(transport_data->fd, buffer, length);

	if (rc > 0)
		shannon_frame_log(transport_data->type, "rx", buffer, rc);

	return rc;
}

static int shannon_write(struct ipc_client *client, void *data,
			 const void *buffer, size_t length)
{
	struct shannon_transport_data *transport_data = data;
	int rc;

	if (transport_data == NULL || transport_data->fd < 0 ||
	    buffer == NULL || length == 0) {
		return -1;
	}

	if (transport_data->vendor)
		rc = vendor_write(client, transport_data->fd, buffer, length);
	else
		rc = write(transport_data->fd, buffer, length);

	if (rc > 0)
		shannon_frame_log(transport_data->type, "tx", buffer, rc);

	return rc;
}

static int shannon_poll(struct ipc_client *client, void *data,
			struct ipc_poll_fds *fds, struct timeval *timeout)
{
	struct shannon_transport_data *transport_data = data;

	if (transport_data == NULL || transport_data->fd < 0)
		return -1;

	if (transport_data->vendor)
		return vendor_poll(client, transport_data->fd, fds, timeout);

	return ipc_fds_poll(transport_data->fd, fds, timeout, NULL);
}

static int shannon_power_on(__attribute__((unused)) struct ipc_client *client,
			    __attribute__((unused)) void *data)
{
	return 0;
}

static int shannon_power_off(struct ipc_client *client,
			     __attribute__((unused)) void *data)
{
	/* The kernel keeps the modem running; it stops it on shutdown */
	if (wwan_available())
		return 0;

	return vendor_power_off(client);
}

/* The modem activates contexts itself; nothing to do on the AP side */
static int shannon_gprs_activate(
	__attribute__((unused)) struct ipc_client *client,
	__attribute__((unused)) void *data,
	__attribute__((unused)) unsigned int cid)
{
	return 0;
}

static int shannon_gprs_deactivate(
	__attribute__((unused)) struct ipc_client *client,
	__attribute__((unused)) void *data,
	__attribute__((unused)) unsigned int cid)
{
	return 0;
}

static int shannon_data_create(
	__attribute__((unused)) struct ipc_client *client,
	void **transport_data, __attribute__((unused)) void **power_data,
	__attribute__((unused)) void **gprs_data)
{
	struct shannon_transport_data *data;

	if (transport_data == NULL)
		return -1;

	data = calloc(1, sizeof(*data));
	if (data == NULL)
		return -1;

	data->fd = -1;
	*transport_data = data;

	return 0;
}

static int shannon_data_destroy(
	__attribute__((unused)) struct ipc_client *client,
	void *transport_data, __attribute__((unused)) void *power_data,
	__attribute__((unused)) void *gprs_data)
{
	if (transport_data == NULL)
		return -1;

	free(transport_data);

	return 0;
}

static char *shannon_gprs_get_iface(struct ipc_client *client,
				    unsigned int cid)
{
	const struct ipc_device_desc *device = client->device;
	char *iface = NULL;

	if (device == NULL || cid == 0 || cid > device->gprs_iface_count)
		return NULL;

	if (asprintf(&iface, "%s%u", device->gprs_iface_prefix, cid - 1) < 0)
		return NULL;

	return iface;
}

static int shannon_gprs_get_capabilities(
	struct ipc_client *client,
	struct ipc_client_gprs_capabilities *capabilities)
{
	if (client->device == NULL || capabilities == NULL)
		return -1;

	capabilities->cid_count = client->device->gprs_iface_count;

	return 0;
}

struct ipc_client_ops shannon_fmt_ops = {
	.boot = shannon_boot,
	.send = sipc_fmt_send,
	.recv = sipc_fmt_recv,
};

struct ipc_client_ops shannon_rfs_ops = {
	.boot = NULL,
	.send = sipc_rfs_send,
	.recv = sipc_rfs_recv,
};

struct ipc_client_handlers shannon_handlers = {
	.read = shannon_read,
	.write = shannon_write,
	.open = shannon_open,
	.close = shannon_close,
	.poll = shannon_poll,
	.transport_data = NULL,
	.power_on = shannon_power_on,
	.power_off = shannon_power_off,
	.power_data = NULL,
	.gprs_activate = shannon_gprs_activate,
	.gprs_deactivate = shannon_gprs_deactivate,
	.gprs_data = NULL,
	.data_create = shannon_data_create,
	.data_destroy = shannon_data_destroy,
};

struct ipc_client_gprs_specs shannon_gprs_specs = {
	.gprs_get_iface = shannon_gprs_get_iface,
	.gprs_get_capabilities = shannon_gprs_get_capabilities,
};
