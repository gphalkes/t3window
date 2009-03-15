#ifndef WINDOW_H
#define WINDOW_H

#include "terminal.h"

//FIXME: make sure that the base type is the correct size to store all the attributes
typedef int CharData;

typedef struct Window Window;

Window *win_new(int height, int width, int y, int x, int depth);
void win_del(Window *win);

Bool win_resize(Window *win, int height, int width);
void win_move(Window *win, int y, int x);
int win_get_width(Window *win);
int win_get_height(Window *win);
void win_set_cursor(Window *win, int y, int x);
void win_set_paint(Window *win, int y, int x);
void win_show(Window *win);
void win_hide(Window *win);


int win_mbaddnstra(Window *win, const char *str, size_t n, CharData attr);
int win_mbaddnstr(Window *win, const char *str, size_t n);
int win_mbaddstra(Window *win, const char *str, CharData attr);
int win_mbaddstr(Window *win, const char *str);

int win_addnstra(Window *win, const char *str, size_t n, CharData attr);
int win_addnstr(Window *win, const char *str, size_t n);
int win_addstra(Window *win, const char *str, CharData attr);
int win_addstr(Window *win, const char *str);

void win_clrtoeol(Window *win);
#endif
