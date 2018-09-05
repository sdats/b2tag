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
 * Contains miscellaneous utility functions.
 */

#include "utilities.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * Compare two timespec structures.
 *
 * @param ts1  Timespec structure to compare.
 * @param ts2  Timespec structure to compare.
 *
 * @note This function considers two timespecs within 1 microsecond to be equal.
 *
 * @retval <0 @p ts1 is earlier than @p ts2.
 * @retval 0 @p ts1 and @p ts2 are equal.
 * @retval >0 @p ts1 is later than @p ts2.
 */
int ts_compare(struct timespec ts1, struct timespec ts2)
{
	ts1.tv_sec  -= ts2.tv_sec;
	ts1.tv_nsec -= ts2.tv_nsec;

	if (ts1.tv_sec > 0)
		return 2;

	if (ts1.tv_sec < 0)
		return -2;

	/* Count timespecs within 1 usec as equal for compatibility with the
	 * original shatag python utility.
	 */
	ts1.tv_nsec /= 1000;

	if (ts1.tv_nsec > 0)
		return 1;

	if (ts1.tv_nsec < 0)
		return -1;

	return 0;
}

/**
 * Prints an error message to stderr and exits the program.
 *
 * @param fmt  The printf-style format string to display.
 * @param ...  Additional arguments for @p fmt.
 */
void die(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	exit(EXIT_FAILURE);
}

/**
 * Prints version information for cshatag.
 */
void version(void)
{
	printf("cshatag version %s\n", VERSION_STRING);
}
