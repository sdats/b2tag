/*
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
 * Hash helper function declarations.
 */

#ifndef HASH_H
#define HASH_H

#include <openssl/evp.h>

/** The largest possible hash size. */
#define MAX_HASH_SIZE EVP_MAX_MD_SIZE

/** The longest possible string representation of a hash. */
#define MAX_HASH_STRING_LENGTH (2 * MAX_HASH_SIZE)

/** The supported hash algorithms. */
typedef enum hash_alg {
	/**
	 * The Blake2b hash algorithm (512-bit).
	 *
	 * Blake2b is usually the fastest algorithm on 64-bit machines.
	 */
	HASH_ALG_BLAKE2B,
	/**
	 * The Blake2s hash algorithm (256-bit).
	 *
	 * Blake2s is usually the fastest algorithm on 32-bit machines.
	 */
	HASH_ALG_BLAKE2S,
	/**
	 * The SHA-512 hash algorithm (512-bit).
	 *
	 * SHA-512 is faster than SHA-256 on 64-bit machines, but slower than Blake2.
	 */
	HASH_ALG_SHA512,
	/**
	 * The SHA-256 hash algorithm (256-bit).
	 *
	 * SHA-256 is usually the slowest algorithm on 64-bit machines.
	 *
	 * @note SHA-256 is the only hash algorithm supported by the original shatag utility.
	 */
	HASH_ALG_SHA256,
	/**
	 * The SHA-1 hash algorithm (160-bit).
	 *
	 * @warning SHA-1 is not secure and is not recommended.
	 */
	HASH_ALG_SHA1,
	/**
	 * The MD5 hash algorithm (128-bit).
	 *
	 * @warning MD5 is not secure and is not recommended.
	 */
	HASH_ALG_MD5,

} hash_alg_t;

/**
 * Hash the contents of file @p fd using the @p alg hash algorithm.
 *
 * Then store the ASCII hex representation of the resulting hash in @p hashbuf.
 *
 * @param fd      The file to hash.
 * @param hashbuf Where to store the resulting hash value.
 * @param hashlen The length of @p hash.
 * @param alg     The hash algorithm to use.
 *
 * @retval 0  The contents of @p fd were successfully hashed.
 * @retval !0 An error occurred while hashing the contents of @p fd.
 */
int fhash(int fd, char *hashbuf, int hashlen, hash_alg_t alg);

/**
 * Returns the hash size of @p alg.
 *
 * @param alg  The algorithm to use.
 *
 * @returns Returns the hash size of the @p alg hash algorithm.
 * @returns Returns 0 if an error occurs.
 */
size_t get_alg_size(hash_alg_t alg);

/**
 * Returns the name of @p alg as a string.
 *
 * @param alg  The algorithm to look up.
 *
 * @returns Returns the name of the @p alg hash algorithm.
 */
const char * get_alg_name(hash_alg_t alg);

/**
 * Looks up a hash algorithm by name and sets @p alg if not NULL.
 *
 * @param name The algorithm to look up.
 * @param alg  Where to store the algorithm type (can be NULL).
 *
 * @returns Returns 0 on success and a negative number on failure.
 */
int get_alg_by_name(const char *name, hash_alg_t *alg);

#endif /* HASH_H */
