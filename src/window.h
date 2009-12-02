#ifndef WINDOW_H
#define WINDOW_H

#include <stdlib.h>
#include "terminal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Window Window;

Window *win_new(int height, int width, int y, int x, int depth);
void win_del(Window *win);

Bool win_resize(Window *win, int height, int width);
void win_move(Window *win, int y, int x);
int win_get_width(Window *win);
int win_get_height(Window *win);
int win_get_x(Window *win);
int win_get_y(Window *win);
int win_get_depth(Window *win);
void win_set_cursor(Window *win, int y, int x);
void win_set_paint(Window *win, int y, int x);
void win_show(Window *win);
void win_hide(Window *win);

int win_addnstr(Window *win, const char *str, size_t n, CharData attr);
int win_addstr(Window *win, const char *str, CharData attr);
int win_addch(Window *win, char c, CharData attr);

int win_addnstrrep(Window *win, const char *str, size_t n, CharData attr, int rep);
int win_addstrrep(Window *win, const char *str, CharData attr, int rep);
int win_addchrep(Window *win, char c, CharData attr, int rep);

int win_box(Window *win, int y, int x, int height, int width, CharData attr);

void win_clrtoeol(Window *win);
void win_clrtobot(Window *win);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
