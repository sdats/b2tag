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

#include "cshatag.h"

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "xa.h"

#define HASHALG "sha256"


void die(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	exit(EXIT_FAILURE);
}

int check_file(const char *filename)
{
	int fd = open(filename, O_RDONLY);
	if (fd < 0)
		die("Error: could not open file \"%s\": %m\n", filename);

	xa_t s = (xa_t){ .alg = HASHALG };
	xa_t a = (xa_t){ .alg = HASHALG };
	int needsupdate = 0;
	int havecorrupt = 0;

	xa_read(fd, &s);
	if (strlen(s.hash) != (size_t)get_alg_size(s.alg) * 2)
		die("Stored hash size mismatch: Expected %d, got %zu.\n", get_alg_size(s.alg), strlen(s.hash));

	xa_calculate(fd, &a);
	if (strlen(a.hash) != (size_t)get_alg_size(a.alg) * 2)
		die("Calculated hash size mismatch: Expected %d, got %zu.\n", get_alg_size(a.alg), strlen(a.hash));

	if ((unsigned long)s.alg != (unsigned long)a.alg)
		die("Algorithm mismatch: \"%s\" != \"%s\"\n", s.alg, a.alg);

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
			getmtime(fd, &a2);

			if (s.s == a2.s && s.ns == a2.ns) {
				/*
				 * Now, this is either data corruption or somebody modified the file
				 * and reset the mtime to the last value (to hide the modification?)
				 */
				fprintf(stderr, "Error: corrupt file \"%s\"\n", filename);
				printf("<corrupt> %s\n", filename);
				printf(" stored: %s\n", xa_format(&s));
				printf(" actual: %s\n", xa_format(&a));
				needsupdate = 1;
				havecorrupt = 1;
			}
		}
		else
			printf("<ok> %s\n", filename);
	}
	else {
		printf("<outdated> %s\n", filename);
		printf(" stored: %s\n", xa_format(&s));
		printf(" actual: %s\n", xa_format(&a));
		needsupdate = 1;
	}

	if (needsupdate && xa_write(fd, &a) != 0)
		die("Error: could not write extended attributes to file \"%s\": %m\n", filename);

	close(fd);

	if (havecorrupt)
		return 5;
	else
		return 0;
}

void usage(char *program)
{
	program = basename(program);
	printf(
		"Usage: %s <FILE>...\n"
		"\n"
		"Display and update xattr-based checksums.\n"
		"\n"
		"Positional arguments:\n"
		"  FILE                  files to checksum\n",
		program);
}

int main(int argc, char *argv[])
{
	int ret = 0;
	char *program = argv[0];

	if (argc < 2) {
		usage(program);
		return EXIT_FAILURE;
	}

	argc--;
	argv++;

	while (argc >= 1) {
		int err = check_file(argv[0]);
		if (err < 0)
			return ret;
		else if (ret == 0 && err > 0)
			ret = err;

		argc--;
		argv++;
	}

	return ret;
}
