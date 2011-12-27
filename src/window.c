/* Copyright (C) 2011 G.P. Halkes
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
/** @file */

#include <stdlib.h>
#include <string.h>

#include "window.h"
#include "internal.h"


/* TODO list:
- _T3_ATTR_ACS should only be allowed on chars below 128 etc. Otherwise interpretation
  of width info may be weird.
- make t3_win_clrtobol
*/

t3_window_t *_t3_head, /**< @internal Head of depth sorted t3_window_t list. */
	*_t3_tail; /**< @internal Tail of depth sorted t3_window_t list. */

/** @addtogroup t3window_win */
/** @{ */

/** Insert a window into the list of known windows.
    @param win The t3_window_t to insert.
*/
static void insert_window(t3_window_t *win) {
	t3_window_t **head_ptr, **tail_ptr;
	t3_window_t *ptr;

	if (win->parent == NULL) {
		head_ptr = &_t3_head;
		tail_ptr = &_t3_tail;
	} else {
		head_ptr = &win->parent->head;
		tail_ptr = &win->parent->tail;
	}

	if (*head_ptr == NULL) {
		*tail_ptr = *head_ptr = win;
		win->next = win->prev = NULL;
		return;
	}

	/* Insert new window at the correct place in the sorted list of windows. */
	for (ptr = *head_ptr; ptr != NULL && ptr->depth < win->depth; ptr = ptr->next) {}

	if (ptr == NULL) {
		win->prev = *tail_ptr;
		win->next = NULL;
		(*tail_ptr)->next = win;
		*tail_ptr = win;
	} else if (ptr->prev == NULL) {
		win->prev = NULL;
		win->next = ptr;
		(*head_ptr)->prev = win;
		*head_ptr = win;
	} else {
		win->prev = ptr->prev;
		win->next = ptr;
		ptr->prev->next = win;
		ptr->prev = win;
	}
}

void _t3_remove_window(t3_window_t *win) {
	if (win->next == NULL) {
		if (win->parent == NULL)
			_t3_tail = win->prev;
		else
			win->parent->tail = win->prev;
	} else {
		win->next->prev = win->prev;
	}

	if (win->prev == NULL) {
		if (win->parent == NULL)
			_t3_head = win->next;
		else
			win->parent->head = win->next;
	} else {
		win->prev->next = win->next;
	}
}

static t3_bool has_loops(t3_window_t *win, t3_window_t *start) {
	return (win->parent == start ||
			win->anchor == start ||
			win->restrictw == start ||
			(win->parent != NULL && has_loops(win->parent, start)) ||
			(win->anchor != NULL && has_loops(win->anchor, start)) ||
			(win->restrictw != NULL && has_loops(win->restrictw, start)));
}

/** Create a new t3_window_t.
    @param parent t3_window_t used for clipping and relative positioning.
    @param height The desired height in terminal lines.
    @param width The desired width in terminal columns.
    @param y The vertical location of the window in terminal lines.
    @param x The horizontal location of the window in terminal columns.
    @param depth The depth of the window in the stack of windows.
    @return A pointer to a new t3_window_t struct or @c NULL if not enough
    	memory could be allocated.

    The @p depth parameter determines the z-order of the windows. Windows
    with lower depth will hide windows with higher depths. However, this only
    holds relative to the @p parent window. The position will be relative to
    the top-left corner of the @p parent window, or to the top-left corner of
    the terminal if @p parent is @c NULL.
*/
t3_window_t *t3_win_new(t3_window_t *parent, int height, int width, int y, int x, int depth) {
	t3_window_t *retval;
	int i;

	if ((retval = t3_win_new_unbacked(parent, height, width, y, x, depth)) == NULL)
		return NULL;

	if ((retval->lines = calloc(1, sizeof(line_data_t) * height)) == NULL) {
		t3_win_del(retval);
		return NULL;
	}

	for (i = 0; i < height; i++) {
		retval->lines[i].allocated = width > INITIAL_ALLOC ? INITIAL_ALLOC : width;
		if ((retval->lines[i].data = malloc(sizeof(t3_chardata_t) * retval->lines[i].allocated)) == NULL) {
			t3_win_del(retval);
			return NULL;
		}
	}

	return retval;
}

/** Create a new t3_window_t with relative position without backing store.
    @param parent t3_window_t used for clipping.
    @param height The desired height in terminal lines.
    @param width The desired width in terminal columns.
    @param y The vertical location of the window in terminal lines.
    @param x The horizontal location of the window in terminal columns.
    @param depth The depth of the window in the stack of windows.
    @return A pointer to a new t3_window_t struct or @c NULL if not enough
    	memory could be allocated or an invalid parameter was passed.

    Windows without a backing store can not be used for drawing. These are
    only defined to allow a window for positioning other windows only.

    The @p depth parameter determines the z-order of the windows. Windows
    with lower depth will hide windows with higher depths.
*/
t3_window_t *t3_win_new_unbacked(t3_window_t *parent, int height, int width, int y, int x, int depth) {
	t3_window_t *retval;

	if (height <= 0 || width <= 0)
		return NULL;

	if ((retval = calloc(1, sizeof(t3_window_t))) == NULL)
		return NULL;

	retval->x = x;
	retval->y = y;
	retval->width = width;
	retval->height = height;
	retval->parent = parent;
	retval->anchor = NULL;
	retval->restrictw = NULL;
	retval->depth = depth;
	retval->cached_pos_line = -1;

	insert_window(retval);
	return retval;
}

/** Change a t3_window_t's parent.
    @param win The t3_window_t to set the parent for.
    @param parent The t3_window_t to link to.
    @return A boolean indicating whether the setting was successful.

    This function will fail if setting the parent will cause a loop in the
    window tree.
*/
t3_bool t3_win_set_parent(t3_window_t *win, t3_window_t *parent) {
	t3_window_t *old_parent;

	if (parent == win->parent)
		return t3_true;

	old_parent = win->parent;
	win->parent = parent;
	if (has_loops(win, win)) {
		win->parent = old_parent;
		return t3_false;
	}

	/* Reset parent, to allow _t3_remove_window to work. */
	win->parent = old_parent;
	_t3_remove_window(win);
	win->parent = parent;
	insert_window(win);
	return t3_true;
}

/** Link a t3_window_t's position to the position of another t3_window_t.
    @param win The t3_window_t to set the anchor for.
    @param anchor The t3_window_t to link to.
    @param relation The relation between this window and @p anchor (see ::t3_win_anchor_t).
    @return A boolean indicating whether the setting was successful.

    This function will fail if either the @p relation is not valid, or setting
    the anchor will cause a loop in the window tree.
*/
t3_bool t3_win_set_anchor(t3_window_t *win, t3_window_t *anchor, int relation) {
	t3_window_t *old_anchor;

	if (T3_GETPARENT(relation) < T3_ANCHOR_TOPLEFT || T3_GETPARENT(relation) > T3_ANCHOR_CENTERRIGHT)
		return t3_false;

	if (T3_GETCHILD(relation) < T3_ANCHOR_TOPLEFT || T3_GETCHILD(relation) > T3_ANCHOR_CENTERRIGHT)
		return t3_false;

	if (anchor == NULL && (T3_GETPARENT(relation) != T3_ANCHOR_TOPLEFT || T3_GETCHILD(relation) != T3_ANCHOR_TOPLEFT))
		return t3_false;

	if (anchor == win->anchor) {
		win->relation = relation;
		return t3_true;
	}

	old_anchor = win->anchor;
	win->anchor = anchor;
	if (has_loops(win, win)) {
		win->anchor = old_anchor;
		return t3_false;
	}

	win->relation = relation;
	return t3_true;
}

/** Change the depth of a t3_window_t.
    @param win The t3_window_t to set the depth for.
    @param depth The new depth for the window.
*/
void t3_win_set_depth(t3_window_t *win, int depth) {
	_t3_remove_window(win);
	win->depth = depth;
	insert_window(win);
}

/** Check whether a window is show, both by the direct setting of the shown flag,
		as well as the parents.
*/
t3_bool _t3_win_is_shown(t3_window_t *win) {
	do {
		if (!win->shown)
			return t3_false;
		win = win->parent;
	} while (win != NULL);
	return t3_true;
}

/** Set default attributes for a window.
    @param win The t3_window_t to set the default attributes for.
    @param attr The attributes to set.

    This function can be used to set a default background for the entire window, as
    well as any other attributes.
*/
void t3_win_set_default_attrs(t3_window_t *win, t3_attr_t attr) {
	if (win == NULL)
		win = _t3_terminal_window;
	win->default_attrs = attr;
}

/** Set the restrictw window.
    @param win The t3_window_t to set the restrictw parameter for.
    @param restrictw The t3_window_t to restrictw @p win to.

    To restrictw the window to the terminal, pass @c NULL in @p restrictw. To
    cancel restriction of the window position, pass @p win in @p restrictw.
*/
t3_bool t3_win_set_restrict(t3_window_t *win, t3_window_t *restrictw) {
	t3_window_t *old_restict;

	if (restrictw == win) {
		win->restrictw = NULL;
		return t3_true;
	}

	old_restict = win->restrictw;
	if (restrictw == NULL) {
		win->restrictw = _t3_terminal_window;
		/* Setting the restriction to the terminal window can not cause a loop. */
		return t3_true;
	} else {
		if (restrictw == win->restrictw)
			return t3_true;
		win->restrictw = restrictw;
	}

	if (has_loops(win, win)) {
		win->restrictw = old_restict;
		return t3_false;
	}
	return t3_true;
}

/** Discard a t3_window_t.
    @param win The t3_window_t to discard.

    Note that child windows are @em not automatically discarded as well. All
    child windows have their parent attribute set to @c NULL.
*/
void t3_win_del(t3_window_t *win) {
	int i;
	if (win == NULL)
		return;

	_t3_remove_window(win);
	/* Make child windows stand alone windows. */
	while (win->head != NULL)
		t3_win_set_parent(win->head, NULL);

	if (win->lines != NULL) {
		for (i = 0; i < win->height; i++)
			free(win->lines[i].data);
		free(win->lines);
	}
	free(win);
}

/** Change a t3_window_t's size.
    @param win The t3_window_t to change the size of.
    @param height The desired new height of the t3_window_t in terminal lines.
    @param width The desired new width of the t3_window_t in terminal columns.
    @return A boolean indicating succes, depending on the validity of the
        parameters and whether reallocation of the internal data
        structures succeeds.
*/
t3_bool t3_win_resize(t3_window_t *win, int height, int width) {
	int i;

	if (height <= 0 || width <= 0)
		return t3_false;

	if (win->lines == NULL) {
		win->height = height;
		win->width = width;
		return t3_true;
	}

	if (height > win->height) {
		void *result;
		if ((result = realloc(win->lines, height * sizeof(line_data_t))) == NULL)
			return t3_false;
		win->lines = result;
		memset(win->lines + win->height, 0, sizeof(line_data_t) * (height - win->height));
		for (i = win->height; i < height; i++) {
			if ((win->lines[i].data = malloc(sizeof(t3_chardata_t) * INITIAL_ALLOC)) == NULL) {
				for (i = win->height; i < height && win->lines[i].data != NULL; i++)
					free(win->lines[i].data);
				return t3_false;
			}
			win->lines[i].allocated = INITIAL_ALLOC;
		}
	} else if (height < win->height) {
		for (i = height; i < win->height; i++)
			free(win->lines[i].data);
		memset(win->lines + height, 0, sizeof(line_data_t) * (win->height - height));
	}

	if (width < win->width) {
		/* Chop lines to maximum width */
		int paint_x = win->paint_x, paint_y = win->paint_y;

		win->cached_pos_line = -1;

		for (i = 0; i < height; i++) {
			t3_win_set_paint(win, i, width);
			t3_win_clrtoeol(win);
		}

		win->paint_x = paint_x;
		win->paint_y = paint_y;
	}

	win->height = height;
	win->width = width;
	return t3_true;
}

/** Change a t3_window_t's position.
    @param win The t3_window_t to change the position of.
    @param y The desired new vertical position of the t3_window_t in terminal lines.
    @param x The desired new horizontal position of the t3_window_t in terminal lines.

    This function will always succeed as it only updates the internal book keeping.
*/
void t3_win_move(t3_window_t *win, int y, int x) {
	win->y = y;
	win->x = x;
}

/** Get a t3_window_t's width. */
int t3_win_get_width(t3_window_t *win) {
	return win->width;
}

/** Get a t3_window_t's height. */
int t3_win_get_height(t3_window_t *win) {
	return win->height;
}

/** Get a t3_window_t's horizontal position.

    The retrieved position may be relative to another window. Use ::t3_win_get_abs_x
    to find the absolute position.
*/
int t3_win_get_x(t3_window_t *win) {
	return win->x;
}

/** Get a t3_window_t's vertical position.

    The retrieved position may be relative to another window. Use ::t3_win_get_abs_y
    to find the absolute position.
*/

int t3_win_get_y(t3_window_t *win) {
	return win->y;
}

/** Get a t3_window_t's depth. */
int t3_win_get_depth(t3_window_t *win) {
	return win->depth;
}

/** Get a t3_window_t's relative positioning information.
    @param win The t3_window_t to get the relative positioning information for.
    @param [out] anchor The location to store the pointer to the t3_window_t relative to
    	which the position is specified.
    @return The relative positioning method.

    To retrieve the separate parts of the relative positioning information, use
    ::T3_GETPARENT and ::T3_GETCHILD.
*/
int t3_win_get_relation(t3_window_t *win, t3_window_t **anchor) {
	if (anchor != NULL)
		*anchor = win->anchor;
	return win->relation;
}

/** Get a t3_window_t's parent window. */
t3_window_t *t3_win_get_parent(t3_window_t *win) {
	return win->parent;
}

/** Get a t3_window_t's absolute horizontal position. */
int t3_win_get_abs_x(t3_window_t *win) {
	int result;

	if (win == NULL)
		return 0;

	switch (T3_GETPARENT(win->relation)) {
		case T3_ANCHOR_TOPLEFT:
		case T3_ANCHOR_BOTTOMLEFT:
		case T3_ANCHOR_CENTERLEFT:
			result = t3_win_get_abs_x(win->anchor == NULL ? win->parent : win->anchor) + win->x;
			break;
		case T3_ANCHOR_TOPRIGHT:
		case T3_ANCHOR_BOTTOMRIGHT:
		case T3_ANCHOR_CENTERRIGHT:
			/* If anchor == NULL, the relation is always T3_ANCHOR_TOPLEFT. */
			result = t3_win_get_abs_x(win->anchor) + win->anchor->width + win->x;
			break;
		case T3_ANCHOR_TOPCENTER:
		case T3_ANCHOR_BOTTOMCENTER:
		case T3_ANCHOR_CENTER:
			/* If anchor == NULL, the relation is always T3_ANCHOR_TOPLEFT. */
			result = t3_win_get_abs_x(win->anchor) + win->anchor->width / 2 + win->x;
			break;
		default:
			result = win->x;
			break;
	}

	switch (T3_GETCHILD(win->relation)) {
		case T3_ANCHOR_TOPRIGHT:
		case T3_ANCHOR_BOTTOMRIGHT:
		case T3_ANCHOR_CENTERRIGHT:
			result -= win->width;
			break;
		case T3_ANCHOR_TOPCENTER:
		case T3_ANCHOR_BOTTOMCENTER:
		case T3_ANCHOR_CENTER:
			result -= (win->width / 2);
			break;
		default:;
	}

	if (win->restrictw != NULL) {
		int left, right;
		left = t3_win_get_abs_x(win->restrictw);
		right = left + win->restrictw->width;

		if (result + win->width > right)
			result = right - win->width;
		if (result < left)
			result = 0;
	}
	return result;
}

/** Get a t3_window_t's absolute vertical position. */
int t3_win_get_abs_y(t3_window_t *win) {
	int result;

	if (win == NULL)
		return 0;

	switch (T3_GETPARENT(win->relation)) {
		case T3_ANCHOR_TOPLEFT:
		case T3_ANCHOR_TOPRIGHT:
		case T3_ANCHOR_TOPCENTER:
			result = t3_win_get_abs_y(win->anchor == NULL ? win->parent : win->anchor) + win->y;
			break;
		case T3_ANCHOR_BOTTOMLEFT:
		case T3_ANCHOR_BOTTOMRIGHT:
		case T3_ANCHOR_BOTTOMCENTER:
			/* If anchor == NULL, the relation is always T3_ANCHOR_TOPLEFT. */
			result = t3_win_get_abs_y(win->anchor) + win->anchor->height + win->y;
			break;
		case T3_ANCHOR_CENTERLEFT:
		case T3_ANCHOR_CENTERRIGHT:
		case T3_ANCHOR_CENTER:
			result = t3_win_get_abs_y(win->anchor) + win->anchor->height / 2 + win->y;
			break;
		default:
			result = win->y;
			break;
	}

	switch (T3_GETCHILD(win->relation)) {
		case T3_ANCHOR_BOTTOMLEFT:
		case T3_ANCHOR_BOTTOMRIGHT:
		case T3_ANCHOR_BOTTOMCENTER:
			result -= win->height;
			break;
		case T3_ANCHOR_CENTERLEFT:
		case T3_ANCHOR_CENTERRIGHT:
		case T3_ANCHOR_CENTER:
			result -= win->height / 2;
			break;
		default:;
	}

	if (win->restrictw != NULL) {
		int top, bottom;
		top = t3_win_get_abs_y(win->restrictw);
		bottom = top + win->restrictw->height;

		if (result + win->height > bottom)
			result = bottom - win->height;
		if (result < top)
			result = 0;
	}
	return result;
}

/** Position the cursor relative to a t3_window_t.
    @param win The t3_window_t to position the cursor in.
    @param y The line relative to @p win to position the cursor at.
    @param x The column relative to @p win to position the cursor at.

    The cursor is only moved if the window is currently shown.
*/
void t3_win_set_cursor(t3_window_t *win, int y, int x) {
	if (_t3_win_is_shown(win))
		t3_term_set_cursor(t3_win_get_abs_y(win) + y, t3_win_get_abs_x(win) + x);
}

/** Change the position where characters are written to the t3_window_t. */
void t3_win_set_paint(t3_window_t *win, int y, int x) {
	win->paint_x = x < 0 ? 0 : x;
	win->paint_y = y < 0 ? 0 : y;
}

/** Make a t3_window_t visible. */
void t3_win_show(t3_window_t *win) {
	win->shown = t3_true;
}

/** Make a t3_window_t invisible. */
void t3_win_hide(t3_window_t *win) {
	win->shown = t3_false;
}

/** @} */
