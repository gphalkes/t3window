#ifndef WINDOW_H
#define WINDOW_H

typedef struct Window Window;
typedef void (*)(Window *, int y, int x, int width, int height) WindowRepaintHandler;

Window *win_new(int height, int width, int y, int x, int depth, WindowRepaintHandler handler);
void win_del(Window *win);

void win_resize(Window *win, int height, int width);
void win_move(Window *win, int y, int x);
int win_get_width(Window *win);
void win_get_height(Window *win);
void win_set_cursor(Window *win, int y, int x);
void win_set_attr(Window *win, int attr);
void win_add_str(Window *win, const char *str);
void win_show(Window *win);
void win_hide(Window *win);
#endif
