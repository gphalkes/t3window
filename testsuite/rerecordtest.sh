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

build_test

tdrerecord -o $TEST/recording.new $TEST/recording || fail "!! Could not rerecord test"

if ! tdcompare -v $TEST/recording{,.new} ; then
	echo -e "\\033[31;1mTest $TEST has different visual result\\033[0m"
	exit 1
fi

exit 0
