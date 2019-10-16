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
 *
 * In addition, as a special exception, the author of this program
 * gives permission to link the code portions of this program with the
 * OpenSSL library under certain conditions as described in each file,
 * and distribute linked combinations including the two.
 * You must obey the GNU General Public License in all respects for all
 * of the code used other than OpenSSL.  If you modify this file(s)
 * with this exception, you may extend this exception to your version
 * of the file(s), but you are not obligated to do so.  If you do not
 * wish to do so, delete this exception statement from your version.
 * If you delete this exception statement from all source files in the
 * program, then also delete it here.
 */

/** @file
 * Contains utility functions.
 */

#include "utilities.h"

#include <stdarg.h>
#include <stdlib.h>

int ts_compare(struct timespec ts1, struct timespec ts2, bool fuzzy)
{
	ts1.tv_sec  -= ts2.tv_sec;
	ts1.tv_nsec -= ts2.tv_nsec;

	if (ts1.tv_sec > 0)
		return 2;

	if (ts1.tv_sec < 0)
		return -2;

	if (fuzzy) {
		/* Count timespecs within 1 usec as equal for compatibility with the
		 * original shatag python utility.
		 */
		ts1.tv_nsec /= 1000;
	}

	if (ts1.tv_nsec > 0)
		return 1;

	if (ts1.tv_nsec < 0)
		return -1;

	return 0;
}

void die(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	exit(EXIT_FAILURE);
}
