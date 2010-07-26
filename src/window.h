/* Copyright (C) 2010 G.P. Halkes
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 3, as
   published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
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
    windows. For example, using T3_PARENT(T3_ANCHOR_TOPRIGHT) | T3_CHILD(T3_ANCHOR_TOPLEFT)
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
typedef struct t3_window_t t3_window_t;

T3_WINDOW_API t3_window_t *t3_win_new(int height, int width, int y, int x, int depth);
T3_WINDOW_API t3_window_t *t3_win_new_relative(int height, int width, int y, int x, int depth, t3_window_t *parent, int relation);
T3_WINDOW_API void t3_win_del(t3_window_t *win);

T3_WINDOW_API void t3_win_set_default_attrs(t3_window_t *win, t3_attr_t attr);

T3_WINDOW_API t3_bool t3_win_resize(t3_window_t *win, int height, int width);
T3_WINDOW_API void t3_win_move(t3_window_t *win, int y, int x);
T3_WINDOW_API int t3_win_get_width(t3_window_t *win);
T3_WINDOW_API int t3_win_get_height(t3_window_t *win);
T3_WINDOW_API int t3_win_get_x(t3_window_t *win);
T3_WINDOW_API int t3_win_get_y(t3_window_t *win);
T3_WINDOW_API int t3_win_get_depth(t3_window_t *win);
T3_WINDOW_API int t3_win_get_relation(t3_window_t *win, t3_window_t **parent);
T3_WINDOW_API void t3_win_set_cursor(t3_window_t *win, int y, int x);
T3_WINDOW_API void t3_win_set_paint(t3_window_t *win, int y, int x);
T3_WINDOW_API void t3_win_show(t3_window_t *win);
T3_WINDOW_API void t3_win_hide(t3_window_t *win);

T3_WINDOW_API int t3_win_addnstr(t3_window_t *win, const char *str, size_t n, t3_attr_t attr);
T3_WINDOW_API int t3_win_addstr(t3_window_t *win, const char *str, t3_attr_t attr);
T3_WINDOW_API int t3_win_addch(t3_window_t *win, char c, t3_attr_t attr);

T3_WINDOW_API int t3_win_addnstrrep(t3_window_t *win, const char *str, size_t n, t3_attr_t attr, int rep);
T3_WINDOW_API int t3_win_addstrrep(t3_window_t *win, const char *str, t3_attr_t attr, int rep);
T3_WINDOW_API int t3_win_addchrep(t3_window_t *win, char c, t3_attr_t attr, int rep);

T3_WINDOW_API int t3_win_box(t3_window_t *win, int y, int x, int height, int width, t3_attr_t attr);

T3_WINDOW_API void t3_win_clrtoeol(t3_window_t *win);
T3_WINDOW_API void t3_win_clrtobot(t3_window_t *win);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
