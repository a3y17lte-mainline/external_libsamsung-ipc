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

#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <asm/types.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

/* linux/netlink.h needs to be included after sys/socket.h */
#include <linux/netlink.h>

#include <samsung-ipc.h>

#include "ipc.h"

ssize_t data_read(__attribute__((unused)) struct ipc_client *client, int fd,
		  void *buf, size_t count)
{
	unsigned char *p = buf;
	size_t done = 0;

	/* From read(2): "According  to POSIX.1, if count is greater than
	 * SSIZE_MAX, the result is implementation-defined"
	 */
	if (buf == NULL || count > SSIZE_MAX) {
		errno = EINVAL;
		return -1;
	}

	while (done < count) {
		ssize_t rc;

		rc = read(fd, p + done, count - done);
		if (rc == -1) {
			if (errno == EINTR)
				continue;
			/* errno is passed to the caller */
			return rc;
		}
		if (rc == 0) {
			/* The end of the file before count bytes */
			errno = ENODATA;
			return -1;
		}
		done += rc;
	}

	return (ssize_t) count;
}

ssize_t data_write(__attribute__((unused)) struct ipc_client *client, int fd,
		   const void *buf, size_t count)
{
	const unsigned char *p = buf;
	size_t done = 0;

	/* From write(2): "According  to POSIX.1, if count is greater than
	 * SSIZE_MAX, the result is implementation-defined"
	 */
	if (buf == NULL || count > SSIZE_MAX) {
		errno = EINVAL;
		return -1;
	}

	while (done < count) {
		ssize_t rc;

		rc = write(fd, p + done, count - done);
		if (rc == -1) {
			if (errno == EINTR)
				continue;
			/* errno is passed to the caller */
			return rc;
		}
		if (rc == 0) {
			errno = EIO;
			return -1;
		}
		done += rc;
	}

	return (ssize_t) count;
}

void *file_data_read(struct ipc_client *client, const char *path, size_t size,
		     size_t chunk_size, unsigned int offset)
{
	void *data = NULL;
	int fd = -1;
	size_t count;
	off_t seek;
	unsigned char *p;
	int rc;

	if (path == NULL || size == 0 || chunk_size == 0 || chunk_size > size) {
		if (path == NULL) {
			ipc_client_log(client, "%s: Failed: path is NULL",
				       __func__);
		}
		if (size == 0) {
			ipc_client_log(client, "%s: Failed: size is 0",
				       __func__);
		}
		if (chunk_size == 0) {
			ipc_client_log(client, "%s: Failed: chunk_size is 0",
				       __func__);
		}
		if (chunk_size > size) {
			ipc_client_log(client, "%s: Failed: chunk_size > size ",
				       __func__);
		}

		return NULL;
	}

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		rc = errno;
		ipc_client_log(client, "%s open failed with error %d: %s", __func__, rc,
			       strerror(rc));
		goto error;
	}

	seek = lseek(fd, (off_t) offset, SEEK_SET);
	if (seek < (off_t) offset) {
		ipc_client_log(client, "%s: Error: seek < (off_t) offset",
			       __func__);
		goto error;
	}

	data = calloc(1, size);
	if (data == NULL)
		goto error;

	p = (unsigned char *) data;

	count = 0;
	while (count < size) {
		rc = read(fd, p,
			  size - count > chunk_size ?
			  chunk_size : size - count);
		if (rc == -1) {
			rc = errno;
			ipc_client_log(client, "%s: read error: %d: %s",
				       __func__, rc, strerror(rc));
			goto error;
		} else if (rc == 0) {
			ipc_client_log(client, "%s: read error: end of file",
				       __func__);
			goto error;
		}

		p += rc;
		count += rc;
	}

	goto complete;

error:
	if (data != NULL) {
		free(data);
		data = NULL;
	}

complete:
	if (fd >= 0)
		close(fd);

	return data;
}

int file_data_write(struct ipc_client *client, const char *path,
		    const void *data, size_t size, size_t chunk_size,
		    unsigned int offset)
{
	int fd = -1;
	size_t count;
	off_t seek;
	unsigned char *p;
	int rc;

	if (path == NULL || data == NULL || size == 0 || chunk_size == 0 ||
	    chunk_size > size) {
		if (path == NULL) {
			ipc_client_log(client, "%s failed: path is NULL",
				       __func__);
		}
		if (size == 0) {
			ipc_client_log(client, "%s failed: size is 0",
				       __func__);
		}
		if (chunk_size == 0) {
			ipc_client_log(client, "%s failed: chunk_size is 0",
				       __func__);
		}
		if (chunk_size > size) {
			ipc_client_log(client, "%s failed: chunk_size > size",
				       __func__);
		}
		errno = EINVAL;
		return -1;
	}

	fd = open(path, O_WRONLY | O_CREAT, 0644);
	if (fd < 0) {
		rc = errno;
		ipc_client_log(client, "%s open failed with error %d: %s",
			       __func__, rc, strerror(rc));
		goto error;
	}

	seek = lseek(fd, (off_t) offset, SEEK_SET);
	if (seek < (off_t) offset) {
		rc = seek < 0 ? errno : EIO;
		ipc_client_log(client, "%s failed: seek < (off_t) offset",
			       __func__);
		goto error;
	}

	p = (unsigned char *) data;

	count = 0;
	while (count < size) {
		rc = write(fd, p,
			   size - count > chunk_size ?
			   chunk_size : size - count);
		if (rc <= 0) {
			rc = rc < 0 ? errno : EIO;
			ipc_client_log(client, "%s: write failed with error %d: %s",
				       __func__, rc, strerror(rc));
			goto error;
		}

		p += rc;
		count += rc;
	}

	/* NV data: on the disk before the caller goes on */
	fsync(fd);
	close(fd);

	return 0;

error:
	if (fd >= 0)
		close(fd);

	errno = rc;
	return -1;
}

int file_data_replace(struct ipc_client *client, const char *path,
		      const void *data, size_t size, size_t chunk_size)
{
	char *tmp_path = NULL;
	char *dir;
	char *slash;
	int fd;

	if (path == NULL || asprintf(&tmp_path, "%s.tmp", path) < 0)
		return -1;

	unlink(tmp_path);

	if (file_data_write(client, tmp_path, data, size, chunk_size, 0) < 0)
		goto error;

	if (rename(tmp_path, path) < 0) {
		ipc_client_log(client, "%s: renaming %s failed: %s", __func__,
			       tmp_path, strerror(errno));
		goto error;
	}

	/* The rename itself on the disk too */
	dir = strdup(path);
	if (dir != NULL) {
		slash = strrchr(dir, '/');
		if (slash != NULL) {
			*(slash == dir ? slash + 1 : slash) = '\0';
			fd = open(dir, O_RDONLY | O_DIRECTORY);
			if (fd >= 0) {
				fsync(fd);
				close(fd);
			}
		}
		free(dir);
	}

	free(tmp_path);

	return 0;

error:
	unlink(tmp_path);
	free(tmp_path);

	return -1;
}

off_t file_data_size(struct ipc_client *client, const char *path)
{
	int fd = -1;
	off_t rc = 0;
	int err = 0;

	if (path == NULL) {
		ipc_client_log(client, "%s: Failed: path is NULL",
			       __func__);
		err = ENOENT;
		goto error;
	}

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		err = errno;
		ipc_client_log(client, "%s: open %s failed with error %d: %s",
			       __func__, path, err, strerror(err));
		goto error;
	}

	rc = lseek(fd, 0, SEEK_END);
	if (rc == -1) {
		err = errno;
		ipc_client_log(client, "%s: seek %s failed with error %d: %s",
			       __func__, path, err, strerror(err));
		goto error;
	}

error:
	if (fd >= 0)
		close(fd);

	if (err) {
		errno = err;
		return -1;
	} else {
		return rc;
	}
}

int ipc_fds_poll(int fd, struct ipc_poll_fds *fds, struct timeval *timeout,
		 int *ready)
{
	const short events = POLLIN | POLLHUP | POLLERR;
	struct pollfd *pollfds;
	unsigned int count = 0;
	unsigned int ready_count;
	unsigned int i;
	int timeout_ms = -1;
	int rc;

	if (fd < 0)
		return -1;

	if (fds != NULL && fds->fds != NULL)
		count = fds->count;

	pollfds = calloc(count + 1, sizeof(struct pollfd));
	if (pollfds == NULL)
		return -1;

	pollfds[0].fd = fd;
	pollfds[0].events = POLLIN;
	for (i = 0; i < count; i++) {
		/* poll() skips negative descriptors */
		pollfds[i + 1].fd = fds->fds[i];
		pollfds[i + 1].events = POLLIN;
	}

	if (timeout != NULL) {
		long long ms = (long long) timeout->tv_sec * 1000 +
			       (timeout->tv_usec + 999) / 1000;

		timeout_ms = ms > INT_MAX ? INT_MAX : (int) ms;
	}

	do {
		rc = poll(pollfds, count + 1, timeout_ms);
	} while (rc < 0 && errno == EINTR);

	if (rc < 0) {
		free(pollfds);
		return -1;
	}

	if (ready != NULL)
		*ready = !!(pollfds[0].revents & events);

	ready_count = 0;
	for (i = 0; i < count; i++) {
		if (fds->fds[i] >= 0 && (pollfds[i + 1].revents & events))
			ready_count++;
		else
			fds->fds[i] = -1;
	}

	if (count > 0)
		fds->count = ready_count;

	free(pollfds);

	return rc;
}

int network_iface_up(const char *iface, int domain, int type)
{
	struct ifreq ifr;
	int fd = -1;
	int rc;

	if (iface == NULL)
		return -1;

	if (strlen(iface) >= IFNAMSIZ)
		return -1;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);

	fd = socket(domain, type, 0);
	if (fd < 0)
		goto error;

	rc = ioctl(fd, SIOCGIFFLAGS, &ifr);
	if (rc < 0)
		goto error;

	ifr.ifr_flags |= IFF_UP;

	rc = ioctl(fd, SIOCSIFFLAGS, &ifr);
	if (rc < 0)
		goto error;

	rc = 0;
	goto complete;

error:
	rc = -1;

complete:
	if (fd >= 0)
		close(fd);

	return rc;
}

int network_iface_down(const char *iface, int domain, int type)
{
	struct ifreq ifr;
	int fd = -1;
	int rc;

	if (iface == NULL)
		return -1;

	if (strlen(iface) >= IFNAMSIZ)
		return -1;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);

	fd = socket(domain, type, 0);
	if (fd < 0)
		goto error;

	rc = ioctl(fd, SIOCGIFFLAGS, &ifr);
	if (rc < 0)
		goto error;

	ifr.ifr_flags = (ifr.ifr_flags & (~IFF_UP));

	rc = ioctl(fd, SIOCSIFFLAGS, &ifr);
	if (rc < 0)
		goto error;

	rc = 0;
	goto complete;

error:
	rc = -1;

complete:
	if (fd >= 0)
		close(fd);

	return rc;
}

int sysfs_value_read(const char *path)
{
	char buffer[100];
	int value;
	int fd = -1;
	int rc;

	if (path == NULL)
		return -1;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		goto error;

	rc = read(fd, &buffer, sizeof(buffer) - 1);
	if (rc <= 0)
		goto error;

	buffer[rc] = '\0';

	value = atoi(buffer);
	goto complete;

error:
	value = -1;

complete:
	if (fd >= 0)
		close(fd);

	return value;
}

int sysfs_value_write(const char *path, int value)
{
	char buffer[100];
	int fd = -1;
	int rc;

	if (path == NULL)
		return -1;

	fd = open(path, O_WRONLY);
	if (fd < 0)
		goto error;

	snprintf((char *) &buffer, sizeof(buffer), "%d\n", value);

	rc = write(fd, buffer, strlen(buffer));
	if (rc < (int) strlen(buffer))
		goto error;

	rc = 0;
	goto complete;

error:
	rc = -1;

complete:
	if (fd >= 0)
		close(fd);

	return rc;
}

char *sysfs_string_read(const char *path, size_t length)
{
	char *string = NULL;
	int fd = -1;
	int rc;

	if (path == NULL || length == 0)
		return NULL;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		goto error;

	/* One more byte, so that the string always ends */
	string = (char *) calloc(1, length + 1);
	if (string == NULL)
		goto error;

	rc = read(fd, string, length);
	if (rc <= 0)
		goto error;

	goto complete;

error:
	if (string != NULL) {
		free(string);
		string = NULL;
	}

complete:
	if (fd >= 0)
		close(fd);

	return string;
}

int sysfs_string_write(const char *path, const char *buffer, size_t length)
{
	int fd = -1;
	int rc;

	if (path == NULL || buffer == NULL || length == 0)
		return -1;

	fd = open(path, O_WRONLY);
	if (fd < 0)
		goto error;

	rc = write(fd, buffer, length);
	if (rc < (int) length)
		goto error;

	rc = 0;
	goto complete;

error:
	rc = -1;

complete:
	if (fd >= 0)
		close(fd);

	return rc;
}

size_t data2string_length(const void *data, size_t size)
{
	size_t length;

	if (data == NULL || size == 0)
		return 0;

	length = size * 2 + 1;

	return length;
}

char *data2string(const void *data, size_t size)
{
	char *string;
	size_t length;
	char *p;
	size_t i;

	if (data == NULL || size == 0)
		return NULL;

	length = data2string_length(data, size);
	if (length == 0)
		return NULL;

	string = (char *) calloc(1, length);
	if (string == NULL)
		return NULL;

	p = string;

	for (i = 0; i < size; i++) {
		sprintf(p, "%02x", *((unsigned char *) data + i));
		p += 2 * sizeof(char);
	}

	return string;
}

size_t string2data_size(const char *string)
{
	size_t length;
	size_t size;

	if (string == NULL)
		return 0;

	length = strlen(string);
	if (length == 0)
		return 0;

	if (length % 2 == 0)
		size = length / 2;
	else
		size = (length - (length % 2)) / 2 + 1;

	return size;
}

void *string2data(const char *string)
{
	void *data;
	size_t size;
	size_t length;
	int shift;
	unsigned char *p;
	unsigned int b;
	size_t i;
	int rc;

	if (string == NULL)
		return NULL;

	length = strlen(string);
	if (length == 0)
		return NULL;

	if (length % 2 == 0)
		shift = 0;
	else
		shift = 1;

	size = string2data_size(string);
	if (size == 0)
		return NULL;

	data = calloc(1, size);
	if (data == NULL)
		return NULL;

	p = (unsigned char *) data;

	for (i = 0; i < length; i++) {
		rc = sscanf(&string[i], "%01x", &b);
		if (rc < 1)
			b = 0;

		if ((shift % 2) == 0)
			*p |= ((b & 0x0f) << 4);
		else
			*p++ |= b & 0x0f;

		shift++;
	}

	return data;
}
