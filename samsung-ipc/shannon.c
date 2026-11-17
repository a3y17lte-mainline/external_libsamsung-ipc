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

#include <string.h>

#include <samsung-ipc.h>

/*
 * SIM
 */

int ipc_sec_sim_apps_info_count(const void *data, size_t size)
{
	const struct ipc_sec_sim_apps_info_header *header = data;
	size_t count;

	if (data == NULL || size < sizeof(*header))
		return -1;

	count = header->count;
	if (count > IPC_SEC_SIM_APPS_MAX)
		count = IPC_SEC_SIM_APPS_MAX;

	/* Only the records that are really there */
	if (count > (size - sizeof(*header)) / sizeof(struct ipc_sec_sim_app))
		count = (size - sizeof(*header)) / sizeof(struct ipc_sec_sim_app);

	return (int) count;
}

const struct ipc_sec_sim_app *ipc_sec_sim_apps_info_app(const void *data,
							size_t size,
							unsigned int index)
{
	int count = ipc_sec_sim_apps_info_count(data, size);

	if (count < 0 || index >= (unsigned int) count)
		return NULL;

	return (const struct ipc_sec_sim_app *)
		((const unsigned char *) data +
		 sizeof(struct ipc_sec_sim_apps_info_header) +
		 index * sizeof(struct ipc_sec_sim_app));
}

int ipc_sec_rsim_aid_setup(struct ipc_sec_rsim_aid_request_header *header,
			   unsigned char command, unsigned char app_type,
			   const unsigned char *aid, size_t aid_length,
			   unsigned short file_id,
			   const unsigned short *path, size_t path_length,
			   unsigned char p1, unsigned char p2,
			   unsigned char p3)
{
	size_t i;

	if (header == NULL || aid_length > IPC_SEC_SIM_APP_AID_MAX ||
	    (aid_length && aid == NULL) ||
	    path_length > IPC_SEC_RSIM_PATH_MAX ||
	    (path_length && path == NULL))
		return -1;

	memset(header, 0, sizeof(*header));
	header->command = command;
	header->app_type = app_type;
	header->aid_length = (unsigned char) aid_length;
	if (aid_length)
		memcpy(header->aid, aid, aid_length);
	header->file_id = file_id;
	header->path_length = (unsigned char) path_length;
	for (i = 0; i < path_length; i++)
		header->path[i] = path[i];
	header->p1 = p1;
	header->p2 = p2;
	header->p3 = p3;

	return 0;
}

int ipc_sec_apdu_setup(struct ipc_sec_apdu_request_data *data,
		       const void *apdu, size_t length)
{
	if (data == NULL || apdu == NULL || length == 0 ||
	    length > IPC_SEC_APDU_MAX)
		return -1;

	memset(data, 0, sizeof(*data));
	data->length[0] = (length >> 8) & 0xff;
	data->length[1] = length & 0xff;
	memcpy(data->apdu, apdu, length);

	return 0;
}

size_t ipc_sec_ims_auth_request_size(size_t rand_length, size_t autn_length)
{
	return 1 + 1 + rand_length + 1 + autn_length;
}

int ipc_sec_ims_auth_request_setup(void *buffer, size_t size,
				   unsigned char type,
				   const void *rand, size_t rand_length,
				   const void *autn, size_t autn_length)
{
	unsigned char *p = buffer;

	if (buffer == NULL || rand == NULL || autn == NULL ||
	    rand_length > 0xff || autn_length > 0xff ||
	    size < ipc_sec_ims_auth_request_size(rand_length, autn_length))
		return -1;

	*p++ = type;
	*p++ = (unsigned char) rand_length;
	memcpy(p, rand, rand_length);
	p += rand_length;
	*p++ = (unsigned char) autn_length;
	memcpy(p, autn, autn_length);

	return 0;
}

static unsigned int ipc_sec_ims_auth_value_count(unsigned char type)
{
	switch (type) {
	case 0:
		return 3;
	case 1:
		return 2;
	case 2:
		return 4;
	default:
		return 0;
	}
}

int ipc_sec_ims_auth_response_parse(const void *data, size_t size,
				    struct ipc_sec_ims_auth_result *result)
{
	const struct ipc_sec_ims_auth_response_header *header = data;
	const unsigned char *p, *end;
	unsigned int count, i;

	if (data == NULL || result == NULL || size < sizeof(*header))
		return -1;

	memset(result, 0, sizeof(*result));
	result->type = header->type;
	result->status = header->status;

	/* Failures carry no values */
	if (header->status >= 1 && header->status <= 3)
		return 0;

	count = ipc_sec_ims_auth_value_count(header->type);
	p = (const unsigned char *) data + sizeof(*header);
	end = (const unsigned char *) data + size;

	for (i = 0; i < count; i++) {
		/* A synchronisation failure needs only AUTS, the first value */
		if (p == end && i > 0 && header->status >= 4)
			break;
		if (p >= end || (size_t) (end - p - 1) < *p)
			return -1;

		result->length[i] = *p;
		result->value[i] = p + 1;
		p += 1 + *p;
	}
	result->count = i;

	return 0;
}

/*
 * Data calls
 */

int ipc_net_regist_response_ext_extract(
	const void *data, size_t size,
	struct ipc_net_regist_response_ext_data *out)
{
	if (data == NULL || out == NULL ||
	    size < sizeof(struct ipc_net_regist_response_data))
		return -1;

	/* Shorter forms leave the LTE fields unknown */
	memset(out, 0, sizeof(*out));
	memcpy(out, data, size < sizeof(*out) ? size : sizeof(*out));

	return 0;
}

int ipc_gprs_call_status_ext_extract(const void *data, size_t size,
				     struct ipc_gprs_call_status_ext_data *out)
{
	if (data == NULL || out == NULL ||
	    size < sizeof(struct ipc_gprs_call_status_data))
		return -1;

	memset(out, 0, sizeof(*out));
	memcpy(out, data, size < sizeof(*out) ? size : sizeof(*out));

	return 0;
}

#define IPC_GPRS_IP_CONFIG_IPV4			0x03
#define IPC_GPRS_IP_CONFIG_IPV4_DNS		0x07
#define IPC_GPRS_IP_CONFIG_PCSCF_IPV4		0x17
#define IPC_GPRS_IP_CONFIG_IPV6			0x23
#define IPC_GPRS_IP_CONFIG_PCSCF_IPV6		0x33
#define IPC_GPRS_IP_CONFIG_IPV6_DNS		0x63
#define IPC_GPRS_IP_CONFIG_MTU			0x98
#define IPC_GPRS_IP_CONFIG_PCSCF_IPV4_EXT	0x9d
#define IPC_GPRS_IP_CONFIG_PCSCF_IPV6_EXT	0xb1
#define IPC_GPRS_IP_CONFIG_BASE_SIZE		0x83
#define IPC_GPRS_IP_CONFIG_EXT_SIZE		0x9e
#define IPC_GPRS_PCSCF_BASE			3

static int ipc_gprs_is_zero(const unsigned char *p, size_t length)
{
	while (length--)
		if (*p++)
			return 0;

	return 1;
}

int ipc_gprs_ip_configuration_ext_parse(
	const void *data, size_t size,
	struct ipc_gprs_ip_configuration_ext *config)
{
	const unsigned char *p = data;
	unsigned int i, max;

	if (data == NULL || config == NULL || size < IPC_GPRS_IP_CONFIG_BASE_SIZE)
		return -1;

	memset(config, 0, sizeof(*config));
	config->cid = p[0];
	config->flags = p[1] | (p[2] << 8);

	/*
	 * The stock RIL takes the IPv4 DNS servers with the IPv4 flag, other
	 * clients read bits 1 and 2 as "DNS present". Take every address
	 * field as it is: all zeroes means absent either way.
	 */
	if (config->flags & IPC_GPRS_IP_CONFIGURATION_FLAG_IPV4)
		memcpy(config->ipv4, p + IPC_GPRS_IP_CONFIG_IPV4, 4);
	memcpy(config->ipv4_dns, p + IPC_GPRS_IP_CONFIG_IPV4_DNS, 8);

	/* The IPv6 field holds the interface identifier for SLAAC (fe80::id) */
	memcpy(config->ipv6, p + IPC_GPRS_IP_CONFIG_IPV6, 16);
	memcpy(config->ipv6_dns, p + IPC_GPRS_IP_CONFIG_IPV6_DNS, 32);

	if ((config->flags & IPC_GPRS_IP_CONFIGURATION_FLAG_MTU) &&
	    size >= IPC_GPRS_IP_CONFIG_MTU + 4)
		config->mtu = p[IPC_GPRS_IP_CONFIG_MTU] |
			      (p[IPC_GPRS_IP_CONFIG_MTU + 1] << 8) |
			      (p[IPC_GPRS_IP_CONFIG_MTU + 2] << 16) |
			      ((unsigned int) p[IPC_GPRS_IP_CONFIG_MTU + 3] << 24);

	/* Three P-CSCF addresses of each kind, five more in the long form */
	max = size >= IPC_GPRS_IP_CONFIG_EXT_SIZE ? IPC_GPRS_PCSCF_MAX :
						     IPC_GPRS_PCSCF_BASE;

	for (i = 0; i < max; i++) {
		size_t v4 = i < IPC_GPRS_PCSCF_BASE ?
			IPC_GPRS_IP_CONFIG_PCSCF_IPV4 + 4 * i :
			IPC_GPRS_IP_CONFIG_PCSCF_IPV4_EXT +
				4 * (i - IPC_GPRS_PCSCF_BASE);
		size_t v6 = i < IPC_GPRS_PCSCF_BASE ?
			IPC_GPRS_IP_CONFIG_PCSCF_IPV6 + 16 * i :
			IPC_GPRS_IP_CONFIG_PCSCF_IPV6_EXT +
				16 * (i - IPC_GPRS_PCSCF_BASE);

		if (v4 + 4 <= size && !ipc_gprs_is_zero(p + v4, 4))
			memcpy(config->pcscf_ipv4[config->pcscf_ipv4_count++],
			       p + v4, 4);
		if (v6 + 16 <= size && !ipc_gprs_is_zero(p + v6, 16))
			memcpy(config->pcscf_ipv6[config->pcscf_ipv6_count++],
			       p + v6, 16);
	}

	return 0;
}

int ipc_gprs_set_apn_info_setup(struct ipc_gprs_set_apn_info_data *data,
				unsigned char profile_id, const char *name,
				unsigned char protocol, const char *apn,
				const char *username, const char *password,
				unsigned char auth)
{
	if (data == NULL || name == NULL || apn == NULL ||
	    strlen(name) >= sizeof(data->name) ||
	    strlen(apn) >= sizeof(data->apn) ||
	    (username && strlen(username) >= sizeof(data->username)) ||
	    (password && strlen(password) >= sizeof(data->password)))
		return -1;

	memset(data, 0, sizeof(*data));
	data->profile_id = profile_id;
	strcpy(data->name, name);
	data->protocol = protocol;
	data->protocol_roaming = protocol;
	strcpy(data->apn, apn);
	if (username)
		strcpy(data->username, username);
	if (password)
		strcpy(data->password, password);
	data->auth = auth;

	return 0;
}

/*
 * Calls
 */

size_t ipc_call_list_set_size(unsigned int count, const char *const *numbers)
{
	size_t size = sizeof(struct ipc_call_list_header);
	unsigned int i;

	for (i = 0; i < count; i++)
		size += sizeof(struct ipc_call_list_entry) +
			(numbers && numbers[i] ? strlen(numbers[i]) : 0);

	return size;
}

/*
 * The voice engine
 */

int ipc_ims_channel_setup(struct ipc_ims_channel_data *data,
			  unsigned char channel, unsigned char operation)
{
	if (data == NULL)
		return -1;

	/* Without addresses, every field after the operation is 0xff */
	memset(data, 0xff, sizeof(*data));
	data->channel = channel;
	data->operation = operation;
	memset(data->remote, 0, sizeof(data->remote));
	memset(data->local, 0, sizeof(data->local));
	data->local_family = 0;

	return 0;
}

int ipc_ims_channel_set_addresses(struct ipc_ims_channel_data *data,
				  unsigned char family,
				  const void *local, const void *remote,
				  unsigned short local_rtp_port,
				  unsigned short remote_rtp_port,
				  unsigned short local_rtcp_port,
				  unsigned short remote_rtcp_port)
{
	size_t length;

	if (data == NULL || local == NULL || remote == NULL)
		return -1;

	switch (family) {
	case IPC_IMS_ADDRESS_IPV4:
		length = 4;
		break;
	case IPC_IMS_ADDRESS_IPV6:
		length = 16;
		break;
	default:
		return -1;
	}

	data->local_rtcp_port = local_rtcp_port;
	data->remote_rtcp_port = remote_rtcp_port;
	data->local_rtp_port = local_rtp_port;
	data->remote_rtp_port = remote_rtp_port;
	data->remote_family = family;
	memset(data->remote, 0, sizeof(data->remote));
	memcpy(data->remote, remote, length);
	data->local_family = family;
	memset(data->local, 0, sizeof(data->local));
	memcpy(data->local, local, length);

	return 0;
}

int ipc_ims_pdn_setup(struct ipc_ims_pdn_data *data, unsigned char family,
		      const void *address, const char *ifname,
		      unsigned char cid)
{
	size_t length;

	if (data == NULL || address == NULL || ifname == NULL ||
	    strlen(ifname) >= sizeof(data->ifname))
		return -1;

	switch (family) {
	case IPC_IMS_ADDRESS_IPV4:
		length = 4;
		break;
	case IPC_IMS_ADDRESS_IPV6:
		length = 16;
		break;
	default:
		return -1;
	}

	memset(data, 0, sizeof(*data));
	data->family = family;
	memcpy(data->address, address, length);
	data->ifname_length = (unsigned int) strlen(ifname);
	memcpy(data->ifname, ifname, data->ifname_length);
	data->cid = cid;

	return 0;
}

int ipc_ims_dedicated_bearer_filter_count(const void *data, size_t size)
{
	const struct ipc_ims_dedicated_bearer_filters_header *filters;
	size_t offset = sizeof(struct ipc_ims_dedicated_bearer_header);
	size_t count;

	if (data == NULL || size < offset)
		return -1;

	/* The short form has no filters */
	if (size < offset + sizeof(*filters))
		return 0;

	filters = (const void *) ((const unsigned char *) data + offset);
	count = filters->count;
	if (count > IPC_IMS_BEARER_FILTERS_MAX)
		count = IPC_IMS_BEARER_FILTERS_MAX;
	offset += sizeof(*filters);
	if (count > (size - offset) / sizeof(struct ipc_ims_bearer_filter))
		count = (size - offset) / sizeof(struct ipc_ims_bearer_filter);

	return (int) count;
}
