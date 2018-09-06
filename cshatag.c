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
 * Main file for the cshatag utility.
 */

/**
 * @mainpage
 * @include README
 */

#include "cshatag.h"

#include <ctype.h>
#include <fcntl.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>

#include "utilities.h"
#include "xa.h"


/** The hash algorithm to use. */
#define DEFAULT_HASHALG "sha256"

/** The options set by command-line arguments. */
struct args_s args = (struct args_s){ 0 };


/**
 * Checks if a file's stored hash and timestamp match the current values.
 *
 * @param filename  The file to check.
 *
 * @retval 0  The file was processed successfully.
 * @retval >0 An recoverable error occurred.
 * @retval <0 A fatal error occurred.
 */
static int check_file(const char *filename)
{
	int fd;
	struct stat st;
	xa_t a;
	xa_t s;
	bool needsupdate = false;
	bool havecorrupt = false;

	a = s = (xa_t){ .alg = args.alg };

	fd = open(filename, O_RDONLY);
	if (fd < 0)
		die("Error: could not open file \"%s\": %m\n", filename);

	fstat(fd, &st);
	a.mtime = st.st_mtim;

	xa_read(fd, &s);
	if (strlen(s.hash) != (size_t)get_alg_size(s.alg) * 2)
		die("Stored hash size mismatch: Expected %d, got %zu.\n", get_alg_size(s.alg), strlen(s.hash));

	if (!args.check) {
		/* Quick check. If stored timestamps match, skip hashing. */
		if (ts_compare(s.mtime, a.mtime) == 0) {
			if (args.verbose >= 1) {
				printf("<ok> %s\n", filename);
				if (args.verbose >= 2)
					printf(" stored: %s\n", xa_format(&s));
			}

			return 0;
		}
	}

	/* Skip existing files. */
	if (!args.update && (s.mtime.tv_sec != 0 || s.mtime.tv_nsec != 0)) {
		if (args.verbose >= 1) {
			printf("<skip> %s\n", filename);
			if (args.verbose >= 2)
				printf(" stored: %s\n", xa_format(&s));
		}

		return 0;
	}

	/* Skip new files. */
	if (!args.tag && s.mtime.tv_sec == 0 && s.mtime.tv_nsec == 0) {
		if (args.verbose >= 1) {
			printf("<skip> %s\n", filename);
			if (args.verbose >= 2)
				printf(" stored: %s\n", xa_format(&s));
		}

		return 0;
	}

	xa_compute(fd, &a);
	if (strlen(a.hash) != (size_t)get_alg_size(a.alg) * 2)
		die("Computed hash size mismatch: Expected %d, got %zu.\n", get_alg_size(a.alg), strlen(a.hash));

	if ((unsigned long)s.alg != (unsigned long)a.alg)
		die("Algorithm mismatch: \"%s\" != \"%s\"\n", s.alg, a.alg);

	if (strcmp(s.hash, a.hash) != 0) {
		needsupdate = true;

		if (ts_compare(s.mtime, a.mtime) == 0) {
			/* Now, this is either data corruption or somebody modified the file
			 * and reset the mtime to the last value (to hide the modification?)
			 */
			fprintf(stderr, "Error: corrupt file \"%s\"\n", filename);
			if (args.verbose >= -1) {
				printf("<corrupt> %s\n", filename);
				if (args.verbose >= 2) {
					printf(" stored: %s\n", xa_format(&s));
					printf(" actual: %s\n", xa_format(&a));
				}
			}
			havecorrupt = true;
		}
		else if (args.verbose >= 0) {
			if (s.mtime.tv_sec == 0 && s.mtime.tv_nsec == 0)
				printf("<new> %s\n", filename);
			else if (ts_compare(s.mtime, a.mtime) < 0)
				printf("<outdated> %s\n", filename);
			else
				printf("<backdated> %s\n", filename);

			if (args.verbose >= 2) {
				printf(" stored: %s\n", xa_format(&s));
				printf(" actual: %s\n", xa_format(&a));
			}
		}
	}
	else if (args.verbose >= 1) {
		printf("<ok> %s\n", filename);
		if (args.verbose >= 2)
			printf(" actual: %s\n", xa_format(&a));
	}

	if (needsupdate && !args.dry_run && xa_write(fd, &a) != 0)
		die("Error: could not write extended attributes to file \"%s\": %m\n", filename);

	close(fd);

	return (havecorrupt) ? 5 : 0;
}

/**
 * Prints a usage message for cshatag.
 *
 * @param program  The name of the program being run.
 */
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
		"  -c, --check           check the hashes on all specified files.\n"
		"  -h, --help            show this help message and exit\n"
		"  -n, --dry-run         do not modify any stored checksums\n"
		"  -q, --quiet           only print errors (including checksum failures)\n"
		"  -t, --tag             compute new checksums for files that don't have them.\n"
		"  -u, --update          update outdated checksums.\n"
		"  -v, --verbose         print all checksums (not just missing/changed)\n"
		"  -V, --version         output version information and exit\n"
		"\n"
		"Hash algorithms:\n"
		"  --md5 (deprecated)    --sha1 (deprecated)\n"
		"  --sha256 (default)    --sha512 (recommended on 64-bit)\n"
		"  --blake2s256          --blake2b512\n"
		"\n"
		"Note: the original shatag python utility only supports sha256.\n",
		program);
}

/**
 * Long options to pass to getopt.
 */
static const struct option long_opts[] = {
	{ "check",      no_argument, 0, 'c' },
	{ "help",       no_argument, 0, 'h' },
	{ "dry-run",    no_argument, 0, 'n' },
	{ "quiet",      no_argument, 0, 'q' },
	{ "tag",        no_argument, 0, 't' },
	{ "update",     no_argument, 0, 'u' },
	{ "verbose",    no_argument, 0, 'v' },
	{ "version",    no_argument, 0, 'V' },
	{ "md5",        no_argument, 0,  0  },
	{ "sha1",       no_argument, 0,  0  },
	{ "sha256",     no_argument, 0,  0  },
	{ "sha512",     no_argument, 0,  0  },
	{ "blake2s256", no_argument, 0,  0  },
	{ "blake2b512", no_argument, 0,  0  },
	{ NULL, 0, 0, 0 }
};

/**
 * The entry point to the cshatag utility.
 *
 * @param argc  The number of command-line arguments.
 * @param argv  The command-line arguments.
 *
 * @retval 0  Program completed successfully.
 * @retval !0 An error occurred.
 */
int main(int argc, char *argv[])
{
	int ret = 0;
	char *program = basename(argv[0]);
	int opt;
	int option_index = 0;

	args.alg = DEFAULT_HASHALG;

	while ((opt = getopt_long(argc, argv, "chnqtuvV", long_opts, &option_index)) != -1) {
		switch (opt) {
		case 0:
			args.alg = long_opts[option_index].name;
			break;
		case 'c':
			args.check = true;
			break;
		case 'h':
			usage(program);
			return EXIT_SUCCESS;
		case 'n':
			args.dry_run = true;
			break;
		case 'q':
			args.verbose--;
			break;
		case 't':
			args.tag = true;
			break;
		case 'u':
			args.update = true;
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
