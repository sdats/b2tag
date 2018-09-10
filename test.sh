#!/bin/bash

RET=0

TEST_FILE=${TEST_FILE:-test.txt}
TEST_MESSAGE=${TEST_MESSAGE:-Hello World!}

function fail()
{
	echo "$*" >&2
	RET=$((RET++))
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
		|| fail "Could not remove old test file"
fi

echo "$TEST_MESSAGE" > "$TEST_FILE" \
	|| fail "Could not create test file: $?"

HASH=$(echo "$TEST_MESSAGE" | sha256sum | cut -d' ' -f1) \
	|| fail "Could not generate reference hash: $?"

MTIME=$(stat --format='%.Y' "$TEST_FILE") \
	|| fail "Could not read test file mtime: $?"

TAG_TS=$(getfattr --only-values --name=user.shatag.ts "$TEST_FILE" 2>/dev/null)
TAG_VAL=$(getfattr --only-values --name=user.shatag.sha256 "$TEST_FILE" 2>/dev/null)

[[ -z $TAG_TS ]]  \
	|| fail "Shatag timestamp already set."
[[ -z $TAG_VAL ]] \
	|| fail "Shatag value already set."

./cshatag $args "$TEST_FILE" \
	|| fail "cshatag returned failure: $?"

TAG_TS=$(getfattr --only-values --name=user.shatag.ts "$TEST_FILE" 2>/dev/null)
TAG_VAL=$(getfattr --only-values --name=user.shatag.sha256 "$TEST_FILE" 2>/dev/null)

[[ -z $TAG_TS ]]  \
	|| fail "Shatag timestamp already set."
[[ -z $TAG_VAL ]] \
	|| fail "Shatag value already set."

./cshatag $args -t "$TEST_FILE" \
	|| fail "cshatag returned failure: $?"

TAG_TS=$(getfattr --only-values --name=user.shatag.ts "$TEST_FILE") \
	|| fail "Could not read test file timestamp attribute: $?"
TAG_VAL=$(getfattr --only-values --name=user.shatag.sha256 "$TEST_FILE") \
	|| fail "Could not read test file hash value attribute: $?"

[[ $MTIME = $TAG_TS ]] \
	|| fail "Shatag timestamp mismatch: $MTIME != $TAG_TS"
[[ $HASH = $TAG_VAL ]] \
	|| fail "Shatag value mismatch: $HASH != $TAG_VAL"

rm -f "$TEST_FILE"

exit $RET
