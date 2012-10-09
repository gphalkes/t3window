#!/bin/bash

DIR="`dirname \"$0\"`"
. "$DIR"/_common.sh


if [ $# -eq 0 ] ; then
	fail "Usage: showtestdiff.sh <test> [<subtest>]"
fi

setup_TEST "$1"

dwdiff -Pc -C3 $TEST/recording $TEST/recording.new
exit 0
