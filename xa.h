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
 * Extended attribute functions and data structures.
 */

#ifndef XA_H
#define XA_H

#include <stdbool.h>
#include <sys/xattr.h>

#include "hash.h"

#ifndef ENOATTR
#define ENOATTR ENODATA
#endif

/**
 * Metadata structure for cshatag.
 */
typedef struct xa_s
{
	/** Whether the xattr struct contains a valid hash. */
	bool valid;
	/** The file's last modified time. */
	struct timespec mtime;
	/** The hash algorithm to use. */
	hash_alg_t alg;
	/** The file data's hash value as an ASCII hex string. */
	char hash[MAX_HASH_SIZE * 2 + 1];
} xa_t;

/**
 * Clear the timestamp and hash values in @p xa.
 *
 * @li @p xa->alg will be left untouched.
 * @li @p xa->mtime will be zeroed.
 * @li @p xa->hash will be set to a string of ASCII '0's the same length as @p xa->alg.
 *
 * @param xa  The extended attribute structure to clear.
 */
void xa_clear(xa_t *xa);

/**
 * Hash the contents of @p fd and store the result in @p xa.
 *
 * Additionally, retrieve the last mtime of @p fd and store it in @p xa
 * (unless @p xa already contains a non-zero mtime).
 *
 * @param fd  The file to compute the hash of.
 * @param xa  The extended attribute structure to store the values in.
 *
 * @retval 0  The contents of @p fd were successfully hashed.
 * @retval !0 An error occurred while hashing the contents of @p fd.
 */
int xa_compute(int fd, xa_t *xa);

/**
 * Retrieve the stored extended attributes for @p fd and store it in @p xa.
 *
 * @param fd  The file to retrieve the extended attributes from.
 * @param xa  The extended attribute structure to store the values in.
 *
 * @retval 0  The extended attributes were successfully read.
 * @retval !0 An error occurred reading the extended attributes.
 */
int xa_read(int fd, xa_t *xa);

/**
 * Update the stored extended attributes for @p fd from @p xa.
 *
 * @param fd  The file to update the extended attributes of.
 * @param xa  The extended attribute structure to store to disk.
 *
 * @retval 0  The extended attributes were successfully updated.
 * @retval !0 An error occurred updating the extended attributes.
 */
int xa_write(int fd, xa_t *xa);

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
const char *xa_format(xa_t *xa);

#endif /* XA_H */
