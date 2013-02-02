# Copyright (C) 2011 G.P. Halkes
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 3, as
# published by the Free Software Foundation.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

SOURCES.libt3window.la := window.c terminal.c curses_interface.c convert_output.c misc.c window_shared.c \
	input.c terminal_init.c window_paint.c utf8.c generated/available_since.c log.c
LDFLAGS.libt3window.la := -shared
CFLAGS.misc := -DUSE_GETTEXT
LDLIBS.libt3window.la += -lncurses -lunistring
LDLIBS.libt3window.la += -L$(CURDIR)/../include/transcript/.libs -ltranscript

SOURCES.test := test.c
LDFLAGS.test := -L.libs
LDLIBS.test :=-lt3window

LTTARGETS := libt3window.la
TARGETS := test
#================================================#
# NO RULES SHOULD BE DEFINED BEFORE THIS INCLUDE #
#================================================#
include ../../makesys/rules.mk
#================================================#
T3NAME := window
include ../../t3shared/rules.mk

CFLAGS += -D_T3_WINDOW_INTERNAL
#~ CFLAGS += -D_T3_WINDOW_DEBUG
CFLAGS += -DHAS_WINSIZE_IOCTL
CFLAGS += -DHAS_SELECT_H
CFLAGS += -DHAS_STRDUP
CFLAGS += -DHAS_TIOCLINUX -DHAS_KDGKBENT

test: | libt3window.la

.PHONY: links
