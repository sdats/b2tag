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

#include "utilities.h"

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
	int err;
	int end;
	int start;
	/* Example: 1335974989.123456789 => len=20 */
	char buf[32];
	ssize_t len;

	xa_clear(xa);

	assert(fd >= 0);

	/* Read timestamp xattr. */
	len = fgetxattr(fd, "user.shatag.ts", buf, sizeof(buf) - 1);
	if (len < 0) {
		if (errno == ENOATTR)
			return 1;

		if (errno == ENOTSUP)
			pr_err("Filesystem does not support extended attributes\n");
		else
			pr_err("Failed to retrieve user.shatag.ts: %m\n");
		return -1;
	}

	buf[len] = '\0';

	err = sscanf(buf, "%ld.%n%10ld%n", &xa->mtime.tv_sec, &start, &xa->mtime.tv_nsec, &end);
	if (err < 1) {
		pr_err("Malformed timestamp: %m\n");
		xa_clear(xa);
		return 2;
	}

	end -= start;
	if (end < 9) {
		xa->fuzzy = true;

		for (; end < 9; end++)
			xa->mtime.tv_nsec *= 10;
	}

	if (xa->mtime.tv_nsec >= 1000000000 || end >= 10) {
		pr_err("Invalid timestamp (ns too large): %s\n", buf);
		xa_clear(xa);
		return 2;
	}

	/* Read hash xattr. */
	snprintf(buf, sizeof(buf), "user.shatag.%s", get_alg_name(xa->alg));
	len = fgetxattr(fd, buf, xa->hash, sizeof(xa->hash) - 1);
	if (len < 0) {
		xa_clear(xa);
		if (errno == ENOATTR)
			return 1;

		pr_err("Failed to retrieve %s: %m\n", buf);
		return -1;
	}

	xa->hash[len] = '\0';

	if (len != (ssize_t)get_alg_size(xa->alg) * 2) {
		pr_err("Stored hash size mismatch: %zd != %d\n", len, get_alg_size(xa->alg) * 2);
		xa_clear(xa);
		return 2;
	}

	/* Make sure the hash is all lowercase hex chars. */
	for (start = 0; (ssize_t)start < len; start++) {
		char c = xa->hash[start];

		if (!isxdigit(c)) {
			pr_err("Malformed hash.\n");
			if (isprint(c))
				pr_debug("Found '%c' (0x%02x).\n", c, (unsigned)c);
			else
				pr_debug("Found 0x%02x character.\n", (unsigned)c);

			xa_clear(xa);
			return 2;
		}

		/* Convert to lowercase if necessary. */
		if (isupper(c))
			xa->hash[start] = tolower(c);
	}

	xa->valid = true;

	return 0;
}

int xa_write(int fd, xa_t *xa)
{
	int err;
	char buf[32];

	assert(fd >= 0);
	assert(xa != NULL);

	if (!xa->valid)
		return -EINVAL;

	snprintf(buf, sizeof(buf), "user.shatag.%s", get_alg_name(xa->alg));
	err = fsetxattr(fd, buf, &xa->hash, strlen(xa->hash), 0);
	if (err != 0) {
		pr_err("Failed to set `%s' xattr: %m\n", buf);
		return err;
	}

	snprintf(buf, sizeof(buf), "%lu.%09lu", xa->mtime.tv_sec, xa->mtime.tv_nsec);
	err = fsetxattr(fd, "user.shatag.ts", buf, strlen(buf), 0);
	if (err != 0)
		pr_err("Failed to set `%s' xattr: %m\n", "user.shatag.ts");

	return err;
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
