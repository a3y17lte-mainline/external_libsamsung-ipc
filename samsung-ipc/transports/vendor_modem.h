/*
 * Copyright (C) 2010 Google, Inc.
 * Copyright (C) 2010 Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*
 * The parts of Samsung's modem driver interface (drivers/misc/modem_v1 in the
 * vendor kernels, and the mainline driver's legacy front end) that the
 * vendor transport uses.
 */

#ifndef __VENDOR_MODEM_H__
#define __VENDOR_MODEM_H__

#include <stdint.h>
#include <sys/ioctl.h>

#define IOCTL_MODEM_ON			_IO('o', 0x19)
#define IOCTL_MODEM_OFF			_IO('o', 0x20)
#define IOCTL_MODEM_RESET		_IO('o', 0x21)
#define IOCTL_MODEM_BOOT_ON		_IO('o', 0x22)
#define IOCTL_MODEM_BOOT_OFF		_IO('o', 0x23)
#define IOCTL_MODEM_STATUS		_IO('o', 0x27)
#define IOCTL_MODEM_DL_START		_IO('o', 0x28)
#define IOCTL_MODEM_XMIT_BOOT		_IO('o', 0x40)
#define IOCTL_SECURITY_REQ		_IO('o', 0x53)

/* IOCTL_MODEM_STATUS */
enum modem_state {
	STATE_OFFLINE,
	STATE_CRASH_RESET,	/* silent reset */
	STATE_CRASH_EXIT,	/* cp ramdump */
	STATE_BOOTING,
	STATE_ONLINE,
	STATE_NV_REBUILDING,	/* <= rebuilding start */
	STATE_LOADER_DONE,
	STATE_SIM_ATTACH,
	STATE_SIM_DETACH,
};

/*
 * IOCTL_MODEM_XMIT_BOOT: one chunk of a firmware image. The kernel's layout,
 * which has no pointer or long in it, so it is the same for 32 and 64 bit
 * processes.
 */
struct modem_firmware {
	uint64_t binary;	/* the chunk's address in this process */
	uint32_t size;		/* the whole image */
	uint32_t m_offset;	/* offset in the modem's memory */
	uint32_t b_offset;	/* offset in the image */
	uint32_t mode;
	uint32_t len;		/* the chunk */
} __attribute__((__packed__));

/* IOCTL_SECURITY_REQ */
struct modem_sec_req {
	uint32_t mode;
	uint32_t size_boot;
	uint32_t size_main;
	uint32_t pad_zero;
} __attribute__((__packed__));

_Static_assert(sizeof(struct modem_firmware) == 28,
	       "struct modem_firmware must match the kernel's");
_Static_assert(sizeof(struct modem_sec_req) == 16,
	       "struct modem_sec_req must match the kernel's");

#endif /* __VENDOR_MODEM_H__ */
