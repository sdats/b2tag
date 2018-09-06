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
 * Hash helper functions for cshatag.
 */

#include "hash.h"

#include <assert.h>
#include <unistd.h>

#include "utilities.h"

/** The size of the file read buffer. */
#define BUFSZ 65536

/**
 * Converts a raw array into a hex string.
 *
 * @param out  The buffer to store the hex string in.
 * @param bin  The input raw data array.
 * @param len  The length of @p bin.
 *
 * @note @p out must be at least (@p len * 2) + 1 in length.
 */
static void bin2hex(char *out, unsigned char *bin, int len)
{
	char hexval[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
	int i;

	assert(out != NULL);
	assert(bin != NULL || len == 0);

	for (i = 0; i < len; i++) {
		out[2 * i] = hexval[((bin[i] >> 4) & 0x0F)];
		out[2 * i + 1] = hexval[(bin[i]) & 0x0F];
	}

	out[2 * len] = 0;
}

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
int fhash(int fd, char *hashbuf, int hashlen, const char *alg)
{
	EVP_MD_CTX *c;
	EVP_MD const *a;
	char *buf;
	unsigned char rawhash[EVP_MAX_MD_SIZE];
	ssize_t len;
	int alg_len;

	assert(hashbuf != NULL);
	assert(hashlen > 0);
	assert(alg != NULL);

	buf = malloc(BUFSZ);
	c = EVP_MD_CTX_new();

	if (buf == NULL || c == NULL)
		die("Insufficient memory for hashing file");

	a = EVP_get_digestbyname(alg);
	if (NULL == a)
		die("Failed to find hash algorithm %s\n", alg);

	if (EVP_DigestInit_ex(c, a, NULL) == 0)
		die("Failed to initialize digest\n");

	/* The length of the algorithm's hash (as a hex string, including NUL). */
	alg_len = EVP_MD_CTX_size(c);

	assert(alg_len > 0);

	if ((alg_len * 2) >= hashlen)
		die("Hash exceeds buffer size: %d > %d\n", alg_len * 2 + 1, hashlen);

	while ((len = read(fd, buf, BUFSZ)) > 0) {
		if (EVP_DigestUpdate(c, buf, (size_t)len) == 0)
			die("Failed to update digest\n");
	}

	if (len < 0)
		die("Error reading file: %m\n");

	if (EVP_DigestFinal_ex(c, rawhash, (unsigned int *)&alg_len) == 0)
		die("Failed to finalize digest\n");

	assert(alg_len > 0);

	if ((alg_len * 2) >= hashlen)
		die("Final hash length too large: %d > %d\n", alg_len * 2 + 1, hashlen);

	bin2hex(hashbuf, rawhash, alg_len);

	EVP_MD_CTX_free(c);
	free(buf);

	return 0;
}

/**
 * Returns the hash size of @p alg.
 *
 * @param alg  The algorithm to use.
 *
 * @returns Returns the hash size of the @p alg hash algorithm.
 */
int get_alg_size(const char *alg)
{
	EVP_MD const *a;
	int len;

	assert(alg != NULL);

	a = EVP_get_digestbyname(alg);

	if (a == NULL)
		die("Failed to find hash algorithm \"%s\"\n", alg);

	len = EVP_MD_size(a);
	if (len > EVP_MAX_MD_SIZE)
		die("Algorithm \"%s\" is too large: %d > %d\n", alg, len, EVP_MAX_MD_SIZE);

	if (len < 0)
		die("Algorithm \"%s\" is too small: %d\n", alg, len);

	return len;
}
