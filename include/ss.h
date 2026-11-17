/*
 * This file is part of libsamsung-ipc.
 *
 * Copyright (C) 2010-2011 Joerie de Gram <j.de.gram@gmail.com>
 * Copyright (C) 2012 Paul Kocialkowski <contact@oaulk.fr>
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

#include <samsung-ipc.h>

#ifndef __SAMSUNG_IPC_SS_H__
#define __SAMSUNG_IPC_SS_H__

/*
 * Commands
 */

#define IPC_SS_WAITING						0x0C01
#define IPC_SS_CLI						0x0C02
#define IPC_SS_BARRING						0x0C03
#define IPC_SS_BARRING_PW					0x0C04
#define IPC_SS_FORWARDING					0x0C05
#define IPC_SS_INFO						0x0C06
#define IPC_SS_MANAGE_CALL					0x0C07
#define IPC_SS_USSD						0x0C08
#define IPC_SS_AOC						0x0C09
#define IPC_SS_RELEASE_COMPLETE				0x0C0A

/*
 * Values
 */

#define IPC_SS_USSD_NO_ACTION_REQUIRE				0x01
#define IPC_SS_USSD_ACTION_REQUIRE				0x02
#define IPC_SS_USSD_TERMINATED_BY_NET				0x03
#define IPC_SS_USSD_OTHER_CLIENT				0x04
#define IPC_SS_USSD_NOT_SUPPORT				0x05
#define IPC_SS_USSD_TIME_OUT					0x06

/*
 * Structures
 */

struct ipc_ss_ussd_header {
	unsigned char state;
	unsigned char dcs;
	unsigned char length;
} __attribute__((__packed__));

/*
 * The layouts below are as the stock RIL of the Galaxy A3 2017 builds and
 * reads them (IpcProtocol41::IpcTxSs* and IpcRxSs*).
 */

/* Service classes (ConvertSsClassTypeToIpc); "all" for none given */
#define IPC_SS_CLASS_ALL					0xFF
#define IPC_SS_CLASS_VOICE					0x11
#define IPC_SS_CLASS_DATA					0x12
#define IPC_SS_CLASS_FAX					0x13
#define IPC_SS_CLASS_SMS					0x16

/* Status of a service in an answer */
#define IPC_SS_STATUS_ACTIVE					0x02

/* What a set does */
#define IPC_SS_MODE_REGISTRATION				0x01
#define IPC_SS_MODE_ERASURE					0x02
#define IPC_SS_MODE_ACTIVATE					0x03
#define IPC_SS_MODE_DEACTIVATE					0x04

/* A service and its status for one class, as answers list them */
struct ipc_ss_class_status {
	unsigned char class_type;			/* IPC_SS_CLASS */
	unsigned char status;				/* IPC_SS_STATUS */
} __attribute__((__packed__));

/* IPC_SS_WAITING: get has the class alone */
struct ipc_ss_waiting_set_data {
	unsigned char class_type;			/* IPC_SS_CLASS */
	unsigned char mode;				/* IPC_SS_MODE */
} __attribute__((__packed__));

/* The answer: a count, then that many ipc_ss_class_status */
struct ipc_ss_waiting_response_header {
	unsigned char count;
} __attribute__((__packed__));

/* IPC_SS_CLI: get has the type alone; the answer is type and status */
#define IPC_SS_CLI_TYPE_CLIP					0x01
#define IPC_SS_CLI_TYPE_CLIR					0x02
#define IPC_SS_CLI_TYPE_CNAP					0x06

#define IPC_SS_CLI_CLIR_INVOCATION				0x03
#define IPC_SS_CLI_CLIR_SUPPRESSION				0x04

struct ipc_ss_cli_data {
	unsigned char type;				/* IPC_SS_CLI_TYPE */
	unsigned char status;
} __attribute__((__packed__));

/* IPC_SS_BARRING. Facilities: 1-8 are AO OI OX AI IR AB AG AC */
#define IPC_SS_BARRING_ACB					0x21
#define IPC_SS_BARRING_ICB					0x22

struct ipc_ss_barring_get_data {
	unsigned char facility;
	unsigned char class_type;			/* IPC_SS_CLASS */
} __attribute__((__packed__));

struct ipc_ss_barring_set_data {
	unsigned char facility;
	unsigned char mode;				/* IPC_SS_MODE */
	unsigned char class_type;			/* IPC_SS_CLASS */
	char password[4];
} __attribute__((__packed__));

/* The answer: a byte the stock RIL does not read, a count, then that
 * many ipc_ss_class_status */
struct ipc_ss_barring_response_header {
	unsigned char unknown;
	unsigned char count;
} __attribute__((__packed__));

/* IPC_SS_BARRING_PW: the old password and the new one twice */
struct ipc_ss_barring_pw_data {
	char old_password[4];
	char new_password[4];
	char new_password_again[4];
} __attribute__((__packed__));

/* IPC_SS_FORWARDING. Reasons are Android's plus one */
#define IPC_SS_FORWARDING_NUMBER_MAX				0x51
#define IPC_SS_FORWARDING_ENTRY_MAX				13

struct ipc_ss_forwarding_get_data {
	unsigned char reason;
	unsigned char class_type;			/* IPC_SS_CLASS */
} __attribute__((__packed__));

struct ipc_ss_forwarding_set_data {
	unsigned char mode;				/* IPC_SS_MODE */
	unsigned char reason;
	unsigned char class_type;			/* IPC_SS_CLASS */
	unsigned char no_reply_time;			/* seconds */
	unsigned char number_length;
	unsigned char number_type;			/* 0x91 with a leading +, else 0x81 */
	char number[IPC_SS_FORWARDING_NUMBER_MAX + 1];	/* ASCII, as dialled */
} __attribute__((__packed__));

/* The answer: reason, a count, then that many entries */
struct ipc_ss_forwarding_response_header {
	unsigned char reason;
	unsigned char count;
} __attribute__((__packed__));

#define IPC_SS_FORWARDING_ENTRY_NUMBER_INTL			0x11

struct ipc_ss_forwarding_entry {
	unsigned char class_type;			/* IPC_SS_CLASS */
	unsigned char status;				/* IPC_SS_STATUS */
	unsigned char no_reply_time;
	unsigned char number_length;
	unsigned char number_type;			/* _INTL: a + goes in front */
	char number[82];
} __attribute__((__packed__));

/*
 * IPC_SS_INFO: a notification during a call (3GPP TS 27.007 +CSSI/+CSSU).
 * Codes below 0x31 are about the AP's own call, 0x33-0x36 and 0x61-0x67
 * about a call from the network; a number follows when no_number is 0
 */
struct ipc_ss_info_header {
	unsigned char code;
	unsigned char no_number;
	unsigned char unknown;
	unsigned char index;
	unsigned char number_length;
	unsigned char number_type;
} __attribute__((__packed__));

/*
 * IPC_SS_MANAGE_CALL set: the 3GPP TS 22.030 call handling (+CHLD), on a
 * call by the modem's id where one is named
 */
#define IPC_SS_MANAGE_CALL_RELEASE_HELD_OR_WAITING		0x01	/* CHLD=0 */
#define IPC_SS_MANAGE_CALL_RELEASE_ACTIVE_ACCEPT		0x02	/* CHLD=1 */
#define IPC_SS_MANAGE_CALL_RELEASE_ONE				0x03	/* CHLD=1x */
#define IPC_SS_MANAGE_CALL_SWAP				0x04	/* CHLD=2 */
#define IPC_SS_MANAGE_CALL_SPLIT				0x05	/* CHLD=2x */
#define IPC_SS_MANAGE_CALL_CONFERENCE				0x06	/* CHLD=3 */
#define IPC_SS_MANAGE_CALL_TRANSFER				0x07	/* CHLD=4 */
#define IPC_SS_MANAGE_CALL_DEFLECT				0x08	/* CHLD=4*number */

#define IPC_SS_MANAGE_CALL_NUMBER_MAX				0x52

struct ipc_ss_manage_call_data {
	unsigned char operation;			/* IPC_SS_MANAGE_CALL */
	unsigned char id;
	unsigned char number_length;
	char number[IPC_SS_MANAGE_CALL_NUMBER_MAX];
} __attribute__((__packed__));

/*
 * IPC_SS_USSD exec: send (with the DCS and the packed text) or cancel. A
 * reply to a network question is a response; anything else a request
 */
#define IPC_SS_USSD_SEND					0x01
#define IPC_SS_USSD_CANCEL					0x03

#define IPC_SS_USSD_TYPE_REQUEST				0x01
#define IPC_SS_USSD_TYPE_RESPONSE				0x02

/* The stock RIL caps the length at 182, but its frame holds 181 */
#define IPC_SS_USSD_DATA_MAX					181

struct ipc_ss_ussd_request_data {
	unsigned char action;				/* IPC_SS_USSD_SEND */
	unsigned char type;				/* IPC_SS_USSD_TYPE */
	unsigned char dcs;
	unsigned char length;
	unsigned char data[IPC_SS_USSD_DATA_MAX];
} __attribute__((__packed__));

#endif /*  __SAMSUNG_IPC_SS_H__ */
