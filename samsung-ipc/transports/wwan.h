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
 * The mainline kernel's WWAN ports: Samsung IPC (type "SIPC") and the
 * remote file system channel (type "SIPC-RFS"). A read returns one message,
 * a write sends one, and the kernel loads and starts the modem when the IPC
 * port is first opened. Packet data contexts are WWAN links, which userspace
 * creates:
 *   ip link add rmnet0 parentdev wwan0 type wwan linkid 0
 */

#ifndef __TRANSPORT_WWAN_H__
#define __TRANSPORT_WWAN_H__

#include <stddef.h>

#include <samsung-ipc.h>

#define WWAN_PORT_CLASS		"/sys/class/wwan"
#define WWAN_PORT_TYPE_IPC	"SIPC"
#define WWAN_PORT_TYPE_RFS	"SIPC-RFS"

/* Whether the kernel has a Samsung IPC port */
int wwan_available(void);

/* Opens the port for the client type; opening the IPC port starts the modem */
int wwan_open(struct ipc_client *client, int type);

#endif /* __TRANSPORT_WWAN_H__ */
