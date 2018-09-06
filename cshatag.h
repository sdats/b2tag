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
 * Shared functions for the cshatag utility.
 */

#ifndef CSHATAG_H
#define CSHATAG_H

#include <stdbool.h>

/**
 * The options passed to the program on the command-line.
 */
struct args_s {
	/** Which hash algorithm to use. */
	const char *alg;
	/** The verbosity level (how many messages to print). */
	int verbose;
	/** Whether to check the hashes on up-to-date files. */
	bool check;
	/** Don't change any extended attributes. */
	bool dry_run;
	/** Only compute the checksums of new files. */
	bool tag;
	/** Only compute the checksums of outdated files. */
	bool update;
};

/** The options set by command-line arguments. */
extern struct args_s args;

#endif /* CSHATAG_H */
