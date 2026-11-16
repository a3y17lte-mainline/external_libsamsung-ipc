/*
 * This file is part of libsamsung-ipc.
 *
 * Copyright (C) 2012 Alexander Tarasikov <alexander.tarasikov@gmail.com>
 * Copyright (C) 2013-2014 Paul Kocialkowski <contact@paulk.fr>
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

#define _GNU_SOURCE
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <samsung-ipc.h>

#include "ipc.h"
#include "modems/xmm626/xmm626.h"
#include "modems/xmm626/xmm626_kernel_smdk4412.h"
#include "modems/xmm626/xmm626_modem_prj.h"

int xmm626_kernel_smdk4412_power(
	__attribute__((unused)) struct ipc_client *client, int device_fd,
	int power)
{
	int rc;

	if (device_fd < 0)
		return -1;

	rc = ioctl(device_fd, power ? IOCTL_MODEM_ON : IOCTL_MODEM_OFF, 0);
	if (rc < 0)
		return -1;

	return 0;
}

int xmm626_kernel_smdk4412_boot_power(
	__attribute__((unused)) struct ipc_client *client, int device_fd,
	int power)
{
	int rc;

	if (device_fd < 0)
		return -1;

	rc = ioctl(device_fd, power ?
		   IOCTL_MODEM_BOOT_ON : IOCTL_MODEM_BOOT_OFF,
		   0);
	if (rc < 0)
		return -1;

	return 0;
}

/* Writes a whole frame; the transport may take it in pieces */
static int xmm626_kernel_smdk4412_frame_write(struct ipc_client *client,
					      const void *buffer, size_t length)
{
	const unsigned char *p = buffer;
	size_t count = 0;
	int rc;

	while (count < length) {
		rc = client->handlers->write(client,
					     client->handlers->transport_data,
					     p + count, length - count);
		if (rc <= 0)
			return -1;

		count += rc;
	}

	return 0;
}

/*
 * Reads one frame: a header of header_size bytes that starts with the
 * frame's length (16 bits for FMT, 32 bits for RFS), then the data.
 * The transport hands a frame over in one or more reads. The frame's own
 * length is checked against the header size and limit before it is used, and
 * whatever a read returns beyond it is not part of the frame.
 */
static void *xmm626_kernel_smdk4412_frame_read(struct ipc_client *client,
					       size_t header_size, int rfs,
					       size_t limit, size_t *size)
{
	unsigned char *buffer;
	unsigned char *p;
	size_t length;
	size_t count;
	int rc;

	buffer = calloc(1, XMM626_DATA_SIZE);
	if (buffer == NULL)
		return NULL;

	rc = client->handlers->read(client, client->handlers->transport_data,
				    buffer, XMM626_DATA_SIZE);
	if (rc < (int) header_size) {
		ipc_client_log(client, "Reading the %s header failed",
			       rfs ? "RFS" : "FMT");
		goto error;
	}

	count = (size_t) rc;

	if (rfs)
		length = ((struct ipc_rfs_header *) buffer)->length;
	else
		length = ((struct ipc_fmt_header *) buffer)->length;

	if (length < header_size || length > limit) {
		ipc_client_log(client, "Invalid %s header length: %zu",
			       rfs ? "RFS" : "FMT", length);
		goto error;
	}

	if (count > length) {
		ipc_client_log(client,
			       "Dropping %zu bytes after the %s message",
			       count - length, rfs ? "RFS" : "FMT");
		count = length;
	}

	if (length > XMM626_DATA_SIZE) {
		p = realloc(buffer, length);
		if (p == NULL)
			goto error;

		buffer = p;
	}

	while (count < length) {
		rc = client->handlers->read(client,
					    client->handlers->transport_data,
					    buffer + count, length - count);
		if (rc <= 0) {
			ipc_client_log(client, "Reading %s data failed",
				       rfs ? "RFS" : "FMT");
			goto error;
		}

		count += rc;
	}

	*size = length;

	return buffer;

error:
	free(buffer);

	return NULL;
}

/* The data after the header, as the message's own copy */
static int xmm626_kernel_smdk4412_message_data(struct ipc_message *message,
					       const unsigned char *frame,
					       size_t size, size_t header_size)
{
	if (size == header_size)
		return 0;

	message->data = malloc(size - header_size);
	if (message->data == NULL)
		return -1;

	memcpy(message->data, frame + header_size, size - header_size);
	message->size = size - header_size;

	return 0;
}

int xmm626_kernel_smdk4412_fmt_send(struct ipc_client *client,
				    struct ipc_message *message)
{
	struct ipc_fmt_header header;
	unsigned char *buffer;
	size_t length;
	int rc;

	if (client == NULL || client->handlers == NULL ||
	    client->handlers->write == NULL || message == NULL) {
		return -1;
	}

	/* The header's length field has 16 bits */
	if (message->size > UINT16_MAX - sizeof(struct ipc_fmt_header) ||
	    (message->size > 0 && message->data == NULL)) {
		ipc_client_log(client, "Invalid FMT message size: %zu",
			       message->size);
		return -1;
	}

	ipc_fmt_header_setup(&header, message);

	length = sizeof(struct ipc_fmt_header) + message->size;
	buffer = calloc(1, length);
	if (buffer == NULL)
		return -1;

	memcpy(buffer, &header, sizeof(struct ipc_fmt_header));
	if (message->size > 0) {
		memcpy(buffer + sizeof(struct ipc_fmt_header), message->data,
		       message->size);
	}

	ipc_client_log_send(client, message, __func__);

	rc = xmm626_kernel_smdk4412_frame_write(client, buffer, length);
	if (rc < 0)
		ipc_client_log(client, "Writing FMT data failed");

	free(buffer);

	return rc;
}

int xmm626_kernel_smdk4412_fmt_recv(struct ipc_client *client,
				    struct ipc_message *message)
{
	unsigned char *frame;
	size_t size;
	int rc;

	if (client == NULL || client->handlers == NULL ||
	    client->handlers->read == NULL || message == NULL) {
		return -1;
	}

	frame = xmm626_kernel_smdk4412_frame_read(
		client, sizeof(struct ipc_fmt_header), 0, UINT16_MAX, &size);
	if (frame == NULL)
		return -1;

	ipc_fmt_message_setup((struct ipc_fmt_header *) frame, message);

	rc = xmm626_kernel_smdk4412_message_data(
		message, frame, size, sizeof(struct ipc_fmt_header));
	if (rc == 0)
		ipc_client_log_recv(client, message, __func__);

	free(frame);

	return rc;
}

int xmm626_kernel_smdk4412_rfs_send(struct ipc_client *client,
				    struct ipc_message *message)
{
	struct ipc_rfs_header header;
	unsigned char *buffer;
	size_t length;
	int rc;

	if (client == NULL || client->handlers == NULL ||
	    client->handlers->write == NULL || message == NULL) {
		return -1;
	}

	if (message->size >
	    XMM626_DATA_SIZE_LIMIT - sizeof(struct ipc_rfs_header) ||
	    (message->size > 0 && message->data == NULL)) {
		ipc_client_log(client, "Invalid RFS message size: %zu",
			       message->size);
		return -1;
	}

	ipc_rfs_header_setup(&header, message);

	length = sizeof(struct ipc_rfs_header) + message->size;
	buffer = calloc(1, length);
	if (buffer == NULL)
		return -1;

	memcpy(buffer, &header, sizeof(struct ipc_rfs_header));
	if (message->size > 0) {
		memcpy(buffer + sizeof(struct ipc_rfs_header), message->data,
		       message->size);
	}

	ipc_client_log_send(client, message, __func__);

	rc = xmm626_kernel_smdk4412_frame_write(client, buffer, length);
	if (rc < 0)
		ipc_client_log(client, "Writing RFS data failed");

	free(buffer);

	return rc;
}

int xmm626_kernel_smdk4412_rfs_recv(struct ipc_client *client,
				    struct ipc_message *message)
{
	unsigned char *frame;
	size_t size;
	int rc;

	if (client == NULL || client->handlers == NULL ||
	    client->handlers->read == NULL || message == NULL) {
		return -1;
	}

	frame = xmm626_kernel_smdk4412_frame_read(
		client, sizeof(struct ipc_rfs_header), 1,
		XMM626_DATA_SIZE_LIMIT, &size);
	if (frame == NULL)
		return -1;

	ipc_rfs_message_setup((struct ipc_rfs_header *) frame, message);

	rc = xmm626_kernel_smdk4412_message_data(
		message, frame, size, sizeof(struct ipc_rfs_header));
	if (rc == 0)
		ipc_client_log_recv(client, message, __func__);

	free(frame);

	return rc;
}

int xmm626_kernel_smdk4412_open(
	__attribute__((unused)) struct ipc_client *client, int type)
{
	int fd;

	switch (type) {
	case IPC_CLIENT_TYPE_FMT:
		fd = open(XMM626_SEC_MODEM_IPC0_DEVICE,
			  O_RDWR | O_NOCTTY | O_NONBLOCK);
		break;
	case IPC_CLIENT_TYPE_RFS:
		fd = open(XMM626_SEC_MODEM_RFS0_DEVICE,
			  O_RDWR | O_NOCTTY | O_NONBLOCK);
		break;
	default:
		return -1;
	}

	return fd;
}

int xmm626_kernel_smdk4412_close(
	__attribute__((unused)) struct ipc_client *client, int fd)
{
	if (fd < 0)
		return -1;

	close(fd);

	return 0;
}

int xmm626_kernel_smdk4412_read(
	__attribute__((unused)) struct ipc_client *client, int fd, void *buffer,
	size_t length)
{
	int status;
	int rc;

	if (fd < 0 || buffer == NULL || length <= 0)
		return -1;

	status = ioctl(fd, IOCTL_MODEM_STATUS, 0);
	if (status != STATE_ONLINE && status != STATE_BOOTING)
		return -1;

	rc = read(fd, buffer, length);

	return rc;
}

int xmm626_kernel_smdk4412_write(
	__attribute__((unused)) struct ipc_client *client, int fd,
	const void *buffer, size_t length)
{
	int status;
	int rc;

	if (fd < 0 || buffer == NULL || length <= 0)
		return -1;

	status = ioctl(fd, IOCTL_MODEM_STATUS, 0);
	if (status != STATE_ONLINE && status != STATE_BOOTING)
		return -1;

	rc = write(fd, buffer, length);

	return rc;
}

int xmm626_kernel_smdk4412_poll(
	__attribute__((unused)) struct ipc_client *client,
	int fd, struct ipc_poll_fds *fds, struct timeval *timeout)
{
	int status;
	int ready;
	int rc;

	if (fd < 0)
		return -1;

	rc = ipc_fds_poll(fd, fds, timeout, &ready);
	if (rc < 0)
		return -1;

	if (ready) {
		status = ioctl(fd, IOCTL_MODEM_STATUS, 0);
		if (status != STATE_ONLINE && status != STATE_BOOTING)
			return -1;
	}

	return rc;
}

char *xmm626_kernel_smdk4412_gprs_get_iface(
	__attribute__((unused)) struct ipc_client *client, unsigned int cid)
{
	char *iface = NULL;

	if (cid == 0 || cid > XMM626_SEC_MODEM_GPRS_IFACE_COUNT)
		return NULL;

	if (asprintf(&iface, "%s%u", XMM626_SEC_MODEM_GPRS_IFACE_PREFIX,
		     cid - 1) < 0) {
		return NULL;
	}

	return iface;
}

int xmm626_kernel_smdk4412_gprs_get_capabilities(
	__attribute__((unused)) struct ipc_client *client,
	struct ipc_client_gprs_capabilities *capabilities)
{
	if (capabilities == NULL)
		return -1;

	capabilities->cid_count = XMM626_SEC_MODEM_GPRS_IFACE_COUNT;

	return 0;
}
