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

#include <stddef.h>

#include <samsung-ipc.h>

#include "devices/ipc_devices.h"

/*
 * Where the modem's NV data lives. The default is where the stock software
 * keeps it; a build can point it at a working copy instead, to keep the
 * modem away from the EFS partition.
 */
#ifndef IPC_NV_DATA_DIR
#define IPC_NV_DATA_DIR "/efs"
#endif

#define NV_DATA_SPECS(size)						\
	{								\
		.nv_data_path = IPC_NV_DATA_DIR "/nv_data.bin",		\
		.nv_data_md5_path = IPC_NV_DATA_DIR "/nv_data.bin.md5",	\
		.nv_data_backup_path = IPC_NV_DATA_DIR "/.nv_data.bak",	\
		.nv_data_backup_md5_path =				\
			IPC_NV_DATA_DIR "/.nv_data.bak.md5",		\
		.nv_data_secret = "Samsung_Android_RIL",		\
		.nv_data_size = (size),					\
		.nv_data_chunk_size = 0x1000,				\
	}

/* Galaxy S7 (SM-G930F, Exynos 8890) */
static const char *const herolte_firmware_partitions[] = {
	"/dev/disk/by-partlabel/RADIO",			/* GNU/Linux */
	"/dev/block/platform/155a0000.ufs/by-name/RADIO",	/* LineageOS */
	"/dev/block/by-name/RADIO",			/* Android */
	NULL
};

/* Galaxy A3 2017 (SM-A320F, Exynos 7870, Shannon 315) */
static const char *const a3y17lte_compatibles[] = {
	"samsung,a3y17lte",
	NULL
};

static const char *const a3y17lte_firmware_partitions[] = {
	"/dev/disk/by-partlabel/RADIO",			/* GNU/Linux */
	"/dev/block/by-name/RADIO",			/* Android */
	/* The vendor kernel */
	"/dev/block/platform/13540000.mmc/by-name/RADIO",
	NULL
};

struct ipc_device_desc ipc_devices[] = {
	{
		.name = "herolte",
		/* Its vendor kernel's DT compatibles are not known here */
		.compatibles = NULL,
		.firmware_partitions = herolte_firmware_partitions,
		.gprs_iface_prefix = "rmnet",
		.gprs_iface_count = 3,
		.nv_data_specs = NV_DATA_SPECS(0x200000),
	},
	{
		.name = "a3y17lte",
		.compatibles = a3y17lte_compatibles,
		.firmware_partitions = a3y17lte_firmware_partitions,
		/* The mainline kernel's packet data channels 10-24 */
		.gprs_iface_prefix = "rmnet",
		.gprs_iface_count = 15,
		.nv_data_specs = NV_DATA_SPECS(0x100000),
	},
};

unsigned int ipc_devices_count = sizeof(ipc_devices) /
	sizeof(struct ipc_device_desc);
