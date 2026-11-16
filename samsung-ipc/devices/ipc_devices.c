/*
 * This file is part of libsamsung-ipc.
 *
 * Copyright (C) 2012-2014 Paul Kocialkowski <contact@paulk.fr>
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

#include <stdlib.h>

#include <samsung-ipc.h>

#include "devices/ipc_devices.h"

struct ipc_device_desc ipc_devices[] = {
	{
		.name = "herolte",
		.board_name = NULL,
		.kernel_version = NULL,
		.fmt_ops = &herolte_fmt_ops,
		.rfs_ops = &herolte_rfs_ops,
		.handlers = &herolte_handlers,
		.gprs_specs = &herolte_gprs_specs,
		.nv_data_specs = &herolte_nv_data_specs,
	},
	{
		.name = "a3y17lte",
		.board_name = NULL,
		.kernel_version = NULL,
		.fmt_ops = &a3y17lte_fmt_ops,
		.rfs_ops = &a3y17lte_rfs_ops,
		.handlers = &a3y17lte_handlers,
		.gprs_specs = &a3y17lte_gprs_specs,
		.nv_data_specs = &a3y17lte_nv_data_specs,
	},
};

unsigned int ipc_devices_count = sizeof(ipc_devices) /
	sizeof(struct ipc_device_desc);
