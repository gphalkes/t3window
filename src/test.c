#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <locale.h>
#include "terminal.h"
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
		term_restore();

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
	Window *low, *high;

	(void) argc;
	(void) argv;
	setlocale(LC_ALL, "");

	printf("Waiting for enter to allow debug\n");
	getchar();

	ASSERT(term_init());
	atexit(term_restore);
	inited = true;

	ASSERT(low = win_new(10, 10, 0, 5, 10));
	ASSERT(high = win_new(10, 10, 5, 10, 0));
	win_show(low);
	term_refresh();
	getchar();

	win_set_paint(low, 0, 0);
	win_addstr(low, "0123456789-");
	win_set_paint(low, 6, 0);
	win_mbaddstr(low, "abＱc̃defghijk");
	term_refresh();
	getchar();

/* 	win_show(high);
	term_refresh();
	getchar();
 */
	win_set_paint(high, 0, 0);
	win_addstr(high, "ABCDEFGHIJK");
/* 	term_refresh();
	getchar();
 */
	win_set_paint(high, 1, 0);
	win_addstra(high, "9876543210+", ATTR_REVERSE | ATTR_FG_RED);
/* 	term_refresh();
	getchar();

	win_hide(high);
	term_refresh();
	getchar();
 */
	win_move(high, 5, 0);
	win_resize(high, 10, 8);
	win_show(high);
	term_refresh();
	getchar();

	win_hide(high);
	term_refresh();
	getchar();

	return 0;
}
