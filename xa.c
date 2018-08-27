/**
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

#include "xa.h"

#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/xattr.h>

#include "cshatag.h"

/**
 * Nanosecond precision mtime of a file
 */
void getmtime(int fd, xa_t *xa)
{
	struct stat st;

	fstat(fd, &st);

	if (!S_ISREG(st.st_mode))
		die("Error: this is not a regular file\n");

	xa->s = st.st_mtim.tv_sec;
	xa->ns = st.st_mtim.tv_nsec;
}

/**
 * File's actual metadata
 */
void xa_calculate(int fd, xa_t *xa)
{
	/*
	 * Must read mtime *before* file hash,
	 * if the file is being modified, hash will be invalid
	 * but timestamp will be outdated anyway
	 */
	getmtime(fd, xa);

	/*
	 * Compute hash
	 */
	fhash(fd, xa->hash, sizeof(xa->hash), xa->alg);
}

/**
 * Clear an xa structure.
 */
void xa_clear(xa_t *xa)
{
	xa->s = 0;
	xa->ns = 0;
	snprintf(xa->hash, sizeof(xa->hash), "%0*d", get_alg_size(xa->alg) * 2, 0);
}

/**
 * File's stored metadata
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
		if (errno == ENODATA)
			return;

		die("Error retrieving stored attributes: %m\n");
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

	err = sscanf(buf, "%llu.%n%10lu%n", &xa->s, &start, &xa->ns, &end);
	if (err != 1 && err != 2)
		die("Failed to read timestamp: %m\n");

	if (xa->ns >= 1000000000)
		die("Invalid timestamp (ns too large): %s\n", buf);

	end -= start;
	while (end++ < 9)
		xa->ns *= 10;
}

/**
 * Write out metadata to file's extended attributes
 */
int xa_write(int fd, xa_t *xa)
{
	int err;
	char buf[32];

	snprintf(buf, sizeof(buf), "user.shatag.%s", xa->alg);
	err = fsetxattr(fd, buf, &xa->hash, strlen(xa->hash), 0);
	if (err != 0)
		return err;

	snprintf(buf, sizeof(buf), "%llu.%09lu", xa->s, xa->ns);
	err = fsetxattr(fd, "user.shatag.ts", buf, strlen(buf), 0);
	return err;
}

/**
 * Pretty-print metadata
 */
const char *xa_format(xa_t *xa)
{
	int len;
	static char buf[MAX_HASH_SIZE * 2 + 32];

	len = snprintf(buf, sizeof(buf), "%s %010llu.%09lu", xa->hash, xa->s, xa->ns);

	if (len < 0)
		die("Error formatting xa: %m\n");

	if ((size_t)len >= sizeof(buf))
		die("Error: buffer too small: %d > %zu\n", len + 1, sizeof(buf));

	return buf;
}
