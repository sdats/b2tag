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
 */

/** @file
 * Functions for dealing with extended attributes for cshatag.
 */

#include "xa.h"

#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/xattr.h>

#include "utilities.h"

/**
 * Clear the timestamp and hash values in @p xa.
 *
 * @li @p xa->alg will be left untouched.
 * @li @p xa->mtime will be zeroed.
 * @li @p xa->hash will be set to a string of ASCII '0's the same length as @p xa->alg.
 *
 * @param xa  The extended attribute structure to clear.
 */
static void xa_clear(xa_t *xa)
{
	memset(&xa->mtime, 0, sizeof(xa->mtime));
	snprintf(xa->hash, sizeof(xa->hash), "%0*d", get_alg_size(xa->alg) * 2, 0);
}

/**
 * Retrieve the stored extended attributes for @p fd and store it in @p xa.
 *
 * @param fd  The file to retrieve the extended attributes from.
 * @param xa  The extended attribute structure to store the values in.
 */
void xa_read(int fd, xa_t *xa)
{
	int err;
	int end;
	int start;
	/* Example: 1335974989.123456789 => len=20 */
	char buf[32];
	ssize_t len;

	xa_clear(xa);

	snprintf(buf, sizeof(buf), "user.shatag.%s", xa->alg);
	len = fgetxattr(fd, buf, xa->hash, sizeof(xa->hash) - 1);
	if (len < 0) {
		if (errno != ENODATA)
			die("Error retrieving stored attributes: %m\n");

		return;
	}
	xa->hash[len] = '\0';

	len = fgetxattr(fd, "user.shatag.ts", buf, sizeof(buf) - 1);
	if (len < 0) {
		if (errno != ENODATA)
			die("Error retrieving stored attributes: %m\n");

		xa_clear(xa);
		return;
	}
	buf[len] = '\0';

	err = sscanf(buf, "%lu.%n%10lu%n", &xa->mtime.tv_sec, &start, &xa->mtime.tv_nsec, &end);
	if (err != 1 && err != 2)
		die("Failed to read timestamp: %m\n");

	end -= start;
	while (end++ < 9)
		xa->mtime.tv_nsec *= 10;

	if (xa->mtime.tv_nsec >= 1000000000)
		die("Invalid timestamp (ns too large): %s\n", buf);
}

/**
 * Update the stored extended attributes for @p fd from @p xa.
 *
 * @param fd  The file to update the extended attributes of.
 * @param xa  The extended attribute structure to store to disk.
 *
 * @retval 0  The extended attributes were successfully updated.
 * @retval !0 An error occurred updating the extended attributes.
 */
int xa_write(int fd, xa_t *xa)
{
	int err;
	char buf[32];

	snprintf(buf, sizeof(buf), "user.shatag.%s", xa->alg);
	err = fsetxattr(fd, buf, &xa->hash, strlen(xa->hash), 0);
	if (err != 0)
		return err;

	snprintf(buf, sizeof(buf), "%lu.%09lu", xa->mtime.tv_sec, xa->mtime.tv_nsec);
	err = fsetxattr(fd, "user.shatag.ts", buf, strlen(buf), 0);
	return err;
}

/**
 * Convert an extended attribute structure into a human-readable form for printing.
 *
 * @param xa  The extended attribute structure to convert.
 *
 * @returns A string containing the human-readable extended attribute structure.
 *
 * @note This function uses a static buffer to format the string. It will be
 *       overwritten by successive calls to xa_format() and is not thread-safe.
 */
const char *xa_format(xa_t *xa)
{
	int len;
	static char buf[MAX_HASH_SIZE * 2 + 32];

	len = snprintf(buf, sizeof(buf), "%s %010lu.%09lu", xa->hash, xa->mtime.tv_sec, xa->mtime.tv_nsec);

	if (len < 0)
		die("Error formatting xa: %m\n");

	if ((size_t)len >= sizeof(buf))
		die("Error: buffer too small: %d > %zu\n", len + 1, sizeof(buf));

	return buf;
}
