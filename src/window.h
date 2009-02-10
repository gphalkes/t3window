#ifndef WINDOW_H
#define WINDOW_H

#include "terminal.h"

typedef struct Window Window;

Window *win_new(int height, int width, int y, int x, int depth);
void win_del(Window *win);

Bool win_resize(Window *win, int height, int width);
void win_move(Window *win, int y, int x);
int win_get_width(Window *win);
int win_get_height(Window *win);
void win_set_cursor(Window *win, int y, int x);
void win_set_attr(Window *win, int attr);
void win_add_str(Window *win, const char *str);
void win_show(Window *win);
void win_hide(Window *win);
#endif
