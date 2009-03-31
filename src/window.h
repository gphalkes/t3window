#ifndef WINDOW_H
#define WINDOW_H

#include <stdlib.h>
#include "terminal.h"

typedef struct Window Window;

Window *win_new(int height, int width, int y, int x, int depth);
void win_del(Window *win);

bool win_resize(Window *win, int height, int width);
void win_move(Window *win, int y, int x);
int win_get_width(Window *win);
int win_get_height(Window *win);
void win_set_cursor(Window *win, int y, int x);
void win_set_paint(Window *win, int y, int x);
void win_show(Window *win);
void win_hide(Window *win);

int win_mbaddnstr(Window *win, const char *str, size_t n, CharData attr);
int win_mbaddstr(Window *win, const char *str, CharData attr);

/* FIXME: make the now single byte variants autodetect, and make specific
   autodetect versions. */

int win_addnstr(Window *win, const char *str, size_t n, CharData attr);
int win_addstr(Window *win, const char *str, CharData attr);
int win_addch(Window *win, char c, CharData attr);

int win_addnstrrep(Window *win, const char *str, size_t n, CharData attr, int rep);
int win_addstrrep(Window *win, const char *str, CharData attr, int rep);
int win_addchrep(Window *win, char c, CharData attr, int rep);

void win_clrtoeol(Window *win);

#endif
