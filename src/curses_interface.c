/** @file */

#include <curses.h>
#include <term.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include "curses_interface.h"

FILE *_t3_putp_file; /**< @c FILE struct corresponding to the terminal. Used for tputs in ::_t3_putp. */

int _t3_setupterm(const char *term) {
	int error;
	if (setupterm(term, 1, &error) != OK)
		return error + 2;
	return 0;
}

char *_t3_tigetstr(const char *name) {
	return tigetstr(name);
}

int _t3_tigetnum(const char *name) {
	return tigetnum(name);
}

int _t3_tigetflag(const char *name) {
	return tigetflag(name);
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
