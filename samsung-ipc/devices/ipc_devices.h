/*
 * This file is part of libsamsung-ipc.
 *
 * Copyright (C) 2013-2014 Paul Kocialkowski <contact@paulk.fr>
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

#ifndef __IPC_DEVICES_H__
#define __IPC_DEVICES_H__

#include <samsung-ipc.h>

#include "ipc.h"

/*
 * A phone with a Shannon modem. Everything that differs between phones is
 * data: the code (devices/shannon.c) is shared and picks the transport at
 * run time, the mainline kernel's WWAN ports if they are there, the vendor
 * kernel's /dev/umts_* otherwise.
 */
struct ipc_device_desc {
	const char *name;
	/* Device tree compatibles that identify it; NULL: by name only */
	const char *const *compatibles;
	/* The firmware image (RADIO), for the vendor kernel's boot */
	const char *const *firmware_partitions;
	/* Packet data interfaces: <prefix><cid - 1> */
	const char *gprs_iface_prefix;
	unsigned int gprs_iface_count;
	struct ipc_client_nv_data_specs nv_data_specs;
};

extern struct ipc_device_desc ipc_devices[];
extern unsigned int ipc_devices_count;

extern struct ipc_client_ops shannon_fmt_ops;
extern struct ipc_client_ops shannon_rfs_ops;
extern struct ipc_client_handlers shannon_handlers;
extern struct ipc_client_gprs_specs shannon_gprs_specs;

#endif /* __IPC_DEVICES_H__ */
