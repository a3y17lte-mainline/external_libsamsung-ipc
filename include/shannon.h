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
 * Messages of the Shannon (LTE) modems that the older headers do not
 * describe: SIM applications and authentication for IMS, the full IP
 * configuration of a data call (IPv6, P-CSCF), data profiles, IMS state and
 * the modem's voice engine for VoLTE (group 0x16).
 *
 * Layouts were worked out on the Galaxy A3 2017 (SM-A320F, Shannon 315).
 * Offsets in the comments count from the start of the payload, after the
 * 7-byte IPC header. Fields marked "unknown" are sent as zero.
 */

#include <stddef.h>

#ifndef __SAMSUNG_IPC_SHANNON_H__
#define __SAMSUNG_IPC_SHANNON_H__

/*
 * Commands
 */

#define IPC_SEC_SIM_ICCID					0x050C
#define IPC_SEC_SIM_APPS_INFO					0x050F
#define IPC_CALL_SRVCC						0x0214
#define IPC_NET_SSAC_INFO					0x0819

#define IPC_GROUP_IMS						0x16
#define IPC_IMS_CHANNEL						0x1601
#define IPC_IMS_CODEC						0x1602
#define IPC_IMS_RTP_MEDIA					0x1603
#define IPC_IMS_DTMF						0x1604
#define IPC_IMS_OPTION						0x1605
#define IPC_IMS_PDN						0x1606
#define IPC_IMS_SESSION_REFRESH					0x1607
#define IPC_IMS_REG_STATE					0x1608
#define IPC_IMS_ENGINE						0x1609
#define IPC_IMS_FRAME_TIME					0x160A
#define IPC_IMS_DEDICATED_BEARER				0x160B
#define IPC_IMS_INFO						0x160C
#define IPC_IMS_RRC_CONNECTION					0x160E
#define IPC_IMS_SEND_IPC					0x160F
#define IPC_IMS_MODE_CHANGE					0x1610
#define IPC_IMS_SIP_INFO_ACB					0x1611
#define IPC_IMS_AVOID_TUNE_AWAY					0x1612
#define IPC_IMS_GET_NET_INFO					0x1618
#define IPC_IMS_SIGNALLING					0x1619
#define IPC_IMS_GET_VERSION					0x161B
#define IPC_IMS_DELETE_CP_CLIENT_SOCKET				0x161C
#define IPC_IMS_SIM_INFO					0x1620
#define IPC_IMS_GET_RADIO_ENVIRONMENT_INFO			0x1621

/* The voice engine acknowledges requests with IPC_GEN_PHONE_RES, code: */
#define IPC_IMS_RESULT_SUCCESS					0x8000

/*
 * SIM applications (IPC_SEC_SIM_APPS_INFO)
 */

#define IPC_SEC_SIM_APPS_MAX					10
#define IPC_SEC_SIM_APP_AID_MAX					16

struct ipc_sec_sim_apps_info_header {
	unsigned char count;
} __attribute__((__packed__));

struct ipc_sec_sim_app {
	unsigned char type;		/* card application type */
	unsigned char aid_length;
	unsigned char aid[IPC_SEC_SIM_APP_AID_MAX];
	unsigned char pin1_state;
	unsigned char pin2_state;
	unsigned char counters[4];	/* remaining PIN/PUK attempts */
} __attribute__((__packed__));

/*
 * SIM file access with an application id (IPC_SEC_RSIM_ACCESS on Shannon)
 *
 * The path is a list of 16-bit file ids, each stored little endian.
 */

#define IPC_SEC_RSIM_PATH_MAX					6

struct ipc_sec_rsim_aid_request_header {
	unsigned char command;		/* IPC_SEC_RSIM_COMMAND */
	unsigned char app_type;
	unsigned char aid_length;
	unsigned char aid[IPC_SEC_SIM_APP_AID_MAX];
	unsigned short file_id;
	unsigned char path_length;	/* in file ids */
	unsigned short path[IPC_SEC_RSIM_PATH_MAX];
	unsigned char p1, p2, p3;
} __attribute__((__packed__));

/*
 * Raw APDUs on logical channels (IPC_SEC_GSIM_ACCESS on Shannon)
 *
 * Every request is the same size. Opening a channel is MANAGE CHANNEL
 * (00 70 00 00 01), closing it 00 70 80 <channel> 00; other commands carry
 * the channel number in the low bits of the class byte.
 */

#define IPC_SEC_APDU_MAX					0x105

struct ipc_sec_apdu_request_data {
	unsigned char length[2];	/* big endian */
	unsigned char apdu[IPC_SEC_APDU_MAX];
} __attribute__((__packed__));

/*
 * Authentication with the SIM's AKA algorithm (IPC_SEC_IMS_AUTH)
 *
 * Request: type, RAND, AUTN, each value with a length byte in front.
 * Response: type, status and the results as length-prefixed values: three
 * for type 0 (RES, CK, IK), two for type 1, four for type 2. Status 0 is
 * success; 4 and 5 also carry values (resynchronisation), 1 to 3 are
 * failures.
 */

#define IPC_SEC_IMS_AUTH_TYPE_AKA				0x00
#define IPC_SEC_IMS_AUTH_VALUES_MAX				4

struct ipc_sec_ims_auth_response_header {
	unsigned char type;
	unsigned char status;
} __attribute__((__packed__));

struct ipc_sec_ims_auth_result {
	unsigned char type;
	unsigned char status;
	unsigned int count;
	const unsigned char *value[IPC_SEC_IMS_AUTH_VALUES_MAX];
	size_t length[IPC_SEC_IMS_AUTH_VALUES_MAX];
};

/*
 * Data calls
 */

/* IPC_GPRS_CALL_STATUS as Shannon sends it */
struct ipc_gprs_call_status_ext_data {
	unsigned char cid;
	unsigned char status;		/* IPC_GPRS_STATUS */
	unsigned char fail_cause;	/* IPC_GPRS_FAIL_CAUSE */
	unsigned char external;
	unsigned char dedicated_bearer_changed;
	unsigned int retry_time;	/* ms, 0 if none */
} __attribute__((__packed__));

/* IPC_GPRS_IP_CONFIGURATION, the full form */
#define IPC_GPRS_IP_CONFIGURATION_FLAG_IPV4			0x0001
#define IPC_GPRS_IP_CONFIGURATION_FLAG_IPV6			0x0040
#define IPC_GPRS_IP_CONFIGURATION_FLAG_MTU			0x2000
#define IPC_GPRS_IP_CONFIGURATION_FLAG_UNKNOWN			0x4000

#define IPC_GPRS_PCSCF_MAX					8

struct ipc_gprs_ip_configuration_ext {
	unsigned char cid;
	unsigned short flags;
	unsigned char ipv4[4];
	unsigned char ipv4_dns[2][4];
	unsigned char ipv6[16];		/* fe80:: + interface id */
	unsigned char ipv6_dns[2][16];
	unsigned int pcscf_ipv4_count;
	unsigned char pcscf_ipv4[IPC_GPRS_PCSCF_MAX][4];
	unsigned int pcscf_ipv6_count;
	unsigned char pcscf_ipv6[IPC_GPRS_PCSCF_MAX][16];
	unsigned int mtu;		/* 0 if not given */
};

/* IPC_GPRS_SET_APN_INFO: one data profile */
#define IPC_GPRS_PROFILE_NAME_MAX				20

struct ipc_gprs_set_apn_info_data {
	unsigned char profile_id;
	char name[IPC_GPRS_PROFILE_NAME_MAX];	/* "lte_internet", "lte_ims" */
	unsigned char protocol;		/* IPC_GPRS_PROTOCOL */
	char apn[101];
	char username[32];
	char password[32];
	unsigned char auth;
	unsigned char protocol_roaming;	/* IPC_GPRS_PROTOCOL */
	unsigned char disabled;
	unsigned short unknown[3];
} __attribute__((__packed__));

/*
 * IMS state and calls
 */

/* IPC_IMS_REG_STATE: the AP's IMS registration, for the modem */
struct ipc_ims_reg_state_data {
	unsigned char status;
	unsigned char features;
	unsigned char network_type;
	unsigned char ecmp;
	unsigned char rat;
	unsigned char unknown;
} __attribute__((__packed__));

/* IPC_CALL_SRVCC, both directions */
#define IPC_CALL_SRVCC_STARTED					0x00
#define IPC_CALL_SRVCC_COMPLETED				0x01
#define IPC_CALL_SRVCC_FAILED					0x02

struct ipc_call_srvcc_data {
	unsigned char state;
} __attribute__((__packed__));

/*
 * The modem's voice engine (group 0x16)
 */

#define IPC_IMS_CHANNEL_CREATE					0x01
#define IPC_IMS_CHANNEL_DELETE					0x02
#define IPC_IMS_CHANNEL_CHANGE					0x03

#define IPC_IMS_ADDRESS_IPV4					0x01
#define IPC_IMS_ADDRESS_IPV6					0x02

struct ipc_ims_channel_data {
	unsigned char channel;
	unsigned char operation;	/* IPC_IMS_CHANNEL_* */
	unsigned short local_rtcp_port;
	unsigned short remote_rtcp_port;
	unsigned short local_rtp_port;
	unsigned short remote_rtp_port;
	unsigned char remote_family;	/* IPC_IMS_ADDRESS_* */
	unsigned char remote[16];
	unsigned char local_family;
	unsigned char local[16];
	unsigned char stream_type;
} __attribute__((__packed__));

struct ipc_ims_codec_data {
	unsigned char channel;
	unsigned int changed;		/* mask of the settings below */
	unsigned char codec;
	unsigned char payload_type;
	unsigned short frequency;
	unsigned int bitrate;
	unsigned char ptime;
	unsigned char max_rx_ptime;
	unsigned char channels;
	unsigned char fixed_rate;
	unsigned char packetization;	/* octet-aligned or bandwidth-efficient */
	unsigned char unknown1[4];
	unsigned char dtx;
	unsigned char red_level;
	unsigned char red_payload_type;
	unsigned char unknown2;
	unsigned short mode_set;
	unsigned char unknown3;
	unsigned char evs_dtx_recv;
	unsigned char evs_ch_aware_recv;
	unsigned char evs_mode_switch;
	unsigned char evs_ch_send;
	unsigned char evs_ch_recv;
	unsigned char evs_cmr;
	unsigned int evs_bitrate_send_min;
	unsigned int evs_bitrate_send_max;
	unsigned int evs_bitrate_recv_min;
	unsigned int evs_bitrate_recv_max;
	unsigned char evs_bw_send;
	unsigned char evs_bw_recv;
	unsigned int evs_default_bitrate;
	unsigned char evs_default_bw;
} __attribute__((__packed__));

struct ipc_ims_rtp_media_data {
	unsigned char operation;
	unsigned char channel;
	unsigned char status;
	unsigned char unknown;
} __attribute__((__packed__));

struct ipc_ims_pdn_data {
	unsigned char family;		/* IPC_IMS_ADDRESS_* */
	unsigned char address[16];
	unsigned int ifname_length;
	char ifname[16];
	unsigned char cid;
} __attribute__((__packed__));

struct ipc_ims_engine_data {
	unsigned char engine;
	unsigned char state;
} __attribute__((__packed__));

/* Notifications */
struct ipc_ims_media_event_header {
	unsigned char event;
	unsigned char channel;
} __attribute__((__packed__));

#define IPC_IMS_BEARER_ADDED					0x02
#define IPC_IMS_BEARER_RELEASED					0x03
#define IPC_IMS_BEARER_FILTERS_MAX				16

struct ipc_ims_bearer_filter {
	unsigned char raw[26];
} __attribute__((__packed__));

struct ipc_ims_dedicated_bearer_header {
	unsigned char bearer_id;
	unsigned char qci;
	unsigned char event;		/* IPC_IMS_BEARER_* */
	unsigned long long bitrate[2];	/* the log names the first gbr_dl */
} __attribute__((__packed__));

struct ipc_ims_dedicated_bearer_filters_header {
	unsigned char unknown[2];
	unsigned char count;
} __attribute__((__packed__));

struct ipc_ims_rrc_connection_data {
	unsigned char event;
} __attribute__((__packed__));

/*
 * Helpers
 */

int ipc_sec_sim_apps_info_count(const void *data, size_t size);
const struct ipc_sec_sim_app *ipc_sec_sim_apps_info_app(const void *data,
							size_t size,
							unsigned int index);

int ipc_sec_rsim_aid_setup(struct ipc_sec_rsim_aid_request_header *header,
			   unsigned char command, unsigned char app_type,
			   const unsigned char *aid, size_t aid_length,
			   unsigned short file_id,
			   const unsigned short *path, size_t path_length,
			   unsigned char p1, unsigned char p2,
			   unsigned char p3);

int ipc_sec_apdu_setup(struct ipc_sec_apdu_request_data *data,
		       const void *apdu, size_t length);

size_t ipc_sec_ims_auth_request_size(size_t rand_length, size_t autn_length);
int ipc_sec_ims_auth_request_setup(void *buffer, size_t size,
				   unsigned char type,
				   const void *rand, size_t rand_length,
				   const void *autn, size_t autn_length);
int ipc_sec_ims_auth_response_parse(const void *data, size_t size,
				    struct ipc_sec_ims_auth_result *result);

int ipc_gprs_call_status_ext_extract(const void *data, size_t size,
				     struct ipc_gprs_call_status_ext_data *out);
int ipc_gprs_ip_configuration_ext_parse(
	const void *data, size_t size,
	struct ipc_gprs_ip_configuration_ext *config);
int ipc_gprs_set_apn_info_setup(struct ipc_gprs_set_apn_info_data *data,
				unsigned char profile_id, const char *name,
				unsigned char protocol, const char *apn,
				const char *username, const char *password,
				unsigned char auth);

size_t ipc_call_list_set_size(unsigned int count, const char *const *numbers);

int ipc_ims_channel_setup(struct ipc_ims_channel_data *data,
			  unsigned char channel, unsigned char operation);
int ipc_ims_channel_set_addresses(struct ipc_ims_channel_data *data,
				  unsigned char family,
				  const void *local, const void *remote,
				  unsigned short local_rtp_port,
				  unsigned short remote_rtp_port,
				  unsigned short local_rtcp_port,
				  unsigned short remote_rtcp_port);
int ipc_ims_pdn_setup(struct ipc_ims_pdn_data *data, unsigned char family,
		      const void *address, const char *ifname,
		      unsigned char cid);
int ipc_ims_dedicated_bearer_filter_count(const void *data, size_t size);

#endif /* __SAMSUNG_IPC_SHANNON_H__ */
