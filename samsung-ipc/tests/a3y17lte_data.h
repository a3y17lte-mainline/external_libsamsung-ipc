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

#ifndef __TESTS_A3Y17LTE_DATA_H__
#define __TESTS_A3Y17LTE_DATA_H__

struct ipc_client;

int test_shannon_layout_sizes(struct ipc_client *client);
int test_shannon_sim_apps_info(struct ipc_client *client);
int test_shannon_rsim_aid_setup(struct ipc_client *client);
int test_shannon_apdu_setup(struct ipc_client *client);
int test_shannon_ims_auth(struct ipc_client *client);
int test_shannon_ip_configuration(struct ipc_client *client);
int test_shannon_call_status(struct ipc_client *client);
int test_shannon_set_apn_info(struct ipc_client *client);
int test_shannon_ims_channel(struct ipc_client *client);
int test_shannon_ims_pdn(struct ipc_client *client);
int test_shannon_dedicated_bearer(struct ipc_client *client);

#endif /* __TESTS_A3Y17LTE_DATA_H__ */
