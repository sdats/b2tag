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
 * Utility function declarations.
 */

#ifndef UTILITIES_H
#define UTILITIES_H

#include <stdio.h>
#include <time.h>

#include "cshatag.h"

/**
 * Compare two timespec structures.
 *
 * @param ts1  Timespec structure to compare.
 * @param ts2  Timespec structure to compare.
 *
 * @retval <0 @p ts1 is earlier than @p ts2.
 * @retval 0 @p ts1 and @p ts2 are equal.
 * @retval >0 @p ts1 is later than @p ts2.
 */
int ts_compare(struct timespec ts1, struct timespec ts2);

/**
 * Prints an error message to stderr and exits the program.
 *
 * @param fmt  The printf-style format string to display.
 * @param ...  Additional arguments for @p fmt.
 */
void die(const char *fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));

/**
 * Prints version information for cshatag.
 */
void version(void);

#define _check_level(level) (args.verbose >= (level))

#define check_crit()  _check_level(-1)
#define check_err()   _check_level(0)
#define check_warn()  _check_level(1)
#define check_info()  _check_level(2)
#define check_debug() _check_level(3)

#define _print(level, ...) \
	do { \
		if (check_##level()) \
			fprintf(stderr, __VA_ARGS__); \
	} while (0)

#define pr_crit(...)  _print(crit,  __VA_ARGS__)
#define pr_err(...)   _print(err,   __VA_ARGS__)
#define pr_warn(...)  _print(warn,  __VA_ARGS__)
#define pr_info(...)  _print(info,  __VA_ARGS__)
#define pr_debug(...) _print(debug, __VA_ARGS__)

#endif /* UTILITIES_H */
