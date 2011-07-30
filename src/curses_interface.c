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
/** @file */

#include <curses.h>
#include <term.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include "curses_interface.h"

FILE *_t3_putp_file; /**< @c FILE struct corresponding to the terminal. Used for tputs in ::_t3_putp. */

#define COPY_BUFFER_SIZE 160
#define COPY_BUFFER(_name) \
	char _name##_buffer[COPY_BUFFER_SIZE]; \
	strncpy(_name##_buffer, _name, COPY_BUFFER_SIZE); \
	_name##_buffer[COPY_BUFFER_SIZE - 1] = 0;

int _t3_setupterm(const char *term, int fd) {
	int error;

	/* Copy the term name into a new buffer, because setupterm expects a char *
	   not a const char *. */
	COPY_BUFFER(term);

	if (setupterm(term_buffer, fd, &error) != OK)
		return error + 2;
	return 0;
}

char *_t3_tigetstr(const char *name) {
	/* Copy the name into a new buffer, because tigetstr expects a char *
	   not a const char *. */
	COPY_BUFFER(name);
	return tigetstr(name_buffer);
}

int _t3_tigetnum(const char *name) {
	/* Copy the name into a new buffer, because tigetnum expects a char *
	   not a const char *. */
	COPY_BUFFER(name);
	return tigetnum(name_buffer);
}

int _t3_tigetflag(const char *name) {
	/* Copy the name into a new buffer, because tigetflag expects a char *
	   not a const char *. */
	COPY_BUFFER(name);
	return tigetflag(name_buffer);
}

static int writechar(int c) {
	return fputc(c, _t3_putp_file);
}

void _t3_putp(const char *string) {
	if (string == NULL)
		return;
	tputs(string, 1, writechar);
}

char *_t3_tparm(char *string, int nr_of_args, ...) {
	int args[9], i;
	va_list arglist;

	if (nr_of_args > 9 || nr_of_args < 0)
		return NULL;

	va_start(arglist, nr_of_args);
	memset(args, 0, sizeof(args));

	for (i = 0; i < nr_of_args; i++)
		args[i] = va_arg(arglist, int);

	return tparm(string, args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8]);
}
