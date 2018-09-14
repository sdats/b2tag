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
 * File helper function declarations.
 */

#ifndef FILE_H
#define FILE_H

/**
 * Figure out whether a file path is a file or directory and process it.
 *
 * If @p filename is a regular file, this will pass it to check_file().
 *
 * If @p filename is a directory and --recursive was set on the command-line,
 * this will pass it on to check_dir().
 *
 * @param filename  The path to check.
 *
 * @retval 0  The file was processed successfully.
 * @retval >0 An recoverable error occurred.
 * @retval <0 A fatal error occurred.
 */
int process_path(const char *filename);

#endif /* FILE_H */
