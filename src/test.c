/* Copyright (C) 2010 G.P. Halkes
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
#include <stdlib.h>
#include <stdarg.h>
#include <locale.h>
#include "window.h"

#define ASSERT(_cond) do { if (!(_cond)) fatal("Assertion failed on line %s:%d: %s\n", __FILE__, __LINE__, #_cond); } while (0)

int inited;

/** Alert the user of a fatal error and quit.
    @param fmt The format string for the message. See fprintf(3) for details.
    @param ... The arguments for printing.
*/
void fatal(const char *fmt, ...) {
	va_list args;

	if (inited)
		t3_term_restore();

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	exit(EXIT_FAILURE);
}

void callback(const char *str, int length, int width, t3_attr_t attr) {
	int i;

	(void) width;
	(void) attr;

	t3_term_set_attrs(T3_ATTR_BLINK | T3_ATTR_REVERSE);
	for (i = 0; i < length; i++)
		t3_term_putc(str[i]);
}

int main(int argc, char *argv[]) {
	t3_window_t *low, *high;

	(void) argc;
	(void) argv;
	setlocale(LC_ALL, "");

	printf("Waiting for enter to allow debug\n");
	getchar();

	ASSERT(t3_term_init(-1, NULL) == T3_ERR_SUCCESS);
	atexit(t3_term_restore);
	inited = t3_true;

	ASSERT(low = t3_win_new(10, 10, 0, 5, 10));
	ASSERT(high = t3_win_new(10, 10, 5, 10, 0));
	t3_win_show(low);
	t3_term_update();
	getchar();

	t3_win_set_paint(low, 0, 0);
	t3_win_addstr(low, "0123456789-", 0);
	t3_win_set_paint(low, 6, 0);
	t3_win_addstr(low, "abＱc̃defghijk", 0);
	t3_term_update();
	getchar();

	t3_term_show_cursor();
	t3_win_set_cursor(low, 0, 0);
/* 	t3_win_show(high);
	t3_term_update();
	getchar();
 */
	t3_win_set_paint(high, 0, 0);
	t3_win_addstr(high, "ABCDEFGHIJK", 0);
/* 	t3_term_update();
	getchar();
 */
	t3_win_set_paint(high, 1, 0);
	t3_win_addstr(high, "9876543210+", T3_ATTR_REVERSE | T3_ATTR_FG_RED);
	t3_win_set_paint(high, 2, 0);
	t3_win_addstr(high, "wutvlkmjqx", T3_ATTR_ACS);

	t3_term_set_user_callback(callback);
	t3_win_set_paint(high, 3, 0);
	t3_win_addstr(high, "f", T3_ATTR_USER);
/* 	t3_term_update();
	getchar();

	t3_win_hide(high);
	t3_term_update();
	getchar();
 */
	t3_win_move(high, 5, 0);
	t3_win_resize(high, 10, 8);
	t3_win_show(high);
	t3_term_update();
	getchar();

	t3_win_hide(high);
	t3_term_update();
	getchar();

	t3_win_box(low, 0, 0, 10, 10, T3_ATTR_REVERSE);
	t3_term_update();
	getchar();

	t3_win_hide(low);
	t3_win_show(high);
	t3_term_update();
	getchar();

	return 0;
}

