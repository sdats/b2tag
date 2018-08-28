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
 * Hash helper function declarations.
 */

#ifndef HASH_H
#define HASH_H

#include <openssl/evp.h>

/** The largest possible hash size. */
#define MAX_HASH_SIZE EVP_MAX_MD_SIZE

/**
 * Hash the contents of file @p fd using the @p alg hash algorithm.
 *
 * Then store the ASCII hex representation of the resulting hash in @p hashbuf.
 *
 * @param fd      The file to hash.
 * @param hashbuf Where to store the resulting hash value.
 * @param hashlen The length of @p hash.
 * @param alg     The hash algorithm to use.
 */
void fhash(int fd, char *hashbuf, int hashlen, const char *alg);

/**
 * Returns the hash size of @p alg.
 *
 * @param alg  The algorithm to use.
 *
 * @returns Returns the hash size of the @p alg hash algorithm.
 */
int get_alg_size(const char *alg);

#endif /* HASH_H */
