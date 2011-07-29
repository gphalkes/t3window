/* Copyright (C) 2011 G.P. Halkes
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 3, as
   published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef CURSES_INTERFACE_H
#define CURSES_INTERFACE_H

#include <stdio.h>
#include "window_api.h"

T3_WINDOW_LOCAL FILE *_t3_putp_file;

T3_WINDOW_LOCAL int _t3_setupterm(const char *term, int fd);
T3_WINDOW_LOCAL char *_t3_tigetstr(const char *name);
T3_WINDOW_LOCAL int _t3_tigetnum(const char *name);
T3_WINDOW_LOCAL int _t3_tigetflag(const char *name);
T3_WINDOW_LOCAL void _t3_putp(const char *string);
T3_WINDOW_LOCAL char *_t3_tparm(char *string, int nr_of_args, ...);

#endif
