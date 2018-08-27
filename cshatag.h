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

#ifndef CSHATAG_H
#define CSHATAG_H

#include <stdbool.h>

typedef struct args_s {
	int verbose;
	bool dry_run;
} args_t;

void die(const char *fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));

void version(void);

#endif /* CSHATAG_H */
