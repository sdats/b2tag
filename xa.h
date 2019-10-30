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
 * Extended attribute functions and data structures.
 */

#ifndef XA_H
#define XA_H

#include <stdbool.h>

#include <sys/time.h>

#include "hash.h"

/**
 * Types of errors returned.
 */
typedef enum err {
	E_OK = 0,
	E_IO_ERROR = 1,
	E_NOT_FOUND = 2,
	E_INVALID = 3,
	E_UNSUPPORTED = 4,
} err_t;

/*
 * Low-level attribute operations.
 */

/**
 * Read the timestamp from the extended attributes of @p fd.
 * 
 * @param fd     The file which extended attributes to operate on.
 * @param mtime  Where to store the result.
 * @param fuzzy  Whether timestamp looks truncated.
 * 
 * @retval E_OK           The timestamp was read and parsed successfully.
 * @retval E_IO_ERROR     An error occurred while reading the attribute.
 * @retval E_NOT_FOUND    The corresponding attribute is not present.
 * @retval E_INVALID      The corresponding attribute contains invalid data.
 * @retval E_UNSUPPORTED  Extended attributes are not supported.
 */
err_t xa_read_timestamp(int fd, struct timespec* mtime, bool* truncated);

/**
 * Write the timestamp into the extended attributes of @p fd.
 * 
 * @param fd     The file which extended attributes to operate on.
 * @param mtime  The timestamp to write.
 * 
 * @retval E_OK           The timestamp was written successfully.
 * @retval E_IO_ERROR     An error occurred while writing the attribute.
 * @retval E_UNSUPPORTED  Extended attributes are not supported.
 */
err_t xa_write_timestamp(int fd, const struct timespec mtime);

/**
 * Remove the timestamp from the extended attributes of @p fd.
 * 
 * @param fd     The file which extended attributes to operate on.
 * 
 * @retval E_OK           The timestamp was removed successfully.
 * @retval E_IO_ERROR     An error occurred while removing the attribute.
 * @retval E_NOT_FOUND    The corresponding attribute is not present.
 * @retval E_UNSUPPORTED  Extended attributes are not supported.
 */
err_t xa_remove_timestamp(int fd);

/**
 * Read the checksum for the algorithm @p alg from the extended attributes of @p fd.
 * 
 * @param fd        The file which extended attributes to operate on.
 * @param alg       The algorithm for which the checksum should be read.
 * @param checksum  Where to store the result. Must be big enough to store at least
 *                  MAX_HASH_STRING_LENGTH + 1 characters.
 * 
 * @retval E_OK           The hash was read and parsed successfully.
 * @retval E_IO_ERROR     An error occurred while reading the attribute.
 * @retval E_NOT_FOUND    The corresponding attribute is not present.
 * @retval E_INVALID      The corresponding attribute contains invalid data.
 * @retval E_UNSUPPORTED  Extended attributes are not supported.
 */
err_t xa_read_checksum(int fd, hash_alg_t alg, char* checksum);

/**
 * Write the checksum into the extended attributes of @p fd.
 * 
 * @param fd        The file which extended attributes to operate on.
 * @param alg       The type of the algorithm that produced the checksum.
 * @param checksum  The null-terminated string representation of the checksum to write.
 * 
 * @retval E_OK           The checksum was written successfully.
 * @retval E_IO_ERROR     An error occurred while writing the attribute.
 * @retval E_UNSUPPORTED  Extended attributes are not supported.
 */
err_t xa_write_checksum(int fd, hash_alg_t alg, const char* checksum);

/**
 * Remove the checksum for algorithm @p alg from the extended attributes of @p fd.
 * 
 * @param fd     The file which extended attributes to operate on.
 * @param alg    The algorithm for which the checksum should be removed.
 * 
 * @retval E_OK           The checksum was removed successfully.
 * @retval E_IO_ERROR     An error occurred while removing the attribute.
 * @retval E_NOT_FOUND    The corresponding attribute is not present.
 * @retval E_UNSUPPORTED  Extended attributes are not supported.
 */
err_t xa_remove_checksum(int fd, hash_alg_t alg);

/**
 * Metadata structure for b2tag.
 */
typedef struct xa_s
{
	/** Whether the xattr struct contains a valid hash. */
	bool valid;
	/** Whether the mtime is inaccurate, see COMPATIBILITY in b2tag(1). */
	bool fuzzy;
	/** The file's last modified time. */
	struct timespec mtime;
	/** The hash algorithm to use. */
	hash_alg_t alg;
	/** The file data's hash value as an ASCII hex string. */
	char hash[MAX_HASH_STRING_LENGTH + 1];
} xa_t;

/**
 * Clear the timestamp and hash values in @p xa.
 *
 * @li @p xa->alg will be left untouched.
 * @li @p xa->hash will be set to a string of ASCII '0's the same length as @p xa->alg
 *     (and NUL-terminated).
 * @li The rest of @p xa will be zeroed.
 *
 * @param xa  The extended attribute structure to clear.
 */
void xa_clear(xa_t *xa);

/**
 * Hash the contents of @p fd and store the result in @p xa.
 *
 * Additionally, retrieve the last mtime of @p fd and store it in @p xa
 * (unless @p xa already contains a non-zero mtime).
 *
 * @param fd  The file to compute the hash of.
 * @param xa  The extended attribute structure to store the values in.
 *
 * @retval 0  The contents of @p fd were successfully hashed.
 * @retval !0 An error occurred while hashing the contents of @p fd.
 */
int xa_compute(int fd, xa_t *xa);

/**
 * Retrieve the stored extended attributes for @p fd and store it in @p xa.
 *
 * @param fd  The file to retrieve the extended attributes from.
 * @param xa  The extended attribute structure to store the values in.
 *
 * @retval -1  An error occurred reading the extended attributes.
 * @retval  0  The extended attributes were successfully read.
 * @retval  1  The file does not have the shatag extended attributes.
 * @retval  2  The shatag extended attributes are corrupted.
 */
int xa_read(int fd, xa_t *xa);

/**
 * Update the stored extended attributes for @p fd from @p xa.
 *
 * @param fd  The file to update the extended attributes of.
 * @param xa  The extended attribute structure to store to disk.
 *
 * @retval 0  The extended attributes were successfully updated.
 * @retval !0 An error occurred updating the extended attributes.
 */
int xa_write(int fd, xa_t *xa);

/**
 * Convert an extended attribute structure into a human-readable form for printing.
 *
 * @param xa  The extended attribute structure to convert.
 *
 * @returns A string containing the human-readable extended attribute structure.
 *
 * @note This function uses a static buffer to format the string. It will be
 *       overwritten by successive calls to xa_format() and is not thread-safe.
 */
const char *xa_format(xa_t *xa);

#endif /* XA_H */
