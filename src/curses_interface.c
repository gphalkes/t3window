#include <curses.h>
#include <term.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include "curses_interface.h"

FILE *_putp_file;

int call_setupterm(void) {
	int error;
	if (setupterm(NULL, 1, &error) != OK)
		return error + 2;
	return 0;
}

char *call_tigetstr(const char *name) {
	return tigetstr(name);
}

int call_tigetnum(const char *name) {
	return tigetnum(name);
}

int call_tigetflag(const char *name) {
	return tigetflag(name);
}

static int writechar(int c) {
	return fputc(c, _putp_file);
}

void call_putp(const char *string) {
	if (string == NULL)
		return;
	tputs(string, 1, writechar);
}

char *call_tparm(char *string, int nr_of_args, ...) {
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
