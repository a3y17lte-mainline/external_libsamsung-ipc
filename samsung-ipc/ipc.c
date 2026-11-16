/*
 * This file is part of libsamsung-ipc.
 *
 * Copyright (C) 2010-2011 Joerie de Gram <j.de.gram@gmail.com>
 * Copyright (C) 2011 Simon Busch <morphis@gravedo.de>
 * Copyright (C) 2013-2014 Paul Kocialkowsk <contact@paulk.fr>
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
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <samsung-ipc.h>

#include "ipc.h"
#include "devices/ipc_devices.h"
#include "partitions/toc/toc.h"

/* Where the kernel shows the device tree's root compatible list */
static const char *const ipc_compatible_paths[] = {
	"/proc/device-tree/compatible",
	"/sys/firmware/devicetree/base/compatible",
	NULL
};

int ipc_device_match(const char *name, const char *compatible, size_t size)
{
	unsigned int i, j;
	size_t offset;

	if (name != NULL) {
		for (i = 0; i < ipc_devices_count; i++)
			if (strcmp(name, ipc_devices[i].name) == 0)
				return (int) i;

		return -1;
	}

	if (compatible == NULL)
		return -1;

	/* The list's entries in order: the first is the most specific */
	for (offset = 0; offset < size;) {
		const char *entry = compatible + offset;
		size_t length = strnlen(entry, size - offset);

		for (i = 0; i < ipc_devices_count; i++) {
			const char *const *list = ipc_devices[i].compatibles;

			for (j = 0; list != NULL && list[j] != NULL; j++)
				if (strlen(list[j]) == length &&
				    memcmp(list[j], entry, length) == 0)
					return (int) i;
		}

		offset += length + 1;
	}

	return -1;
}

/*
 * Which device this is: the name in LIBSAMSUNG_IPC_DEVICE, else the name
 * the library was built for (IPC_DEVICE_NAME), else the device tree.
 */
int ipc_device_detect(void)
{
	char compatible[1024];
	const char *name;
	ssize_t length;
	unsigned int i;
	int fd;

	name = getenv("LIBSAMSUNG_IPC_DEVICE");
	if (name != NULL && *name == '\0')
		name = NULL;
#ifdef IPC_DEVICE_NAME
	if (name == NULL)
		name = IPC_DEVICE_NAME;
#endif
	if (name != NULL)
		return ipc_device_match(name, NULL, 0);

	for (i = 0; ipc_compatible_paths[i] != NULL; i++) {
		fd = open(ipc_compatible_paths[i], O_RDONLY | O_CLOEXEC);
		if (fd < 0)
			continue;

		length = read(fd, compatible, sizeof(compatible));
		close(fd);
		if (length > 0)
			return ipc_device_match(NULL, compatible,
						(size_t) length);
	}

	return -1;
}

size_t ipc_client_firmware_nv_size(struct ipc_client *client)
{
	if (client == NULL || client->device == NULL)
		return 0;

	return toc_nv_size(client, client->device->firmware_partitions);
}

static struct ipc_client *ipc_dummy_client_create(void)
{
	struct ipc_client *client = NULL;

	client = (struct ipc_client *) calloc(1, sizeof(struct ipc_client));
	if (client == NULL)
		return NULL;

	client->type = IPC_CLIENT_TYPE_DUMMY;

	return client;
}

static struct ipc_client *ipc_transport_client_create(int type)
{
	struct ipc_client *client = NULL;
	unsigned int device_index;
	int rc;

	if (type < 0 || type > IPC_CLIENT_TYPE_RFS)
		return NULL;

	rc = ipc_device_detect();
	if (rc < 0)
		goto error;

	device_index = (unsigned int) rc;
	if (device_index >= ipc_devices_count)
		goto error;

	client = (struct ipc_client *) calloc(1, sizeof(struct ipc_client));
	if (client == NULL)
		goto error;

	client->type = type;

	switch (type) {
	case IPC_CLIENT_TYPE_RFS:
		client->ops = &shannon_rfs_ops;
		break;
	case IPC_CLIENT_TYPE_FMT:
		client->ops = &shannon_fmt_ops;
		break;
	default:
		goto error;
	}

	client->device = &ipc_devices[device_index];
	client->gprs_specs = &shannon_gprs_specs;
	client->nv_data_specs = &ipc_devices[device_index].nv_data_specs;

	/* Handlers can be modified */
	client->handlers = (struct ipc_client_handlers *) calloc(
		1, sizeof(struct ipc_client_handlers));
	if (client->handlers == NULL)
		goto error;

	memcpy(client->handlers, &shannon_handlers,
	       sizeof(struct ipc_client_handlers));

	goto complete;

error:
	if (client != NULL) {
		free(client);
		client = NULL;
	}

complete:
	return client;
}

struct ipc_client *ipc_client_create(int type)
{
	switch (type) {
	case IPC_CLIENT_TYPE_RFS:
	case IPC_CLIENT_TYPE_FMT:
		return ipc_transport_client_create(type);
	case IPC_CLIENT_TYPE_DUMMY:
		return ipc_dummy_client_create();
	default:
		return NULL;
	}
}

int ipc_client_destroy(struct ipc_client *client)
{
	if (client == NULL)
		return -1;

	if (client->handlers != NULL)
		free(client->handlers);

	memset(client, 0, sizeof(struct ipc_client));
	free(client);

	return 0;
}

int ipc_client_transport_handlers_register(
	struct ipc_client *client,
	int (*open)(struct ipc_client *client, void *transport_data, int type),
	int (*close)(struct ipc_client *client, void *transport_data),
	int (*read)(struct ipc_client *client, void *transport_data, void *data,
		    size_t size),
	int (*write)(struct ipc_client *client, void *transport_data,
		     const void *data, size_t size),
	int (*poll)(struct ipc_client *client, void *transport_data,
		    struct ipc_poll_fds *fds, struct timeval *timeout),
	void *transport_data)
{
	if (client == NULL || client->handlers == NULL)
		return -1;

	if (read != NULL)
		client->handlers->read = read;
	if (write != NULL)
		client->handlers->write = write;
	if (poll != NULL)
		client->handlers->poll = poll;
	if (open != NULL)
		client->handlers->open = open;
	if (close != NULL)
		client->handlers->close = close;
	if (transport_data != NULL)
		client->handlers->transport_data = transport_data;

	return 0;
}

int ipc_client_power_handlers_register(
	struct ipc_client *client,
	int (*power_on)(struct ipc_client *client, void *power_data),
	int (*power_off)(struct ipc_client *client, void *power_data),
				       void *power_data)
{
	if (client == NULL || client->handlers == NULL)
		return -1;

	if (power_on != NULL)
		client->handlers->power_on = power_on;
	if (power_off != NULL)
		client->handlers->power_off = power_off;
	if (power_data != NULL)
		client->handlers->power_data = power_data;

	return 0;
}

int ipc_client_gprs_handlers_register(
	struct ipc_client *client,
	int (*gprs_activate)(struct ipc_client *client, void *gprs_data,
			     unsigned int cid),
	int (*gprs_deactivate)(struct ipc_client *client, void *gprs_data,
			       unsigned int cid), void *gprs_data)
{
	if (client == NULL || client->handlers == NULL)
		return -1;

	if (gprs_activate != NULL)
		client->handlers->gprs_activate = gprs_activate;
	if (gprs_deactivate != NULL)
		client->handlers->gprs_deactivate = gprs_deactivate;
	if (gprs_data != NULL)
		client->handlers->gprs_data = gprs_data;

	return 0;
}

void ipc_client_log(struct ipc_client *client, const char *message, ...)
{
	char buffer[4096];
	va_list args;

	if (client == NULL || client->log_callback == NULL || message == NULL)
		return;

	va_start(args, message);
	vsnprintf((char *) &buffer, sizeof(buffer), message, args);
	client->log_callback(client->log_data, buffer);
	va_end(args);
}

int ipc_client_log_callback_register(
	struct ipc_client *client,
	void (*log_callback)(void *log_data, const char *message),
	void *log_data)
{
	if (client == NULL)
		return -1;

	client->log_callback = log_callback;
	client->log_data = log_data;

	return 0;
}

int ipc_client_boot(struct ipc_client *client)
{
	if (client == NULL || client->ops == NULL || client->ops->boot == NULL)
		return -1;

	return client->ops->boot(client);
}

int ipc_client_send(struct ipc_client *client, unsigned char mseq,
		    unsigned short command, unsigned char type,
		    const void *data, size_t size)
{
	struct ipc_message message;

	if (client == NULL || client->ops == NULL || client->ops->send == NULL)
		return -1;

	memset(&message, 0, sizeof(message));
	message.mseq = mseq;
	message.aseq = 0xff;
	message.command = command;
	message.type = type;
	message.data = (void *) data;
	message.size = size;

	return client->ops->send(client, &message);
}

int ipc_client_recv(struct ipc_client *client, struct ipc_message *message)
{
	if (client == NULL || client->ops == NULL ||
	    client->ops->recv == NULL || message == NULL) {
		return -1;
	}

	return client->ops->recv(client, message);
}

int ipc_client_open(struct ipc_client *client)
{
	if (client == NULL)
		return -1;

	if (client->handlers == NULL) {
		ipc_client_log(client, "%s failed: client->handlers is NULL",
			       __func__);
		return -1;
	}

	if (client->handlers->open == NULL) {
		ipc_client_log(client,
			       "%s failed: client->handlers->open is NULL",
			       __func__);
		return -1;
	}

	return client->handlers->open(client, client->handlers->transport_data,
				      client->type);
}

int ipc_client_close(struct ipc_client *client)
{
	if (client == NULL || client->handlers == NULL ||
	    client->handlers->close == NULL) {
		return -1;
	}

	return client->handlers->close(client,
				       client->handlers->transport_data);
}

int ipc_client_poll(struct ipc_client *client, struct ipc_poll_fds *fds,
		    struct timeval *timeout)
{
	if (client == NULL || client->handlers == NULL ||
	    client->handlers->poll == NULL) {
		return -1;
	}

	return client->handlers->poll(client, client->handlers->transport_data,
				      fds, timeout);
}

int ipc_client_power_on(struct ipc_client *client)
{
	if (client == NULL || client->handlers == NULL ||
	    client->handlers->power_on == NULL) {
		return -1;
	}

	return client->handlers->power_on(client, client->handlers->power_data);
}

int ipc_client_power_off(struct ipc_client *client)
{
	if (client == NULL || client->handlers == NULL ||
	    client->handlers->power_off == NULL) {
		return -1;
	}

	return client->handlers->power_off(client,
					   client->handlers->power_data);
}

int ipc_client_gprs_activate(struct ipc_client *client, unsigned int cid)
{
	if (client == NULL || client->handlers == NULL ||
	    client->handlers->gprs_activate == NULL) {
		return -1;
	}

	return client->handlers->gprs_activate(client,
					       client->handlers->gprs_data,
					       cid);
}

int ipc_client_gprs_deactivate(struct ipc_client *client, unsigned int cid)
{
	if (client == NULL || client->handlers == NULL ||
	    client->handlers->gprs_deactivate == NULL) {
		return -1;
	}

	return client->handlers->gprs_deactivate(client,
						 client->handlers->gprs_data,
						 cid);
}

int ipc_client_data_create(struct ipc_client *client)
{
	if (client == NULL || client->handlers == NULL ||
	    client->handlers->data_create == NULL) {
		return -1;
	}

	return client->handlers->data_create(client,
					     &client->handlers->transport_data,
					     &client->handlers->power_data,
					     &client->handlers->gprs_data);
}

int ipc_client_data_destroy(struct ipc_client *client)
{
	if (client == NULL || client->handlers == NULL ||
	    client->handlers->data_destroy == NULL) {
		return -1;
	}

	return client->handlers->data_destroy(client,
					      client->handlers->transport_data,
					      client->handlers->power_data,
					      client->handlers->gprs_data);
}

char *ipc_client_gprs_get_iface(struct ipc_client *client, unsigned int cid)
{
	if (client == NULL || client->gprs_specs == NULL ||
	    client->gprs_specs->gprs_get_iface == NULL) {
		return NULL;
	}

	return client->gprs_specs->gprs_get_iface(client, cid);
}

int ipc_client_gprs_get_capabilities(
	struct ipc_client *client,
	struct ipc_client_gprs_capabilities *capabilities)
{
	if (client == NULL || client->gprs_specs == NULL ||
	    client->gprs_specs->gprs_get_capabilities == NULL) {
		return -1;
	}

	return client->gprs_specs->gprs_get_capabilities(client, capabilities);
}

char *ipc_client_nv_data_path(struct ipc_client *client)
{
	if (client == NULL || client->nv_data_specs == NULL ||
	    client->nv_data_specs->nv_data_path == NULL) {
		return NULL;
	}

	return client->nv_data_specs->nv_data_path;
}

char *ipc_client_nv_data_md5_path(struct ipc_client *client)
{
	if (client == NULL || client->nv_data_specs == NULL ||
	    client->nv_data_specs->nv_data_md5_path == NULL) {
		return NULL;
	}

	return client->nv_data_specs->nv_data_md5_path;
}

char *ipc_client_nv_data_backup_path(struct ipc_client *client)
{
	if (client == NULL || client->nv_data_specs == NULL ||
	    client->nv_data_specs->nv_data_backup_path == NULL) {
		return NULL;
	}

	return client->nv_data_specs->nv_data_backup_path;
}

char *ipc_client_nv_data_backup_md5_path(struct ipc_client *client)
{
	if (client == NULL || client->nv_data_specs == NULL ||
	    client->nv_data_specs->nv_data_backup_md5_path == NULL) {
		return NULL;
	}

	return client->nv_data_specs->nv_data_backup_md5_path;
}

char *ipc_client_nv_data_secret(struct ipc_client *client)
{
	if (client == NULL || client->nv_data_specs == NULL ||
	    client->nv_data_specs->nv_data_secret == NULL) {
		return NULL;
	}

	return client->nv_data_specs->nv_data_secret;
}

size_t ipc_client_nv_data_size(struct ipc_client *client)
{
	if (client == NULL || client->nv_data_specs == NULL ||
	    client->nv_data_specs->nv_data_size == 0) {
		return 0;
	}

	return client->nv_data_specs->nv_data_size;
}

size_t ipc_client_nv_data_chunk_size(struct ipc_client *client)
{
	if (client == NULL || client->nv_data_specs == NULL ||
	    client->nv_data_specs->nv_data_chunk_size == 0) {
		return 0;
	}

	return client->nv_data_specs->nv_data_chunk_size;
}
