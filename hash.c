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
 * Hash helper functions for b2tag.
 */

#include "hash.h"

#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "utilities.h"

/** The size of the file read buffer. */
#define BUFSZ 65536

/** The function signature of the OpenSSL EVP algorithms. */
typedef const EVP_MD *(*evp_func)(void);

/** Structure containing info about a hash algorithm. */
struct alg_data {
	/** The name of the algorithm (lowercase). */
	const char *name;
	/** The OpenSSL EVP function of the algorithm. */
	evp_func md;
};

/** Data about all the hash algorithms b2tag supports. */
static struct alg_data hash_alg_data[] = {
	[HASH_ALG_MD5]     = {
		.name = "md5",
		.md = EVP_md5
	},
	[HASH_ALG_SHA1]    = {
		.name ="sha1",
		.md = EVP_sha1
	},
	[HASH_ALG_SHA256]  = {
		.name ="sha256",
		.md = EVP_sha256
	},
	[HASH_ALG_SHA512]  = {
		.name ="sha512",
		.md = EVP_sha512
	},
	[HASH_ALG_BLAKE2B] = {
		.name ="blake2b512",
		.md = EVP_blake2b512
	},
	[HASH_ALG_BLAKE2S] = {
		.name ="blake2s256",
		.md = EVP_blake2s256
	},
};

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

int fhash(int fd, char *hashbuf, int hashlen, hash_alg_t alg)
{
	int err = -1;
	EVP_MD_CTX *c;
	char *buf;
	unsigned char rawhash[EVP_MAX_MD_SIZE];
	ssize_t len;
	int alg_len;

	assert(fd >= 0);
	assert(hashbuf != NULL);
	assert(hashlen > 0);
	assert(alg < ARRAY_SIZE(hash_alg_data));
	assert(hash_alg_data[alg].md != NULL);
	assert(hash_alg_data[alg].md() != NULL);

	buf = malloc(BUFSZ);
	c = EVP_MD_CTX_new();

	if (buf == NULL || c == NULL) {
		pr_err("Insufficient memory for hashing file");
		goto out;
	}

	if (EVP_DigestInit_ex(c, hash_alg_data[alg].md(), NULL) == 0) {
		pr_err("Failed to initialize digest\n");
		goto out;
	}

	/* The length of the algorithm's hash. */
	alg_len = EVP_MD_CTX_size(c);

	assert(alg_len > 0);
	assert(alg_len <= MAX_HASH_SIZE);

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

size_t get_alg_size(hash_alg_t alg)
{
	int len;

	assert(alg < ARRAY_SIZE(hash_alg_data));
	assert(hash_alg_data[alg].md != NULL);
	assert(hash_alg_data[alg].md() != NULL);

	len = EVP_MD_size(hash_alg_data[alg].md());

	if (len <= 0 || len > EVP_MAX_MD_SIZE) {
		pr_err("Invalid algorithm size for alg %d: %d\n", (int)alg, len);
		len = 0;
	}

	return len;
}

const char * get_alg_name(hash_alg_t alg)
{
	assert(alg < ARRAY_SIZE(hash_alg_data));
	assert(hash_alg_data[alg].name != NULL);

	return hash_alg_data[alg].name;
}

int get_alg_by_name(const char *name, hash_alg_t *alg)
{
	size_t i;

	assert(name != NULL);

	for (i = 0; i < ARRAY_SIZE(hash_alg_data); i++) {
		if (strcmp(hash_alg_data[i].name, name) == 0) {
			if (alg != NULL)
				*alg = (hash_alg_t)i;
			return 0;
		}
	}

	return -1;
}
