#!/bin/bash

DIR="`dirname \"$0\"`"
. "$DIR"/_common.sh


if [ $# -eq 0 ] ; then
	fail "Usage: recordtest.sh <test>"
fi

TEST="$PWD/$1"

shift
cd_workdir

rm *

setup_ldlibrary_path
{
	echo '#line 1 "./test.c"'
	cat ../test.c
	echo '#line 1 "$TEST/test.c"'
	cat $TEST/test.c
} > test.c
gcc -g -Wall -I../../src test.c -L../../src/.libs/ -lt3window -o test
../../../../record/src/tdrecord -o $TEST/recording -e LD_LIBRARY_PATH $RECORDOPTS ./test || fail "!! Could not record test"

exit 0
