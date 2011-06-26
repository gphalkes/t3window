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

SOURCES.libt3window.la := window.c terminal.c curses_interface.c convert_output.c misc.c window_shared.c input.c
LDFLAGS.libt3window.la := -shared -Lt3unicode -Ltranscript
CFLAGS.misc := -DUSE_GETTEXT
LDLIBS.libt3window.la := -lncurses -lt3unicode -ltranscript

SOURCES.test := test.c
LDFLAGS.test := -L.libs
LDLIBS.test :=-lt3window

LD_LIBRARY_PATH := unicode
export LD_LIBRARY_PATH

LTTARGETS := libt3window.la
TARGETS := test
#================================================#
# NO RULES SHOULD BE DEFINED BEFORE THIS INCLUDE #
#================================================#
include ../../makesys/rules.mk
#================================================#
T3NAME := window
include ../../t3shared/rules.mk

CFLAGS += -DHAS_WINSIZE_IOCTL
CFLAGS += -DHAS_SELECT_H
CFLAGS += -DHAS_STRDUP

test: | libt3window.la

links:
	@[ -L t3window ] || ln -s . t3window
	@[ -L t3unicode ] || ln -s ../../unicode/src t3unicode
	@[ -L transcript ] || ln -s ../../../transcript/src transcript

.PHONY: links
