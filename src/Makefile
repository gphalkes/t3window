# Copyright (C) 2010 G.P. Halkes
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 3, as
# published by the Free Software Foundation.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

LCFILES.libt3window.la := window.c terminal.c curses_interface.c convert_output.c misc.c window_shared.c
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
