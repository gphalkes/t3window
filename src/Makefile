CFLAGS +=-I. -DT3_WINDOW_BUILD_DSO

LCFILES.libt3window.la := window.c terminal.c curses_interface.c convert_output.c misc.c
LDFLAGS.libt3window.la := -shared -Lunicode
CFLAGS.window := -D_XOPEN_SOURCE
CFLAGS.misc := -DUSE_GETTEXT
LDLIBS.libt3window.la := -lncurses -lt3unicode
CFLAGS.convert_output := -Iunicode

CFILES.test := test.c
LDFLAGS.test := -L.libs
LDLIBS.test :=-lt3window

LD_LIBRARY_PATH := unicode
export LD_LIBRARY_PATH

TARGETS := libt3window.la test

#================================================#
# NO RULES SHOULD BE DEFINED BEFORE THIS INCLUDE #
#================================================#
include ../../makesys/rules.mk
#================================================#
T3NAME := window
include ../../t3shared/rules.mk

test: | libt3window.la
