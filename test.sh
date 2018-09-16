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
# In addition, as a special exception, the author of this program
# gives permission to link the code portions of this program with the
# OpenSSL library under certain conditions as described in each file,
# and distribute linked combinations including the two.
# You must obey the GNU General Public License in all respects for all
# of the code used other than OpenSSL.  If you modify this file(s)
# with this exception, you may extend this exception to your version
# of the file(s), but you are not obligated to do so.  If you do not
# wish to do so, delete this exception statement from your version.
# If you delete this exception statement from all source files in the
# program, then also delete it here.
#
# This is a simple sanity test for cshatag
#

RET=0

# Allow overriding the test file any test message: simply set the
# corresponding environment variable.
TEST_FILE=${TEST_FILE:-test.txt}
TEST_MESSAGE=${TEST_MESSAGE:-The quick brown fox jumped over the lazy dog.}

function fail() {
	echo "$*" >&2
	return 1
}

function to_hex() {
	xxd -p | tr -d '\n'
}

function from_hex() {
	xxd -p -r
}

function hash() {
	local alg="${1:-sha256}"
	local prog

	shift

	case "$alg" in
		md5|sha1|sha256|sha512)
			prog=${alg}sum
			;;
		blake2|blake2b512)
			prog=b2sum
			;;
		*)
			fail "Unknown hash algorithm: $alg"
			return 1;
			;;
	esac

	"${prog}" "$@" | cut -d' ' -f1
}

function get_mtime() {
	stat --format='%.Y' "$@"
}

function get_attr_hex() {
	local attr="$1"
	local file="$2"
	[[ $# -eq 2 ]] || return 1
	[[ $attr = blake2 ]] && attr=blake2b512
	getfattr --only-values --name "user.shatag.$attr" "$file" 2>/dev/null | to_hex
}

function clear_attr() {
	[[ $# -ge 2 ]] \
		&& setfattr -x "user.shatag.$1" "${@:2}" 2>/dev/null
}

function print_ts() {
	local ts_hex="$1"
	if [[ -z $ts_hex ]]; then
		echo "Timestamp: <empty>"
	elif [[ $ts_hex =~ ^(3[0-9]|2[Ee])+$ ]]; then
		echo "Timestamp: $(from_hex <<<"$ts_hex")"
	else
		echo "Timestamp:"
		# Do a pretty hex dump rather than the plain hex
		from_hex <<<"$ts_hex" | xxd
	fi
}

function print_hash() {
	local hash_hex="$1"
	local alg="$2"
	if [[ -z $hash_hex ]]; then
		echo "${alg^} hash: <empty>"
	elif [[ $hash_hex =~ ^(3[0-9]|[46][1-6])+$ ]]; then
		echo "${alg^} hash: $(from_hex <<<"$hash_hex")"
	else
		echo "${alg^} hash:"
		# Do a pretty hex dump rather than the plain hex
		from_hex <<<"$hash_hex" | xxd
	fi
}

function check_ts() {
	local err expect file ts ts_hex

	file="$1"
	# Blank expect = timestamp should be unset
	expect="$2"

	ts_hex=$(get_attr_hex ts "$file")
	err=$?

	if [[ -z $expect ]]; then
		if [[ $err -eq 0 ]]; then
			fail "Timestamp already set."
			print_ts "$ts_hex"
			return 1
		elif [[ -n $ts_hex ]]; then
			fail "Timestamp is not empty."
			print_ts "$ts_hex"
			return 1
		fi

		return 0
	fi

	if [[ $err -ne 0 ]]; then
		fail "Could not read timestamp attribute: $err"
		return 1
	elif [[ -z $ts_hex ]]; then
		fail "Empty timestamp attribute (expected $expect)"
		return 1
	elif [[ $ts_hex =~ ^(3[0-9]|2[Ee])+$ ]]; then
		ts=$(from_hex <<<"$ts_hex")
		if [[ ! $expect = $ts ]]; then
			fail "Timestamp mismatch: '$expect' != '$ts'"
			return 1
		fi
	else
		fail "Timestamp contains non-numeric characters (0-9 and .)"
		print_ts "$ts_hex"
		return 1
	fi

	return 0
}

function check_hash() {
	local alg err expect file hash hash_hex

	file="$1"
	# Blank expect = hash should be unset
	expect="$2"

	alg="${3:-sha256}"

	hash_hex=$(get_attr_hex "$alg" "$file")
	err=$?

	if [[ -z $expect ]]; then
		if [[ $err -eq 0 ]]; then
			fail "${alg^} hash already set."
			print_hash "$hash_hex" "$alg"
			return 1
		elif [[ -n $hash_hex ]]; then
			fail "${alg^} hash is not empty."
			print_hash "$hash_hex" "$alg"
			return 1
		fi

		return 0
	fi

	if [[ $err -ne 0 ]]; then
		fail "Could not read $alg hash attribute: $err"
		return 1
	elif [[ -z $hash_hex ]]; then
		fail "Empty $alg hash attribute (expected $expect)"
		return 1
	elif [[ $hash_hex =~ ^(3[0-9]|[46][1-6])+$ ]]; then
		hash=$(from_hex <<<"$hash_hex")
		if [[ ! $expect = $hash ]]; then
			fail "${alg^} hash mismatch: '$expect' != '$hash'"
			return 1
		fi
	else
		fail "${alg^} hash contains non-numeric characters (0-9 and .)"
		print_hash "$hash_hex" "$alg"
		return 1
	fi

	return 0
}

set -o pipefail

# set -x if the "V" environment variable is set and non-zero
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

echo "$TEST_MESSAGE" > "$TEST_FILE" \
	|| fail "Could not create test file: $?" \
	|| let RET++

check_ts   "$TEST_FILE" "" || let RET++
check_hash "$TEST_FILE" "" $ALG || let RET++

# Test setup: create the test file, hash the test message, and grab the
# test file's modified time
for ALG in '' md5 sha1 sha256 sha512 blake2; do
	HASH=$(echo "$TEST_MESSAGE" | hash $ALG) \
		|| fail "Could not generate $ALG reference hash: $?" \
		|| let RET++

	MTIME=$(get_mtime "$TEST_FILE") \
		|| fail "Could not read test file mtime: $?" \
		|| let RET++

	ALG_NAME="${ALG:-default}"

	clear_attr ts "$TEST_FILE"

	# Make sure the newly-created test file doesn't have the shatag xattrs
	echo "Test sanity ($ALG_NAME)"
	check_ts   "$TEST_FILE" "" || let RET++
	check_hash "$TEST_FILE" "" $ALG || let RET++

	# Make sure cshatag doesn't add xattrs when -n is given
	echo "Test dry-run ($ALG_NAME)"
	./cshatag -n $args --$ALG "$TEST_FILE" \
		|| fail "cshatag returned failure: $?" \
		|| let RET++

	check_ts   "$TEST_FILE" "" || let RET++
	check_hash "$TEST_FILE" "" $ALG || let RET++

	# Make sure cshatag adds the proper xattrs
	echo "Test new file ($ALG_NAME)"
	./cshatag $args --$ALG "$TEST_FILE" \
		|| fail "cshatag returned failure: $?" \
		|| let RET++

	check_ts   "$TEST_FILE" "$MTIME" || let RET++
	check_hash "$TEST_FILE" "$HASH" $ALG || let RET++

	# Print test
	echo "Test print file hashes ($ALG_NAME)"
	./cshatag -p $args --$ALG "$TEST_FILE" | hash "$ALG" -c - >/dev/null \
		|| fail "cshatag returned failure: $?" \
		|| let RET++

	if [[ -z $ALG ]]; then
		clear_attr sha256 "$TEST_FILE"
	fi
done

# If the test was successful, remove the test file
if [[ $RET -eq 0 ]]; then
	echo "All tests successful"
	rm -f "$TEST_FILE"
else
	echo "$RET test failures"
fi

exit $RET
