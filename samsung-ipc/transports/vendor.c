/*
 * This file is part of libsamsung-ipc.
 *
 * Copyright (C) 2012 Alexander Tarasikov <alexander.tarasikov@gmail.com>
 * Copyright (C) 2013-2014 Paul Kocialkowski <contact@paulk.fr>
 * Copyright (C) 2017 Wolfgang Wiedmeyer <wolfgit@wiedmeyer.de>
 * Copyright (C) 2020 Tony Garnock-Jones <tonyg@leastfixedpoint.com>
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

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <samsung-ipc.h>

#include "ipc.h"
#include "partitions/toc/toc.h"
#include "transports/vendor.h"
#include "transports/vendor_modem.h"

/* What Samsung's cbd sends per IOCTL_MODEM_XMIT_BOOT */
#define VENDOR_CHUNK_SIZE	(62 * 1024)

int vendor_available(void)
{
	return access(VENDOR_BOOT_DEVICE, F_OK) == 0;
}

static int vendor_upload(struct ipc_client *client, int device_fd,
			 int image_fd, const struct firmware_toc_entry *toc,
			 const char *name, uint32_t *size)
{
	const struct firmware_toc_entry *boot;
	const struct firmware_toc_entry *entry;
	struct modem_firmware firmware;
	unsigned char *chunk = NULL;
	uint32_t remaining;
	int rc = -1;

	boot = find_toc_entry("BOOT", toc);
	entry = find_toc_entry(name, toc);
	if (boot == NULL || entry == NULL) {
		ipc_client_log(client, "%s: no %s entry in the TOC", __func__,
			       boot == NULL ? "BOOT" : name);
		return -1;
	}

	if (entry->loadaddr < boot->loadaddr) {
		ipc_client_log(client, "%s: %s is loaded below BOOT", __func__,
			       name);
		return -1;
	}

	ipc_client_log(client, "Uploading %s: %u bytes", name, entry->size);

	if (size != NULL)
		*size = entry->size;

	chunk = malloc(VENDOR_CHUNK_SIZE);
	if (chunk == NULL)
		return -1;

	memset(&firmware, 0, sizeof(firmware));
	firmware.binary = (uint64_t) (uintptr_t) chunk;
	firmware.size = entry->size;
	firmware.m_offset = entry->loadaddr - boot->loadaddr;
	firmware.b_offset = entry->offset;

	if (lseek(image_fd, entry->offset, SEEK_SET) < 0) {
		ipc_client_log(client, "%s: lseek failed: %s", __func__,
			       strerror(errno));
		goto exit;
	}

	remaining = entry->size;
	while (remaining > 0) {
		firmware.len = remaining < VENDOR_CHUNK_SIZE ?
			remaining : VENDOR_CHUNK_SIZE;

		if (data_read(client, image_fd, chunk, firmware.len) !=
		    (ssize_t) firmware.len) {
			ipc_client_log(client, "%s: reading %s failed: %s",
				       __func__, name, strerror(errno));
			goto exit;
		}

		if (ioctl(device_fd, IOCTL_MODEM_XMIT_BOOT, &firmware) < 0) {
			ipc_client_log(client,
				       "%s: IOCTL_MODEM_XMIT_BOOT failed: %s",
				       __func__, strerror(errno));
			goto exit;
		}

		firmware.m_offset += firmware.len;
		firmware.b_offset += firmware.len;
		remaining -= firmware.len;
	}

	rc = 0;

exit:
	free(chunk);

	return rc;
}

static int vendor_security(struct ipc_client *client, int device_fd,
			   int secure, uint32_t size_boot, uint32_t size_main)
{
	struct modem_sec_req req;

	memset(&req, 0, sizeof(req));
	req.mode = secure ? 0 : 2;
	req.size_boot = size_boot;
	req.size_main = size_main;

	if (ioctl(device_fd, IOCTL_SECURITY_REQ, &req) < 0) {
		ipc_client_log(client, "%s: IOCTL_SECURITY_REQ (%s) failed: %s",
			       __func__, secure ? "secure" : "insecure",
			       strerror(errno));
		return -1;
	}

	return 0;
}

static int vendor_ioctl(struct ipc_client *client, int device_fd,
			unsigned long request, const char *name)
{
	if (ioctl(device_fd, request, 0) < 0) {
		ipc_client_log(client, "%s failed: %s", name, strerror(errno));
		return -1;
	}

	return 0;
}

/*
 * The handshake Samsung's cbd runs with the started boot loader: two words
 * out, two expected answers. Where the values come from is not known.
 */
static int vendor_handshake(struct ipc_client *client, int device_fd)
{
	static const uint32_t steps[][2] = {
		{ 0x900d, 0xa00d },
		{ 0x9f00, 0xaf00 },
	};
	unsigned int i;
	uint32_t word;

	for (i = 0; i < sizeof(steps) / sizeof(steps[0]); i++) {
		word = steps[i][0];
		if (data_write(client, device_fd, &word, sizeof(word)) !=
		    sizeof(word)) {
			ipc_client_log(client, "%s: write failed: %s",
				       __func__, strerror(errno));
			return -1;
		}

		if (data_read(client, device_fd, &word, sizeof(word)) !=
		    sizeof(word)) {
			ipc_client_log(client, "%s: read failed: %s",
				       __func__, strerror(errno));
			return -1;
		}

		if (word != steps[i][1]) {
			ipc_client_log(client,
				       "Handshake step %u failed: expected "
				       "0x%x, got 0x%x", i + 1, steps[i][1],
				       word);
			return -1;
		}
	}

	return 0;
}

int vendor_boot(struct ipc_client *client,
		const char *const *firmware_partitions)
{
	struct firmware_toc_entry toc[N_TOC_ENTRIES];
	const struct firmware_toc_entry *nv;
	uint32_t size_boot = 0;
	uint32_t size_main = 0;
	const char *nv_path;
	void *nv_data;
	int device_fd = -1;
	int image_fd = -1;
	int nv_fd = -1;
	int rc = -1;

	image_fd = toc_open(client, firmware_partitions, toc);
	if (image_fd < 0)
		goto exit;

	/* The modem reserves the NV size its firmware was built for */
	nv = find_toc_entry("NV", toc);
	if (nv == NULL || nv->size != ipc_client_nv_data_size(client)) {
		ipc_client_log(client,
			       "The firmware's NV size (%u) is not this "
			       "device's (%zu)", nv != NULL ? nv->size : 0,
			       ipc_client_nv_data_size(client));
		goto exit;
	}

	/* Checked, and restored from the checked backup if it has to be */
	nv_data = ipc_nv_data_load(client);
	if (nv_data == NULL) {
		ipc_client_log(client, "No valid NV data");
		goto exit;
	}
	free(nv_data);

	nv_path = ipc_client_nv_data_path(client);

	nv_fd = open(nv_path, O_RDONLY | O_NOCTTY);
	if (nv_fd < 0) {
		ipc_client_log(client, "Opening %s failed: %s", nv_path,
			       strerror(errno));
		goto exit;
	}

	device_fd = open(VENDOR_BOOT_DEVICE, O_RDWR | O_NOCTTY);
	if (device_fd < 0) {
		ipc_client_log(client, "Opening %s failed: %s",
			       VENDOR_BOOT_DEVICE, strerror(errno));
		goto exit;
	}

	ipc_client_log(client, "Resetting the modem");
	if (vendor_ioctl(client, device_fd, IOCTL_MODEM_RESET,
			 "IOCTL_MODEM_RESET") < 0)
		goto exit;

	if (vendor_security(client, device_fd, 0, 0, 0) < 0)
		goto exit;

	if (vendor_upload(client, device_fd, image_fd, toc, "BOOT",
			  &size_boot) < 0 ||
	    vendor_upload(client, device_fd, image_fd, toc, "MAIN",
			  &size_main) < 0 ||
	    vendor_upload(client, device_fd, nv_fd, toc, "NV", NULL) < 0)
		goto exit;

	if (vendor_security(client, device_fd, 1, size_boot, size_main) < 0)
		goto exit;

	ipc_client_log(client, "Starting the modem");
	if (vendor_ioctl(client, device_fd, IOCTL_MODEM_ON,
			 "IOCTL_MODEM_ON") < 0 ||
	    vendor_ioctl(client, device_fd, IOCTL_MODEM_BOOT_ON,
			 "IOCTL_MODEM_BOOT_ON") < 0 ||
	    vendor_ioctl(client, device_fd, IOCTL_MODEM_DL_START,
			 "IOCTL_MODEM_DL_START") < 0)
		goto exit;

	if (vendor_handshake(client, device_fd) < 0)
		goto exit;

	if (vendor_ioctl(client, device_fd, IOCTL_MODEM_BOOT_OFF,
			 "IOCTL_MODEM_BOOT_OFF") < 0)
		goto exit;

	ipc_client_log(client, "Modem boot complete");
	rc = 0;

	/*
	 * The vendor driver only passes messages once both the FMT and the RFS
	 * channel are open (rild_ready() in link_device_shmem.c), so a client
	 * needs both. A crashed modem shows as a status other than ONLINE or
	 * BOOTING: reads, writes and polls fail then, and booting again is up
	 * to the daemon.
	 */

exit:
	if (device_fd >= 0)
		close(device_fd);
	if (image_fd >= 0)
		close(image_fd);
	if (nv_fd >= 0)
		close(nv_fd);

	return rc;
}

int vendor_power_off(struct ipc_client *client)
{
	int fd;
	int rc;

	fd = open(VENDOR_BOOT_DEVICE, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (fd < 0)
		return -1;

	rc = vendor_ioctl(client, fd, IOCTL_MODEM_OFF, "IOCTL_MODEM_OFF");

	close(fd);

	return rc;
}

int vendor_open(__attribute__((unused)) struct ipc_client *client, int type)
{
	switch (type) {
	case IPC_CLIENT_TYPE_FMT:
		return open(VENDOR_IPC_DEVICE, O_RDWR | O_NOCTTY | O_NONBLOCK);
	case IPC_CLIENT_TYPE_RFS:
		return open(VENDOR_RFS_DEVICE, O_RDWR | O_NOCTTY | O_NONBLOCK);
	default:
		return -1;
	}
}

static int vendor_online(int fd)
{
	int status = ioctl(fd, IOCTL_MODEM_STATUS, 0);

	return status == STATE_ONLINE || status == STATE_BOOTING;
}

int vendor_read(__attribute__((unused)) struct ipc_client *client, int fd,
		void *buffer, size_t length)
{
	if (fd < 0 || buffer == NULL || length == 0 || !vendor_online(fd))
		return -1;

	return read(fd, buffer, length);
}

int vendor_write(__attribute__((unused)) struct ipc_client *client, int fd,
		 const void *buffer, size_t length)
{
	if (fd < 0 || buffer == NULL || length == 0 || !vendor_online(fd))
		return -1;

	return write(fd, buffer, length);
}

int vendor_poll(__attribute__((unused)) struct ipc_client *client, int fd,
		struct ipc_poll_fds *fds, struct timeval *timeout)
{
	int ready = 0;
	int rc;

	if (fd < 0)
		return -1;

	rc = ipc_fds_poll(fd, fds, timeout, &ready);
	if (rc < 0)
		return -1;

	/* A modem that crashed wakes the poll; report it */
	if (ready && !vendor_online(fd))
		return -1;

	return rc;
}
