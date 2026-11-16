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

#ifndef __XMM626_H__
#define __XMM626_H__

#define XMM626_DATA_SIZE				0x1000
#define XMM626_DATA_SIZE_LIMIT				0x80000

#define XMM626_NV_DATA_PATH				"/efs/nv_data.bin"
#define XMM626_NV_DATA_MD5_PATH			"/efs/nv_data.bin.md5"
#define XMM626_NV_DATA_BACKUP_PATH			"/efs/.nv_data.bak"
#define XMM626_NV_DATA_BACKUP_MD5_PATH			"/efs/.nv_data.bak.md5"
#define XMM626_NV_DATA_SECRET				"Samsung_Android_RIL"
#define XMM626_NV_DATA_SIZE				0x200000
#define XMM626_NV_DATA_CHUNK_SIZE			0x1000

#endif /* __XMM626_H__ */
