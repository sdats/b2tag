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
 * @param out     The buffer to store the hex string in.
 * @param outlen  The length of @p out.
 * @param bin     The input raw data array.
 * @param len     The length of @p bin.
 *
 * @note @p out must be at least (@p len * 2) + 1 in length.
 */
static int bin2hex(char *out, int outlen, unsigned char *bin, int len)
{
	char hexval[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
	int i;

	assert(out != NULL);
	assert(len >= 0);
	assert(bin != NULL || len == 0);

	if (outlen <= (len * 2)) {
		pr_err("Hash buffer is too small: %d < %d\n", outlen, len * 2 + 1);
		return -1;
	}

	for (i = 0; i < len; i++) {
		out[2 * i]     = hexval[(bin[i] >> 4) & 0x0F];
		out[2 * i + 1] = hexval[bin[i]        & 0x0F];
	}

	out[2 * len] = '\0';

	return 0;
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
	int err = -1;
	EVP_MD_CTX *c;
	EVP_MD const *a;
	char *buf;
	unsigned char rawhash[EVP_MAX_MD_SIZE];
	ssize_t len;
	int alg_len;

	assert(fd >= 0);
	assert(hashbuf != NULL);
	assert(hashlen > 0);
	assert(alg != NULL);

	buf = malloc(BUFSZ);
	c = EVP_MD_CTX_new();

	if (buf == NULL || c == NULL) {
		pr_err("Insufficient memory for hashing file");
		goto out;
	}

	a = EVP_get_digestbyname(alg);
	if (NULL == a) {
		pr_err("Failed to find hash algorithm %s\n", alg);
		goto out;
	}

	if (EVP_DigestInit_ex(c, a, NULL) == 0) {
		pr_err("Failed to initialize digest\n");
		goto out;
	}

	/* The length of the algorithm's hash (as a hex string, including NUL). */
	alg_len = EVP_MD_CTX_size(c);

	assert(alg_len > 0);

	if ((alg_len * 2) >= hashlen) {
		pr_err("Hash exceeds buffer size: %d > %d\n", alg_len * 2 + 1, hashlen);
		goto out;
	}

	while ((len = read(fd, buf, BUFSZ)) > 0) {
		if (EVP_DigestUpdate(c, buf, (size_t)len) == 0) {
			pr_err("Failed to update digest\n");
			goto out;
		}
	}

	if (len < 0) {
		pr_err("Error reading file: %m\n");
		goto out;
	}

	if (EVP_DigestFinal_ex(c, rawhash, (unsigned int *)&alg_len) == 0) {
		pr_err("Failed to finalize digest\n");
		goto out;
	}

	assert(alg_len > 0);

	if (bin2hex(hashbuf, hashlen, rawhash, alg_len) != 0)
		goto out;

	err = 0;

out:
	EVP_MD_CTX_free(c);
	free(buf);

	return err;
}

/**
 * Returns the hash size of @p alg.
 *
 * @param alg  The algorithm to use.
 *
 * @returns Returns the hash size of the @p alg hash algorithm.
 * @returns Returns a negative number if an error occurs.
 */
int get_alg_size(const char *alg)
{
	EVP_MD const *a;
	int len;

	assert(alg != NULL);

	a = EVP_get_digestbyname(alg);

	if (a == NULL) {
		pr_err("Failed to find hash algorithm \"%s\"\n", alg);
		return -1;
	}

	len = EVP_MD_size(a);

	assert(len <= EVP_MAX_MD_SIZE);

	if (len < 0)
		pr_err("Invalid algorithm size for \"%s\": %d\n", alg, len);

	return len;
}
