#!/bin/bash

DIR="`dirname \"$0\"`"
. "$DIR"/_common.sh


if [ $# -eq 0 ] ; then
	fail "Usage: accepttest.sh <test> [<subtest>]"
fi

setup_TEST "$1"

mv $TEST/recording.new $TEST/recording
exit 0
