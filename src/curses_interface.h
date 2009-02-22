#ifndef CURSES_INTERFACE_H
#define CURSES_INTERFACE_H

int call_setupterm(void);
char *call_tigetstr(const char *name);
int call_tigetnum(const char *name);
void call_putp(const char *string);
char *call_tparm(char *string, int nr_of_args, ...);

#endif
