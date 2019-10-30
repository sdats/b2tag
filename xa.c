/*
 * Copyright (C) 2012 Jakob Unterwurzacher <jakobunt@gmail.com>
 * Copyright (C) 2018 Tim Schlueter
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the author of this program
 * gives permission to link the code portions of this program with the
 * OpenSSL library under certain conditions as described in each file,
 * and distribute linked combinations including the two.
 * You must obey the GNU General Public License in all respects for all
 * of the code used other than OpenSSL.  If you modify this file(s)
 * with this exception, you may extend this exception to your version
 * of the file(s), but you are not obligated to do so.  If you do not
 * wish to do so, delete this exception statement from your version.
 * If you delete this exception statement from all source files in the
 * program, then also delete it here.
 */

/** @file
 * Functions for dealing with extended attributes for b2tag.
 */

#include "xa.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>

#include <sys/xattr.h>

#include "utilities.h"

#ifndef ENOATTR
#define ENOATTR ENODATA
#endif

#define XATTR_NAMESPACE "user.shatag"
#define TIMESTAMP_XATTR XATTR_NAMESPACE ".ts"


static err_t xa_read_xattr(int fd, const char* attr_name, char* buffer, size_t size) {
	ssize_t len;
	len = fgetxattr(fd, attr_name, buffer, size - 1);
	if (len < 0) {
		switch (errno) {
			case ENOATTR: return E_NOT_FOUND;
			case ERANGE:  return E_INVALID;
			case ENOTSUP: return E_UNSUPPORTED;
			default:      return E_IO_ERROR;
		}
	}

	buffer[len] = '\0';
	return E_OK;
}

static err_t xa_write_xattr(int fd, const char* attr_name, const char* value) {
	int err;
	err = fsetxattr(fd, attr_name, value, strlen(value), 0);
	if (err != 0) {
		switch (errno) {
			case ENOTSUP: return E_UNSUPPORTED;
			default:      return E_IO_ERROR;
		}
	}
	return E_OK;
}

static err_t xa_remove_xattr(int fd, const char* attr_name) {
	int err;
	err = fremovexattr(fd, attr_name);
	if (err != 0) {
		switch (errno) {
			case ENOATTR: return E_NOT_FOUND;
			case ENOTSUP: return E_UNSUPPORTED;
			default:      return E_IO_ERROR;
		}
	}
	return E_OK;
}

err_t xa_read_timestamp(int fd, struct timespec* mtime, bool* truncated) {
	int err;
	int end;
	int start;
	/* Example: 1335974989.123456789 => len=20 */
	char buf[32];
	err_t result;

	assert(fd >= 0);
	assert(mtime);
	assert(truncated);

	result = xa_read_xattr(fd, TIMESTAMP_XATTR, buf, sizeof(buf));
	if (result != E_OK) {
		return result;
	}

	err = sscanf(buf, "%ld.%n%10ld%n", &mtime->tv_sec, &start, &mtime->tv_nsec, &end);
	if (err < 1)
		return E_INVALID;

	end -= start;
	if (end < 9) {
		*truncated = true;
		for (; end < 9; end++)
			mtime->tv_nsec *= 10;
	}

	if (mtime->tv_nsec >= 1000000000 || end >= 10)
		return E_INVALID;

	return E_OK;
}

err_t xa_write_timestamp(int fd, const struct timespec mtime) {
	char buf[32];

	assert(fd >= 0);

	snprintf(buf, sizeof(buf), "%lu.%09lu", mtime.tv_sec, mtime.tv_nsec);
	return xa_write_xattr(fd, TIMESTAMP_XATTR, buf);
}

err_t xa_remove_timestamp(int fd) {
	return xa_remove_xattr(fd, TIMESTAMP_XATTR);
}

err_t xa_read_checksum(int fd, hash_alg_t alg, char* checksum) {
	char buf[32];
	err_t result;
	char* c = checksum;

	assert(fd >= 0);
	assert(checksum);

	snprintf(buf, sizeof(buf), XATTR_NAMESPACE ".%s", get_alg_name(alg));
	result = xa_read_xattr(fd, buf, checksum, MAX_HASH_STRING_LENGTH + 1);
	if (result != E_OK)
		return result;

	if (strlen(checksum) != get_alg_size(alg) * 2)
		return E_INVALID;

	/* Make sure the hash is all lowercase hex chars. */
	while (*c) {
		if (!isxdigit(*c))
			return E_INVALID;

		/* Convert to lowercase if necessary. */
		if (isupper(*c))
			*c = tolower(*c);

		++c;
	}
	return E_OK;
}

err_t xa_write_checksum(int fd, hash_alg_t alg, const char* checksum) {
	char buf[32];

	assert(fd >= 0);
	assert(checksum);

	snprintf(buf, sizeof(buf), XATTR_NAMESPACE ".%s", get_alg_name(alg));
	return xa_write_xattr(fd, buf, checksum);
}

err_t xa_remove_checksum(int fd, hash_alg_t alg) {
	char buf[32];

	assert(fd >= 0);

	snprintf(buf, sizeof(buf), XATTR_NAMESPACE ".%s", get_alg_name(alg));
	return xa_remove_xattr(fd, buf);
}

void xa_clear(xa_t *xa)
{
	hash_alg_t alg;
	size_t len;
	int err;

	assert(xa != NULL);

	/* Save errno. */
	err = errno;

	alg = xa->alg;
	len = (size_t)get_alg_size(alg) * 2;

	memset(xa, 0, sizeof(*xa));

	xa->alg = alg;
	memset(xa->hash, '0', len);

	/* Restore errno. */
	errno = err;
}

int xa_compute(int fd, xa_t *xa)
{
	int err;

	assert(xa != NULL);

	err = fhash(fd, xa->hash, sizeof(xa->hash), xa->alg);
	if (err == 0)
		xa->valid = true;

	assert(strlen(xa->hash) == (size_t)get_alg_size(xa->alg) * 2);

	return err;
}

int xa_read(int fd, xa_t *xa)
{
	err_t result;

	xa_clear(xa);
	assert(fd >= 0);

	/* Read timestamp xattr. */
	result = xa_read_timestamp(fd, &xa->mtime, &xa->fuzzy);
	if (result != E_OK) {
		xa_clear(xa);
		switch (result) {
			case E_NOT_FOUND:
				return 1;
			case E_UNSUPPORTED:
				pr_err("Filesystem does not support extended attributes\n");
				return -1;
			case E_IO_ERROR:
				pr_err("Failed to retrieve `user.shatag.ts': %m\n");
				return -1;
			case E_INVALID:
				pr_err("Malformed timestamp: %m\n");
				return 2;
			default:
				break;
		}
	}

	/* Read hash xattr. */
	result = xa_read_checksum(fd, xa->alg, xa->hash);
	if (result != E_OK) {
		xa_clear(xa);
		switch (result) {
			case E_NOT_FOUND:
				return 1;
			case E_UNSUPPORTED:
				pr_err("Filesystem does not support extended attributes\n");
				return -1;
			case E_IO_ERROR:
				pr_err("Failed to retrieve `" XATTR_NAMESPACE ".%s': %m\n", get_alg_name(xa->alg));
				return -1;
			case E_INVALID:
				pr_err("Malformed checksum `" XATTR_NAMESPACE ".%s': %m\n", get_alg_name(xa->alg));
				return 2;
			default:
				break;
		}
	}

	xa->valid = true;
	return 0;
}

int xa_write(int fd, xa_t *xa)
{
	err_t result;

	assert(fd >= 0);
	assert(xa != NULL);

	if (!xa->valid)
		return -EINVAL;

	result = xa_write_checksum(fd, xa->alg, xa->hash);
	if (result != E_OK) {
		pr_err("Failed to set `" XATTR_NAMESPACE ".%s' xattr: %m\n", get_alg_name(xa->alg));
		return -1;
	}

	result = xa_write_timestamp(fd, xa->mtime);
	if (result != E_OK) {
		pr_err("Failed to set `%s' xattr: %m\n", TIMESTAMP_XATTR);
		return -1;
	}

	return 0;
}

const char *xa_format(xa_t *xa)
{
	int len;
	static char buf[MAX_HASH_SIZE * 2 + 32];

	assert(xa != NULL);

	if (!xa->valid)
		return "<empty>";

	len = snprintf(buf, sizeof(buf), "%s %010lu.%09lu", xa->hash, xa->mtime.tv_sec, xa->mtime.tv_nsec);

	if (len < 0)
		die("Error formatting xa: %m\n");

	if ((size_t)len >= sizeof(buf))
		die("Error: buffer too small: %d > %zu\n", len + 1, sizeof(buf));

	return buf;
}
