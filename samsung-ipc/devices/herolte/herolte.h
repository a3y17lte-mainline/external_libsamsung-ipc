/*
 * This file is part of libsamsung-ipc.
 *
 * Copyright (C) 2013-2014 Paul Kocialkowski <contact@paulk.fr>
 * Copyright (C) 2017 Wolfgang Wiedmeyer <wolfgit@wiedmeyer.de>
 * Copyright (C) 2020 Tony Garnock-Jones <tonyg@leastfixedpoint.com>
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

#ifndef __HEROLTE_H__
#define __HEROLTE_H__

#include <stddef.h>
#include <sys/time.h>

#include <samsung-ipc.h>

struct herolte_transport_data {
	int fd;
};

int herolte_boot(struct ipc_client *client);
int herolte_open(struct ipc_client *client, void *data, int type);
int herolte_close(struct ipc_client *client, void *data);
int herolte_read(struct ipc_client *client, void *data, void *buffer,
		 size_t length);
int herolte_write(struct ipc_client *client, void *data, const void *buffer,
		  size_t length);
int herolte_poll(struct ipc_client *client, void *data,
		 struct ipc_poll_fds *fds, struct timeval *timeout);
int herolte_power_off(struct ipc_client *client, void *data);

extern struct ipc_client_ops herolte_fmt_ops;
extern struct ipc_client_ops herolte_rfs_ops;
extern struct ipc_client_handlers herolte_handlers;
extern struct ipc_client_gprs_specs herolte_gprs_specs;
extern struct ipc_client_nv_data_specs herolte_nv_data_specs;
extern struct ipc_client_nv_data_specs a3y17lte_nv_data_specs;

#endif /* __HEROLTE_H__ */
