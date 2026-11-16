/*
 * This file is part of libsamsung-ipc.
 *
 * Copyright (C) 2013-2014 Paul Kocialkowski <contact@paulk.fr>
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

#ifndef __XMM626_SEC_MODEM_H__
#define __XMM626_SEC_MODEM_H__

#define XMM626_SEC_MODEM_BOOT0_DEVICE		"/dev/umts_boot0"
#define XMM626_SEC_MODEM_IPC0_DEVICE		"/dev/umts_ipc0"
#define XMM626_SEC_MODEM_RFS0_DEVICE		"/dev/umts_rfs0"

#define XMM626_SEC_MODEM_GPRS_IFACE_PREFIX	"rmnet"
#define XMM626_SEC_MODEM_GPRS_IFACE_COUNT	3

int xmm626_kernel_smdk4412_power(struct ipc_client *client, int device_fd,
				 int power);
int xmm626_kernel_smdk4412_boot_power(struct ipc_client *client, int device_fd,
				      int power);

int xmm626_kernel_smdk4412_fmt_send(struct ipc_client *client,
				    struct ipc_message *message);
int xmm626_kernel_smdk4412_fmt_recv(struct ipc_client *client,
				    struct ipc_message *message);
int xmm626_kernel_smdk4412_rfs_send(struct ipc_client *client,
				    struct ipc_message *message);
int xmm626_kernel_smdk4412_rfs_recv(struct ipc_client *client,
				    struct ipc_message *message);

int xmm626_kernel_smdk4412_open(struct ipc_client *client, int type);
int xmm626_kernel_smdk4412_close(struct ipc_client *client, int fd);
int xmm626_kernel_smdk4412_read(struct ipc_client *client, int fd, void *buffer,
				size_t length);
int xmm626_kernel_smdk4412_write(struct ipc_client *client, int fd,
				 const void *buffer, size_t length);
int xmm626_kernel_smdk4412_poll(struct ipc_client *client, int fd,
				struct ipc_poll_fds *fds,
				struct timeval *timeout);

char *xmm626_kernel_smdk4412_gprs_get_iface(struct ipc_client *client,
					    unsigned int cid);
int xmm626_kernel_smdk4412_gprs_get_capabilities(
	struct ipc_client *client,
	struct ipc_client_gprs_capabilities *capabilities);

#endif /* __XMM626_SEC_MODEM_H__ */
