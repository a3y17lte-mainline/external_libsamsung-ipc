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

#ifndef __TESTS_DEVICES_H__
#define __TESTS_DEVICES_H__

#include <stdint.h>

struct ipc_client;

int test_device_match(struct ipc_client *client);
int test_device_gprs(struct ipc_client *client);
int test_firmware_toc(struct ipc_client *client);
int test_nv_data_restore(struct ipc_client *client);

/* A firmware image with the A3's TOC and the given NV size */
int test_write_toc(const char *path, uint32_t nv_size, int with_toc);

#endif /* __TESTS_DEVICES_H__ */
