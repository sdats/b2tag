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

#ifndef XA_H
#define XA_H

#include "hash.h"

/**
 * Holds a file's metadata
 */
typedef struct xa_s
{
	unsigned long long s;
	unsigned long ns;
	const char *alg;
	char hash[MAX_HASH_SIZE * 2 + 1];
} xa_t;

/**
 * Nanosecond precision mtime of a file
 */
void getmtime(int fd, xa_t *xa);

/**
 * Calculate the file's current metadata.
 */
void xa_calculate(int fd, xa_t *xa);

/**
 * Retrieve the file's stored metadata.
 */
void xa_read(int fd, xa_t *xa);

/**
 * Write out metadata to file's extended attributes
 */
int xa_write(int fd, xa_t *xa);

/**
 * Pretty-print metadata
 */
const char *xa_format(xa_t *xa);

#endif /* XA_H */
