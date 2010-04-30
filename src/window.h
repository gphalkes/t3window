#ifndef T3_WINDOW_H
#define T3_WINDOW_H

/** @defgroup t3window_win Window manipulation functions. */

#include <stdlib.h>
#include "terminal.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Define a parent anchor point for a relation (see WinAnchor). */
#define T3_PARENT(_x) ((_x) << 3)
/** Define a child anchor point a relation (see WinAnchor). */
#define T3_CHILD(_x) ((_x) << 6)
/** Get a parent anchor point from a relation (see WinAnchor). */
#define T3_GETPARENT(_x) (((_x) >> 3) & 0x7)
/** Get a child anchor point from a relation (see WinAnchor). */
#define T3_GETCHILD(_x) (((_x) >> 6) & 0x7)

/** Anchor points for defining relations between the positions of two windows.

    The anchor points can be used to define the relative positioning of two
    windows. For example, using PARENT(ANCHOR_TOPRIGHT) | CHILD(ANCHOR_TOPLEFT)
	allows positioning of one window left of another.
*/
enum WinAnchor {
	T3_ANCHOR_ABSOLUTE,
	T3_ANCHOR_TOPLEFT,
	T3_ANCHOR_TOPRIGHT,
	T3_ANCHOR_BOTTOMLEFT,
	T3_ANCHOR_BOTTOMRIGHT
};

/** An opaque struct representing a window which can be shown on the terminal.
    @ingroup t3window_other
*/
typedef struct T3Window T3Window;

T3Window *t3_win_new(int height, int width, int y, int x, int depth);
T3Window *t3_win_new_relative(int height, int width, int y, int x, int depth, T3Window *parent, int relation);
void t3_win_del(T3Window *win);

void t3_win_set_default_attrs(T3Window *win, T3CharData attr);

T3Bool t3_win_resize(T3Window *win, int height, int width);
void t3_win_move(T3Window *win, int y, int x);
int t3_win_get_width(T3Window *win);
int t3_win_get_height(T3Window *win);
int t3_win_get_x(T3Window *win);
int t3_win_get_y(T3Window *win);
int t3_win_get_depth(T3Window *win);
int t3_win_get_relation(T3Window *win, T3Window **parent);
void t3_win_set_cursor(T3Window *win, int y, int x);
void t3_win_set_paint(T3Window *win, int y, int x);
void t3_win_show(T3Window *win);
void t3_win_hide(T3Window *win);

int t3_win_addnstr(T3Window *win, const char *str, size_t n, T3CharData attr);
int t3_win_addstr(T3Window *win, const char *str, T3CharData attr);
int t3_win_addch(T3Window *win, char c, T3CharData attr);

int t3_win_addnstrrep(T3Window *win, const char *str, size_t n, T3CharData attr, int rep);
int t3_win_addstrrep(T3Window *win, const char *str, T3CharData attr, int rep);
int t3_win_addchrep(T3Window *win, char c, T3CharData attr, int rep);

int t3_win_box(T3Window *win, int y, int x, int height, int width, T3CharData attr);

void t3_win_clrtoeol(T3Window *win);
void t3_win_clrtobot(T3Window *win);

#ifdef USING_NAMESPACE_T3
#include "window_namespace.h"
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
