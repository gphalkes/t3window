#ifndef WINDOW_H
#define WINDOW_H

#include <stdlib.h>
#include "terminal.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Define a parent anchor point for a relation (see WinAnchor). */
#define PARENT(_x) ((_x) << 3)
/** Define a child anchor point a relation (see WinAnchor). */
#define CHILD(_x) ((_x) << 6)
/** Get a parent anchor point from a relation (see WinAnchor). */
#define GETPARENT(_x) (((_x) >> 3) & 0x7)
/** Get a child anchor point from a relation (see WinAnchor). */
#define GETCHILD(_x) (((_x) >> 6) & 0x7)

/** Anchor points for defining relations between the positions of two windows.

    The anchor points can be used to define the relative positioning of two
    windows. For example, using PARENT(ANCHOR_TOPRIGHT) | CHILD(ANCHOR_TOPLEFT)
	allows positioning of one window left of another.
*/
enum WinAnchor {
	ANCHOR_ABSOLUTE,
	ANCHOR_TOPLEFT,
	ANCHOR_TOPRIGHT,
	ANCHOR_BOTTOMLEFT,
	ANCHOR_BOTTOMRIGHT
};


typedef struct Window Window;

Window *win_new(int height, int width, int y, int x, int depth);
Window *win_new_relative(int height, int width, int y, int x, int depth, Window *parent, int relation);
void win_del(Window *win);

void win_set_default_attrs(Window *win, CharData attr);

Bool win_resize(Window *win, int height, int width);
void win_move(Window *win, int y, int x);
int win_get_width(Window *win);
int win_get_height(Window *win);
int win_get_x(Window *win);
int win_get_y(Window *win);
int win_get_depth(Window *win);
int win_get_relation(Window *win, Window **parent);
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
