#!/bin/bash

DIR="`dirname \"$0\"`"
. "$DIR"/_common.sh


if [ $# -ne 1 ] ; then
	fail "Usage: runtest.sh <dir with test>"
fi

setup_TEST "$1"
cd_workdir

rm *

setup_ldlibrary_path
{
	echo '#line 1 "./test.c"'
	cat ../test.c
	echo '#line 1 "$TEST/test.c"'
	cat $TEST/test.c
} > test.c

gcc -g -Wall -I../../src test.c -L../../src/.libs/ -lt3window -o test || fail "!! Could not compile test"
../../../../record/src/tdreplay -lreplay.log $REPLAYOPTS $TEST/recording || fail "!! Terminal output is different"

[ "$QUIET" = 1 ] || echo "Test passed" >&2
exit 0
