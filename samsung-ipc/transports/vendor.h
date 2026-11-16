/*
 * This file is part of libsamsung-ipc.
 *
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

/*
 * The vendor kernel's modem interface: /dev/umts_boot0 to start the modem,
 * /dev/umts_ipc0 and /dev/umts_rfs0 for the messages. Userspace uploads the
 * firmware image and the NV data and runs the boot handshake.
 */

#ifndef __TRANSPORT_VENDOR_H__
#define __TRANSPORT_VENDOR_H__

#include <stddef.h>
#include <sys/time.h>

#include <samsung-ipc.h>

#define VENDOR_BOOT_DEVICE	"/dev/umts_boot0"
#define VENDOR_IPC_DEVICE	"/dev/umts_ipc0"
#define VENDOR_RFS_DEVICE	"/dev/umts_rfs0"

/* Whether the vendor interface is there */
int vendor_available(void);

int vendor_boot(struct ipc_client *client,
		const char *const *firmware_partitions);
int vendor_power_off(struct ipc_client *client);

int vendor_open(struct ipc_client *client, int type);
int vendor_read(struct ipc_client *client, int fd, void *buffer,
		size_t length);
int vendor_write(struct ipc_client *client, int fd, const void *buffer,
		 size_t length);
int vendor_poll(struct ipc_client *client, int fd, struct ipc_poll_fds *fds,
		struct timeval *timeout);

#endif /* __TRANSPORT_VENDOR_H__ */
