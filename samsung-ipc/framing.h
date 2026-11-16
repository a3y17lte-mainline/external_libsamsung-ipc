/*
 * This file is part of libsamsung-ipc.
 *
 * Copyright (C) 2012 Alexander Tarasikov <alexander.tarasikov@gmail.com>
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

#ifndef __FRAMING_H__
#define __FRAMING_H__

#include <samsung-ipc.h>

/* What one read asks the transport for; longer messages take more reads */
#define SIPC_READ_SIZE		0x1000
/* The longest RFS message accepted (NV data is read and written in parts) */
#define SIPC_RFS_SIZE_LIMIT	0x80000

int sipc_fmt_send(struct ipc_client *client, struct ipc_message *message);
int sipc_fmt_recv(struct ipc_client *client, struct ipc_message *message);
int sipc_rfs_send(struct ipc_client *client, struct ipc_message *message);
int sipc_rfs_recv(struct ipc_client *client, struct ipc_message *message);

#endif /* __FRAMING_H__ */
