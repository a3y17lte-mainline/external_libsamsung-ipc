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

#ifndef __A3Y17LTE_H__
#define __A3Y17LTE_H__

/* Packet data channels 10-24 of the kernel driver */
#define A3Y17LTE_GPRS_IFACE_COUNT	15

extern struct ipc_client_ops a3y17lte_fmt_ops;
extern struct ipc_client_ops a3y17lte_rfs_ops;
extern struct ipc_client_handlers a3y17lte_handlers;
extern struct ipc_client_gprs_specs a3y17lte_gprs_specs;

#endif /* __A3Y17LTE_H__ */
