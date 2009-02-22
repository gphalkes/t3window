#include <curses.h>
#include <term.h>
#include <stdarg.h>
#include <string.h>

#include "curses_interface.h"

int call_setupterm(void) {
	int error;
	if (setupterm(NULL, 1, &error) == ERR)
		return error + 2;
	return 0;
}

char *call_tigetstr(const char *name) {
	return tigetstr(name);
}

int call_tigetnum(const char *name) {
	return tigetnum(name);
}

void call_putp(const char *string) {
	putp(string);
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