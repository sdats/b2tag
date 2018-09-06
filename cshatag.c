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

#include <assert.h>
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

static char const * const file_state_str[] = {
	"OK",
	"BACKDATED",
	"CORRUPT",
	"INVALID",
	"NEW",
	"OUTDATED",
	"SKIPPED"
};

enum file_state {
	FILE_OK,
	FILE_BACKDATED,
	FILE_CORRUPT,
	FILE_INVALID,
	FILE_NEW,
	FILE_OUTDATED,
	FILE_SKIPPED
};

/**
 * Prints information about a file's state.
 *
 * @param state     The file's state (e.g. ok).
 * @param filename  The name of the file.
 * @param stored    The file's stored attributes (may be NULL).
 * @param actual    The file's actual attributes (may be NULL).
 *
 * @retval 0  The file was processed successfully.
 * @retval >0 An recoverable error occurred.
 * @retval <0 A fatal error occurred.
 */
void print_state(enum file_state state, const char *filename, xa_t *stored, xa_t *actual)
{
	bool print_status = check_err();
	FILE *output = (args.check) ? stdout : stderr;

	switch (state) {
	case FILE_OK:
		if (!args.check)
			print_status = check_warn();

	case FILE_NEW:
	case FILE_OUTDATED:
	case FILE_SKIPPED:
		if (!print_status)
			break;

		fprintf(output, "%s: %s\n", filename, file_state_str[state]);
		break;

	case FILE_INVALID:
	case FILE_BACKDATED:
	case FILE_CORRUPT:
		if (!check_crit())
			break;

		fprintf(output, "%s: %s\n", filename, file_state_str[state]);
		break;

	default:
		pr_err("Unknown file state: %d\n", (int)state);
		return;
	}

	if (check_info()) {
		if (stored != NULL)
			fprintf(output, "# stored: %s\n", xa_format(stored));
		if (actual != NULL)
			fprintf(output, "# actual: %s\n", xa_format(actual));
	}
}

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
	int ret = 0;
	int err;
	int fd;
	struct stat st;
	xa_t a;
	xa_t s;
	bool update_xattrs = false;
	bool has_xattrs = false;

	assert(filename != NULL);

	a = s = (xa_t){ .alg = args.alg };

	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		pr_err("Error: could not open file \"%s\": %m\n", filename);
		return 1;
	}

	fstat(fd, &st);
	a.mtime = st.st_mtim;

	err = xa_read(fd, &s);
	if (err == 0)
		has_xattrs = true;
	else if (err < 0) {
		print_state(FILE_INVALID, filename, NULL, NULL);

		if (args.update && args.force) {
			xa_compute(fd, &a);

			assert(strlen(a.hash) == (size_t)get_alg_size(a.alg) * 2);

			if (xa_write(fd, &a) != 0) {
				pr_err("Error: could not write extended attributes to file \"%s\": %m\n", filename);
				return 2;
			}
		}

		return 1;
	}

	if (has_xattrs)
		err = ts_compare(s.mtime, a.mtime);
	else
		err = -1;

	/* Quick check. If stored timestamps match, skip hashing. */
	if (!args.check && err == 0) {
		print_state(FILE_OK, filename, &s, NULL);
		return 0;
	}

	/* Skip new files. */
	if (!args.tag && !has_xattrs) {
		print_state(FILE_SKIPPED, filename, &s, NULL);
		return 0;
	}

	xa_compute(fd, &a);

	assert(strlen(a.hash) == (size_t)get_alg_size(a.alg) * 2);
	assert((unsigned long)s.alg == (unsigned long)a.alg);

	if (!has_xattrs) {
		print_state(FILE_NEW, filename, NULL, &a);
		update_xattrs = true;
	}
	else if (strcmp(s.hash, a.hash) == 0) {
		print_state(FILE_OK, filename, NULL, &a);
		update_xattrs = (err != 0); /* Update xattrs if timestamps differ. */
	}
	else {
		update_xattrs = true;

		if (err < 0)
			print_state(FILE_OUTDATED, filename, &s, &a);
		else {
			enum file_state state = FILE_CORRUPT;

			if (err > 0)
				state = FILE_BACKDATED;

			print_state(state, filename, &s, &a);
			update_xattrs = args.force;
			ret = 1;
		}
	}

	if (args.update && update_xattrs && xa_write(fd, &a) != 0) {
		pr_err("Error: could not write extended attributes to file \"%s\": %m\n", filename);
		ret = 2;
	}

	close(fd);

	return ret;
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
		"  -c, --check           check the hashes on all specified files\n"
		"  -f, --force           update the stored hashes for backdated, corrupted, or\n"
		"                        invalid files\n"
		"  -h, --help            show this help message and exit\n"
		"  -q, --quiet           only print errors (including checksum failures)\n"
		"  -t, --tag             compute new checksums for files that don't have them\n"
		"                        and update any outdated checksums\n"
		"  -u, --update          update outdated checksums only (ignore new files)\n"
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
	{ "force",      no_argument, 0, 'f' },
	{ "help",       no_argument, 0, 'h' },
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

	while ((opt = getopt_long(argc, argv, "cfhqtuvV", long_opts, &option_index)) != -1) {
		switch (opt) {
		case 0:
			args.alg = long_opts[option_index].name;
			break;
		case 'c':
			args.check = true;
			break;
		case 'f':
			args.force = true;
			break;
		case 'h':
			usage(program);
			return EXIT_SUCCESS;
		case 'q':
			args.verbose--;
			break;
		case 't':
			args.tag = true;
			args.update = true;
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
		default:
			if (isprint(opt))
				fprintf(stderr, "Unknown argument '-%c' (%#02x)\n", opt, opt);
			else
				fprintf(stderr, "Unknown argument -%#02x\n", opt);

			/* Fall through. */

		case '?':
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
