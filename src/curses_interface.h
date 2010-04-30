#ifndef CURSES_INTERFACE_H
#define CURSES_INTERFACE_H

#include <stdio.h>

FILE *_t3_putp_file;

int _t3_setupterm(void);
char *_t3_tigetstr(const char *name);
int _t3_tigetnum(const char *name);
int _t3_tigetflag(const char *name);
void _t3_putp(const char *string);
char *_t3_tparm(char *string, int nr_of_args, ...);

#endif
