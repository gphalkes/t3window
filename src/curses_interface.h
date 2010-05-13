#ifndef CURSES_INTERFACE_H
#define CURSES_INTERFACE_H

#include <stdio.h>
#include "window_api.h"

T3_WINDOW_LOCAL FILE *_t3_putp_file;

T3_WINDOW_LOCAL int _t3_setupterm(const char *term);
T3_WINDOW_LOCAL char *_t3_tigetstr(const char *name);
T3_WINDOW_LOCAL int _t3_tigetnum(const char *name);
T3_WINDOW_LOCAL int _t3_tigetflag(const char *name);
T3_WINDOW_LOCAL void _t3_putp(const char *string);
T3_WINDOW_LOCAL char *_t3_tparm(char *string, int nr_of_args, ...);

#endif
