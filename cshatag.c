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

#include <openssl/sha.h>

#define BUFSZ 65536
#define HASHLEN SHA256_DIGEST_LENGTH

/**
 * Holds a file's metadata
 */
typedef struct
{
	unsigned long long s;
	unsigned long ns;
	char sha256[HASHLEN * 2 + 1];
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
void bin2hex(unsigned char *bin, char *out, size_t len)
{
	char hexval[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
	size_t i;

	for (i = 0; i < len; i++) {
		out[2 * i] = hexval[((bin[i] >> 4) & 0x0F)];
		out[2 * i + 1] = hexval[(bin[i]) & 0x0F];
	}

	out[2 * len] = 0;
}

/**
 * sha256 of contents of f, ASCII hex representation
 */
void fhash(FILE *f, char *hex)
{
	SHA256_CTX c;
	char *buf;
	unsigned char *hash;
	size_t len;

	buf = malloc(BUFSZ);
	hash = calloc(1, HASHLEN);

	if (NULL == buf || NULL == hash)
		die("Insufficient memory for hashing file");

	SHA256_Init(&c);

	while ((len = fread(buf, 1, BUFSZ, f)) != 0)
		SHA256_Update(&c, buf, len);

	SHA256_Final(hash, &c);

	bin2hex(hash, hex, HASHLEN);

	free(buf);
	free(hash);
}

/**
 * Nanosecond precision mtime of a file
 */
void getmtime(FILE *f, xa_t *actual)
{
	int fd = fileno(f);
	struct stat buf;

	fstat(fd, &buf);

	if (!S_ISREG(buf.st_mode))
		die("Error: this is not a regular file\n");

	actual->s = buf.st_mtim.tv_sec;
	actual->ns = buf.st_mtim.tv_nsec;
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
	fhash(f, actual->sha256);
}

/**
 * File's stored metadata
 */
void getstoredxa(FILE *f, xa_t *stored)
{
	int err;
	int fd = fileno(f);
	/* Example: 1335974989.123456789 => len=20 */
	char ts[32];
	ssize_t len;

	/*
	 * Initialize to zero-length string - if fgetxattr fails this is what we get
	 */
	len = fgetxattr(fd, "user.shatag.sha256", stored->sha256, sizeof(stored->sha256));
	stored->sha256[len] = '\0';

	if (len < 0) {
		if (errno == ENODATA)
			memset(stored->sha256, 0, sizeof(stored->sha256)); /* Ignore if no attr exists. */
		else
			die("Error retrieving stored attributes: %m\n");
	}

	/*
	 * Initialize to zero-length string - if fgetxattr fails this is what we get
	 */
	len = fgetxattr(fd, "user.shatag.ts", ts, sizeof(ts));
	ts[len] = '\0';
	if (len < 0) {
		if (errno == ENODATA)
			memset(stored, 0, sizeof(*stored)); /* Ignore if no attr exists. */
		else
			die("Error retrieving stored attributes: %m\n");

		return;
	}

	/*
	 * If sscanf fails (because ts is zero-length) variables stay zero
	 */
	err = sscanf(ts, "%llu.%lu", &stored->s, &stored->ns);
	if (err == 1)
		stored->ns = 0;
	else if (err != 2)
		die("Failed to read timestamp: %m\n");
	else if (stored->ns >= 1000000000)
		die("Invalid timestamp (ns too large): %s\n", ts);
}

/**
 * Write out metadata to file's extended attributes
 */
int writexa(FILE *f, xa_t xa)
{
	int fd = fileno(f);
	int flags = 0;
	int err = 0;
	char buf[32];

	err = fsetxattr(fd, "user.shatag.sha256", &xa.sha256, sizeof(xa.sha256), flags);
	if (err != 0)
		return err;

	snprintf(buf, sizeof(buf), "%llu.%09lu", xa.s, xa.ns);
	err = fsetxattr(fd, "user.shatag.ts", buf, strlen(buf), flags);
	return err;
}

/**
 * Pretty-print metadata
 */
char *formatxa(xa_t s)
{
	char *buf;
	char *prettysha;
	int buflen = HASHLEN * 2 + 32;

	buf = calloc(1, buflen);
	if (NULL == buf)
		die("Insufficient space to store hash stringed");

	if (strlen(s.sha256) > 0)
		prettysha = s.sha256;
	else
		prettysha = "0000000000000000000000000000000000000000000000000000000000000000";

	snprintf(buf, buflen, "%s %llu.%09lu", prettysha, s.s, s.ns);

	return buf;
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

	xa_t s;
	xa_t a;
	int needsupdate = 0;
	int havecorrupt = 0;

	getstoredxa(f, &s);
	getactualxa(f, &a);

	if (s.s == a.s && s.ns == a.ns) {
		/*
		 * Times are the same, go ahead and compare the hash
		 */
		if (strcmp(s.sha256, a.sha256) != 0) {
			/*
			 * Hashes are different, but this may be because
			 * the file has been modified while we were computing the hash.
			 * So check if the mtime ist still the same.
			 */
			xa_t a2;
			getmtime(f, &a2);

			if (s.s == a2.s && s.ns == a2.ns) {
				/*
				 * Now, this is either data corruption or somebody modified the file
				 * and reset the mtime to the last value (to hide the modification?)
				 */
				fprintf(stderr, "Error: corrupt file \"%s\"\n", fn);
				printf("<corrupt> %s\n", fn);
				printf(" stored: %s\n", formatxa(s));
				printf(" actual: %s\n", formatxa(a));
				needsupdate = 1;
				havecorrupt = 1;
			}
		}
		else
			printf("<ok> %s\n", fn);
	}
	else {
		printf("<outdated> %s\n", fn);
		printf(" stored: %s\n", formatxa(s));
		printf(" actual: %s\n", formatxa(a));
		needsupdate = 1;
	}

	if (needsupdate && writexa(f,a) != 0)
		die("Error: could not write extended attributes to file \"%s\": %m\n", fn);

	if (havecorrupt)
		return 5;
	else
		return 0;
}
