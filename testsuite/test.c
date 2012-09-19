/* Copyright (C) 2012 G.P. Halkes
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
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>
#include "window.h"

#define ASSERT(_cond) do { if (!(_cond)) fatal("Assertion failed on line %s:%d: %s\n", __FILE__, __LINE__, #_cond); } while (0)

static int opt_interactive = 1;
static int opt_debug;
static int initialized;

/** Alert the user of a fatal error and quit.
    @param fmt The format string for the message. See fprintf(3) for details.
    @param ... The arguments for printing.
*/
static void fatal(const char *fmt, ...) {
	va_list args;

	if (initialized)
		t3_term_restore();

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	exit(EXIT_FAILURE);
}

static void callback(const char *str, int length, int width, t3_attr_t attr) __attribute__((unused));
static void callback(const char *str, int length, int width, t3_attr_t attr) {
	(void) width;
	(void) attr;

	t3_term_set_attrs(T3_ATTR_BLINK | T3_ATTR_REVERSE);
	t3_term_putn(str, length);
}

static int next(void) {
	int result;

	t3_term_update();
	while (1) {
		if ((result = t3_term_get_keychar(opt_interactive ? -1 : 1)) == 27)
			while (!isalpha(result = t3_term_get_keychar(-1))) {}
		else if (result == T3_WARN_UPDATE_TERMINAL)
			t3_term_update();
		else
			return result;
	}
}

static int test(void);

int main(int argc, char *argv[]) {
	int c;

	while ((c = getopt(argc, argv, "dih")) >= 0) {
		switch (c) {
			case 'd':
				opt_debug = 1;
				break;
			case 'i':
				opt_interactive = 1;
				break;
			case 'h':
				printf("Usage: test [<options>]\n");
				printf("  -i          Interactive\n");
				printf("  -d          Debug\n");
				break;
			default:
				fatal("Error handling options\n");
		}
	}

	if (opt_debug) {
		printf("Waiting for enter to allow debug\n");
		getchar();
	}

	ASSERT(t3_term_init(-1, NULL) == T3_ERR_SUCCESS);
	atexit(t3_term_restore);
	initialized = 1;

	return test();
}
