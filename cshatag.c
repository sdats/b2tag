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

#include <ctype.h>
#include <fcntl.h>
#include <getopt.h>
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

static int check_file(const char *filename, args_t *args)
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

	if (strcmp(s.hash, a.hash) != 0) {
		needsupdate = 1;

		/* Hashes are different, check if the file mod time has been updated. */
		getmtime(fd, &a);

		if (s.sec == a.sec && s.nsec == a.nsec) {
			/*
			 * Now, this is either data corruption or somebody modified the file
			 * and reset the mtime to the last value (to hide the modification?)
			 */
			fprintf(stderr, "Error: corrupt file \"%s\"\n", filename);
			if (args->verbose >= -1) {
				printf("<corrupt> %s\n", filename);
				if (args->verbose >= 2) {
					printf(" stored: %s\n", xa_format(&s));
					printf(" actual: %s\n", xa_format(&a));
				}
			}
			havecorrupt = 1;
		}
		else if (args->verbose >= 0) {
			if (s.sec == 0 && s.nsec == 0)
				printf("<new> %s\n", filename);
			else if (s.sec < a.sec || (s.sec == a.sec && s.nsec < a.nsec))
				printf("<outdated> %s\n", filename);
			else
				printf("<backdated> %s\n", filename);

			if (args->verbose >= 2) {
				printf(" stored: %s\n", xa_format(&s));
				printf(" actual: %s\n", xa_format(&a));
			}
		}
	}
	else if (args->verbose >= 1) {
		printf("<ok> %s\n", filename);
		if (args->verbose >= 2) {
			printf(" stored: %s\n", xa_format(&s));
			printf(" actual: %s\n", xa_format(&a));
		}
	}

	if (args->dry_run)
		needsupdate = false;

	if (needsupdate && xa_write(fd, &a) != 0)
		die("Error: could not write extended attributes to file \"%s\": %m\n", filename);

	close(fd);

	if (havecorrupt)
		return 5;
	else
		return 0;
}

static void usage(const char *program)
{
	printf(
		"Usage: %s [OPTION]... <FILE>...\n"
		"\n"
		"Display and update xattr-based checksums.\n"
		"\n"
		"Positional arguments:\n"
		"  FILE                  files to checksum\n"
		"\n"
		"Optional arguments:\n"
		"  -h, --help            show this help message and exit\n"
		"  -n, --dry-run         do not create or update any checksums\n"
		"  -q, --quiet           only print errors (including checksum failures)\n"
		"  -v, --verbose         print all checksums (not just missing/changed)\n"
		"  -V, --version         output version information and exit\n",
		program);
}

static const struct option long_opts[] = {
	{ "help",      no_argument, 0, 'h' },
	{ "dry-run",   no_argument, 0, 'n' },
	{ "quiet",     no_argument, 0, 'q' },
	{ "verbose",   no_argument, 0, 'v' },
	{ "version",   no_argument, 0, 'V' },
	{ NULL, 0, 0, 0 }
};

int main(int argc, char *argv[])
{
	int ret = 0;
	char *program = basename(argv[0]);
	args_t args = (args_t){ 0 };
	int opt;

	while ((opt = getopt_long(argc, argv, "hnqvV", long_opts, NULL)) != -1) {
		switch (opt) {
		case 'h':
			usage(program);
			return EXIT_SUCCESS;
		case 'n':
			args.dry_run = true;
			break;
		case 'q':
			args.verbose--;
			break;
		case 'v':
			args.verbose++;
			break;
		case 'V':
			version();
			return EXIT_SUCCESS;
		case '?':
		default:
			if (isprint(opt))
				fprintf(stderr, "Unknown argument '-%c' (%#02x)\n", opt, opt);
			else
				fprintf(stderr, "Unknown argument -%#02x\n", opt);
			usage(program);
			return EXIT_FAILURE;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 1) {
		fprintf(stderr, "No file specified.\n");
		usage(program);
		return EXIT_FAILURE;
	}

	while (argc >= 1) {
		int err = check_file(argv[0], &args);
		if (err < 0)
			return ret;
		else if (ret == 0 && err > 0)
			ret = err;

		argc--;
		argv++;
	}

	return ret;
}
