#!/bin/bash
#
# Copyright (C) 2018 Tim Schlueter
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published
# by the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
#
# This is a simple sanity test for cshatag
#

RET=0

# Allow overriding the test file any test message: simply set the
# corresponding environment variable.
TEST_FILE=${TEST_FILE:-test.txt}
TEST_MESSAGE=${TEST_MESSAGE:-Hello World!}

function fail()
{
	echo "$*" >&2
	return 1
}

set -o pipefail

# set -x if the "V" environment variable is set
if [[ -z $V || $V = 0 ]]; then
	args=-q
else
	set -x
fi

if [[ -e $TEST_FILE ]]; then
	echo "Warning: test.txt already exists. Removing." >&2
	rm "$TEST_FILE" \
		|| fail "Could not remove old test file" \
		|| let RET++
fi

# Test setup: create the test file, hash the test message, and grab the
# test file's modified time
echo "$TEST_MESSAGE" > "$TEST_FILE" \
	|| fail "Could not create test file: $?" \
	|| let RET++

HASH=$(echo "$TEST_MESSAGE" | sha256sum | cut -d' ' -f1) \
	|| fail "Could not generate reference hash: $?" \
	|| let RET++

MTIME=$(stat --format='%.Y' "$TEST_FILE") \
	|| fail "Could not read test file mtime: $?" \
	|| let RET++

# Make sure the newly-created test file doesn't have the shatag xattrs
TAG_TS=$(getfattr --only-values --name=user.shatag.ts "$TEST_FILE" 2>/dev/null)
TAG_VAL=$(getfattr --only-values --name=user.shatag.sha256 "$TEST_FILE" 2>/dev/null)

[[ -z $TAG_TS ]]  \
	|| fail "Shatag timestamp already set." \
	|| let RET++
[[ -z $TAG_VAL ]] \
	|| fail "Shatag value already set." \
	|| let RET++

# Make sure cshatag adds the proper xattrs
./cshatag $args "$TEST_FILE" \
	|| fail "cshatag returned failure: $?" \
	|| let RET++

TAG_TS=$(getfattr --only-values --name=user.shatag.ts "$TEST_FILE") \
	|| fail "Could not read test file timestamp attribute: $?" \
	|| let RET++
TAG_VAL=$(getfattr --only-values --name=user.shatag.sha256 "$TEST_FILE") \
	|| fail "Could not read test file hash value attribute: $?" \
	|| let RET++

[[ $MTIME = $TAG_TS ]] \
	|| fail "Shatag timestamp mismatch: $MTIME != $TAG_TS" \
	|| let RET++
[[ $HASH = $TAG_VAL ]] \
	|| fail "Shatag value mismatch: $HASH != $TAG_VAL" \
	|| let RET++

# If the test was successful, remove the test file
if [[ $RET -eq 0 ]]; then
	echo "All tests successful"
	rm -f "$TEST_FILE"
fi

exit $RET
