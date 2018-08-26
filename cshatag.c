/**
 * Copyright (C) 2012 Jakob Unterwurzacher <jakobunt@gmail.com>
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

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/xattr.h>
#include <time.h>

#include <openssl/evp.h>
#include <openssl/sha.h>

#define BUFSZ 65536
#define HASHALG "sha256"

/**
 * Holds a file's metadata
 */
typedef struct
{
	unsigned long long s;
	unsigned long ns;
	char const *alg;
	int length;
	char hash[EVP_MAX_MD_SIZE * 2 + 1];
} xa_t;

static void die(const char *fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));

static void die(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	exit(EXIT_FAILURE);
}

/**
 * ASCII hex representation of char array
 */
void bin2hex(char *out, unsigned char *bin, int len)
{
	char hexval[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
	int i;

	for (i = 0; i < len; i++) {
		out[2 * i] = hexval[((bin[i] >> 4) & 0x0F)];
		out[2 * i + 1] = hexval[(bin[i]) & 0x0F];
	}

	out[2 * len] = 0;
}

/**
 * hash of contents of f, ASCII hex representation
 */
void fhash(FILE *f, xa_t *xa)
{
	EVP_MD_CTX *c;
	EVP_MD const *alg;
	char *buf;
	unsigned char hash[EVP_MAX_MD_SIZE];
	size_t len;
	int alg_len;

	buf = malloc(BUFSZ);
	c = EVP_MD_CTX_new();

	if (NULL == buf || NULL == c)
		die("Insufficient memory for hashing file");

	alg = EVP_get_digestbyname(xa->alg);
	if (NULL == alg)
		die("Failed to find hash algorithm %s\n", HASHALG);

	if (EVP_DigestInit_ex(c, alg, NULL) == 0)
		die("Failed to initialize digest\n");

	if (xa->length != EVP_MD_CTX_size(c))
		die("Hash length mismatch: %d != %d\n", xa->length, EVP_MD_CTX_size(c));

	while ((len = fread(buf, 1, BUFSZ, f)) != 0) {
		if (EVP_DigestUpdate(c, buf, len) == 0)
			die("Failed to update digest\n");
	}

	len = 0;
	if (EVP_DigestFinal_ex(c, hash, (unsigned int *)&alg_len) == 0)
		die("Failed to finalize digest\n");

	if (alg_len != xa->length)
		die("Final hash length mismatch: %d != %d\n", alg_len, xa->length);

	bin2hex(xa->hash, hash, xa->length);

	EVP_MD_CTX_free(c);
	free(buf);
}

/**
 * Nanosecond precision mtime of a file
 */
void getmtime(FILE *f, xa_t *actual)
{
	int fd = fileno(f);
	struct stat st;

	fstat(fd, &st);

	if (!S_ISREG(st.st_mode))
		die("Error: this is not a regular file\n");

	actual->s = st.st_mtim.tv_sec;
	actual->ns = st.st_mtim.tv_nsec;
}

/**
 * File's actual metadata
 */
void getactualxa(FILE *f, xa_t *actual)
{
	/*
	 * Must read mtime *before* file hash,
	 * if the file is being modified, hash will be invalid
	 * but timestamp will be outdated anyway
	 */
	getmtime(f, actual);

	/*
	 * Compute hash
	 */
	fhash(f, actual);
}

/**
 * File's stored metadata
 */
void getstoredxa(FILE *f, xa_t *stored)
{
	int err;
	int fd = fileno(f);
	/* Example: 1335974989.123456789 => len=20 */
	char buf[32];
	ssize_t len;

	/*
	 * Initialize to zero-length string - if fgetxattr fails this is what we get
	 */
	snprintf(buf, sizeof(buf), "user.shatag.%s", stored->alg);

	len = fgetxattr(fd, buf, stored->hash, sizeof(stored->hash));
	stored->hash[len] = '\0';

	if (len < 0) {
		if (errno == ENODATA)
			memset(stored->hash, 0, sizeof(stored->hash)); /* Ignore if no attr exists. */
		else
			die("Error retrieving stored attributes: %m\n");
	}
	else if (len != (ssize_t)stored->length * 2 + 1)
		die("Unexpected attribute size: Expected %zd, got %d.\n", len, stored->length * 2 + 1);

	/*
	 * Initialize to zero-length string - if fgetxattr fails this is what we get
	 */
	len = fgetxattr(fd, "user.shatag.ts", buf, sizeof(buf));
	buf[len] = '\0';
	if (len < 0) {
		if (errno == ENODATA)
			memset(stored->hash, 0, sizeof(stored->hash)); /* Ignore if no attr exists. */
		else
			die("Error retrieving stored attributes: %m\n");

		return;
	}

	/*
	 * If sscanf fails (because buf is zero-length) variables stay zero
	 */
	err = sscanf(buf, "%llu.%lu", &stored->s, &stored->ns);
	if (err == 1)
		stored->ns = 0;
	else if (err != 2)
		die("Failed to read timestamp: %m\n");
	else if (stored->ns >= 1000000000)
		die("Invalid timestamp (ns too large): %s\n", buf);
}

/**
 * Write out metadata to file's extended attributes
 */
int writexa(FILE *f, xa_t *xa)
{
	int fd = fileno(f);
	int err;
	char buf[32];

	snprintf(buf, sizeof(buf), "user.shatag.%s", xa->alg);
	err = fsetxattr(fd, buf, &xa->hash, xa->length * 2 + 1, 0);
	if (err != 0)
		return err;

	snprintf(buf, sizeof(buf), "%llu.%09lu", xa->s, xa->ns);
	err = fsetxattr(fd, "user.shatag.ts", buf, strlen(buf), 0);
	return err;
}

static int get_alg_size(const char *alg)
{
	EVP_MD const *a = EVP_get_digestbyname(alg);
	int len;

	if (a == NULL)
		die("Failed to find hash algorithm \"%s\"\n", alg);

	len = EVP_MD_size(a);
	if (len > EVP_MAX_MD_SIZE)
		die("Algorithm \"%s\" is too large: %d > %d\n", alg, len, EVP_MAX_MD_SIZE);

	if (len < 0)
		die("Algorithm \"%s\" is too small: %d\n", alg, len);

	return len;
}
/**
 * Pretty-print metadata
 */
void formatxa(xa_t *s, char *buf, size_t buflen)
{
	int len;
	char tmp[EVP_MAX_MD_SIZE * 2 + 1];

	if (strlen(s->hash) > 0)
		len = snprintf(tmp, sizeof(tmp), "%s", s->hash);
	else
		len = snprintf(tmp, sizeof(tmp), "%0*d", s->length * 2, 0);

	if ((size_t)len >= sizeof(tmp))
		die("Error: buffer too small: %d > %zu\n", len + 1, sizeof(tmp));

	len = snprintf(buf, buflen, "%s %010llu.%09lu", tmp, s->s, s->ns);
	if ((size_t)len >= buflen)
		die("Error: buffer too small: %d > %zu\n", len + 1, buflen);
}

int main(int argc, char *argv[])
{
	char const *myname = argv[0];

	if (argc != 2)
		die("Usage: %s FILE\n", myname);

	char const *fn = argv[1];

	FILE *f = fopen(fn, "r");
	if (!f)
		die("Error: could not open file \"%s\": %m\n", fn);

	xa_t s = (xa_t){ .alg = HASHALG };
	xa_t a = (xa_t){ .alg = HASHALG };
	int needsupdate = 0;
	int havecorrupt = 0;

	a.length = s.length = get_alg_size(s.alg);

	getstoredxa(f, &s);
	getactualxa(f, &a);

	if (s.s == a.s && s.ns == a.ns) {
		/*
		 * Times are the same, go ahead and compare the hash
		 */
		if (strcmp(s.hash, a.hash) != 0) {
			/*
			 * Hashes are different, but this may be because
			 * the file has been modified while we were computing the hash.
			 * So check if the mtime ist still the same.
			 */
			xa_t a2;
			getmtime(f, &a2);

			char tmp[EVP_MAX_MD_SIZE * 2 + 32];

			if (s.s == a2.s && s.ns == a2.ns) {
				/*
				 * Now, this is either data corruption or somebody modified the file
				 * and reset the mtime to the last value (to hide the modification?)
				 */
				fprintf(stderr, "Error: corrupt file \"%s\"\n", fn);
				printf("<corrupt> %s\n", fn);
				formatxa(&s, tmp, sizeof(tmp));
				printf(" stored: %s\n", tmp);
				formatxa(&a, tmp, sizeof(tmp));
				printf(" actual: %s\n", tmp);
				needsupdate = 1;
				havecorrupt = 1;
			}
		}
		else
			printf("<ok> %s\n", fn);
	}
	else {
		char tmp[EVP_MAX_MD_SIZE * 2 + 32];
		printf("<outdated> %s\n", fn);
		formatxa(&s, tmp, sizeof(tmp));
		printf(" stored: %s\n", tmp);
		formatxa(&a, tmp, sizeof(tmp));
		printf(" actual: %s\n", tmp);
		needsupdate = 1;
	}

	if (needsupdate && writexa(f, &a) != 0)
		die("Error: could not write extended attributes to file \"%s\": %m\n", fn);

	fclose(f);

	if (havecorrupt)
		return 5;
	else
		return 0;
}
