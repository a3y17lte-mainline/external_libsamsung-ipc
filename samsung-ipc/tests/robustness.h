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

#ifndef __TESTS_ROBUSTNESS_H__
#define __TESTS_ROBUSTNESS_H__

struct ipc_client;

int test_helpers_message_bounds(struct ipc_client *client);
int test_frame_recv(struct ipc_client *client);
int test_frame_send(struct ipc_client *client);
int test_data_read_write(struct ipc_client *client);
int test_fds_poll(struct ipc_client *client);
int test_nv_data(struct ipc_client *client);

#endif /* __TESTS_ROBUSTNESS_H__ */
