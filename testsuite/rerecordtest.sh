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
build_test

../../../../record/src/tdrerecord -s -o $TEST/recording.new $TEST/recording || fail "!! Could not rerecord test"

exit 0
