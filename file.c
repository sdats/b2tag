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
 */

/** @file
 * Checks files against their stored hashes and print the statuses.
 */

#include "file.h"

#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "utilities.h"
#include "xa.h"

enum file_state {
	FILE_FAULT,     /**< An error occurred reading file. */
	FILE_OK,        /**< File hash and mtime matches. */
	FILE_SAME,      /**< File hash matches, mtime doesn't. */
	FILE_NEW,       /**< File doesn't have stored hash or mtime. */
	FILE_OUTDATED,  /**< File hash differs, mtime is newer. */
	FILE_BACKDATED, /**< File hash differs, mtime is older. */
	FILE_CORRUPT,   /**< File hash differs, mtime matches. */
	FILE_INVALID,   /**< Xattrs corrupted. */
};

static char const * const file_state_str[] = {
	"FAULT",
	"OK",
	"OK",
	"NEW",
	"OUTDATED",
	"BACKDATED",
	"CORRUPT",
	"INVALID",
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
static void print_state(enum file_state state, const char *filename, xa_t *stored, xa_t *actual)
{
	bool print_status;

	switch (state) {
	case FILE_BACKDATED:
	case FILE_CORRUPT:
	case FILE_FAULT:
	case FILE_INVALID:
		print_status = check_crit();
		break;

	default:
		print_status = check_err();
		break;
	}

	if (!print_status)
		return;

	printf("%s: %s\n", filename, file_state_str[state]);

	if (check_info()) {
		if (stored != NULL && stored->valid)
			printf("# stored: %s\n", xa_format(stored));
		if (actual != NULL && actual->valid)
			printf("# actual: %s\n", xa_format(actual));
	}
}

/**
 * Checks if a file's stored hash and timestamp match the current values.
 *
 * @note @p stored and @p actual may be filled with data depending on the
 *       file's state (but may not be so check the xa_t::valid field).
 *       Also, @p actual->mtime will not be changed if non-zero.
 *
 * @param[in]  fd         The file to get the state of.
 * @param[out] stored     The xa structure to hold the file's stored attributes.
 * @param[in,out] actual  The xa structure to hold the file's current hash+mtime.
 *
 * @returns Returns the file's state.
 *
 * @see file_state
 */
static enum file_state get_file_state(int fd, xa_t *stored, xa_t *actual)
{
	int err;
	int comparison;

	assert(fd >= 0);
	assert(stored != NULL);
	assert(actual != NULL);
	assert((void *)stored->alg == (void *)actual->alg);

	/* Skip the fstat call if mtime seconds is already set. */
	if (actual->mtime.tv_sec == 0) {
		struct stat st;

		err = fstat(fd, &st);
		if (err != 0)
			return FILE_FAULT;

		actual->mtime = st.st_mtim;
	}

	err = xa_read(fd, stored);
	if (err < 0) {
		xa_compute(fd, actual);
		return FILE_INVALID;
	}

	if (err > 0) {
		xa_compute(fd, actual);
		return FILE_NEW;
	}

	comparison = ts_compare(stored->mtime, actual->mtime);

	/* Quick check. If stored timestamps match, skip hashing. */
	if (comparison == 0 && !args.check)
		return FILE_OK;

	xa_compute(fd, actual);

	/* hash and mtime matches -> ok, hash matches and mtime differs -> same */
	if (strcmp(stored->hash, actual->hash) == 0)
		return (comparison == 0) ? FILE_OK : FILE_SAME;

	/* file mtime is newer than the xattr mtime. */
	if (comparison < 0)
		return FILE_OUTDATED;

	/* file mtime is older than the xattr mtime. */
	if (comparison > 0)
		return FILE_BACKDATED;

	/* Same timestamp, different hashes. */
	return FILE_CORRUPT;
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
int check_file(const char *filename)
{
	enum file_state state;
	int ret = 0;
	int err;
	int fd;
	struct stat st;
	xa_t a;
	xa_t s;

	assert(filename != NULL);

	a = s = (xa_t){ .alg = args.alg };

	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		pr_err("Error: could not open file \"%s\": %m\n", filename);
		return 1;
	}

	err = fstat(fd, &st);
	if (err != 0) {
		pr_err("Error: could not stat file \"%s\": %m\n", filename);
		ret = -1;
		goto close_out;
	}

	if (!S_ISREG(st.st_mode)) {
		pr_err("Error: \"%s\": not a regular file\n", filename);
		ret = 1;
		goto close_out;
	}

	a.mtime = st.st_mtim;

	state = get_file_state(fd, &s, &a);
	if (state == FILE_FAULT) {
		pr_err("Error: failed to get file state \"%s\": %m\n", filename);
		ret = -1;
		goto close_out;
	}

	print_state(state, filename, &s, &a);

	if (state == FILE_OK)
		goto close_out;

	if (args.dry_run)
		goto close_out;

	/* Don't update the stored xattrs unless -f is specified for backdated,
	 * corrupt, fault, or invalid files.
	 */
	if (!args.force) {
		if (state == FILE_BACKDATED)
			goto close_out;
		if (state == FILE_CORRUPT)
			goto close_out;
		if (state == FILE_FAULT)
			goto close_out;
		if (state == FILE_INVALID)
			goto close_out;
	}

	err = xa_write(fd, &a);
	if (err != 0) {
		pr_err("Error: could not write extended attributes to file \"%s\": %m\n", filename);
		ret = 2;
	}

close_out:
	close(fd);
	return ret;
}
