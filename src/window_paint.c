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
#include <t3unicode/unicode.h>

#include "window.h"
#include "internal.h"

/** @internal
    @brief The maximum size of a UTF-8 character in bytes. Used in ::t3_win_addnstr.
*/
#define UTF8_MAX_BYTES 4

/** @addtogroup t3window_win */
/** @{ */

/** Ensure that a line_data_t struct has at least a specified number of
        bytes of unused space.
    @param line The line_data_t struct to check.
    @param n The required unused space in bytes.
    @return A boolean indicating whether, after possibly reallocating, the
    	requested number of bytes is available.
*/
static t3_bool ensure_space(line_data_t *line, size_t n) {
	int newsize;
	t3_chardata_t *resized;

	if (n > INT_MAX || INT_MAX - (int) n < line->length)
		return t3_false;

	if (line->allocated > line->length + (int) n)
		return t3_true;

	newsize = line->allocated;

	do {
		/* Sanity check for overflow of allocated variable. Prevents infinite loops. */
		if (newsize > INT_MAX / 2)
			newsize = INT_MAX;
		else
			newsize *= 2;
	} while (newsize - line->length < (int) n);

	if ((resized = realloc(line->data, sizeof(t3_chardata_t) * newsize)) == NULL)
		return t3_false;
	line->data = resized;
	line->allocated = newsize;
	return t3_true;
}

/** @internal
    @brief Add character data to a t3_window_t at the current painting position.
    @param win The t3_window_t to add the characters to.
    @param str The characters to add as t3_chardata_t.
    @param n The length of @p str.
    @return A boolean indicating whether insertion succeeded (only fails on memory
        allocation errors).

    This is the most complex and most central function of the library. All character
    drawing eventually ends up here.
*/
static t3_bool _win_add_chardata(t3_window_t *win, t3_chardata_t *str, size_t n) {
	int width = 0;
	int extra_spaces = 0;
	int i, j;
	size_t k;
	t3_bool result = t3_true;
	t3_chardata_t space = ' ' | _t3_term_attr_to_chardata(win->default_attrs);

	if (win->lines == NULL)
		return t3_false;

	if (win->paint_y >= win->height)
		return t3_true;
	if (win->paint_x >= win->width)
		return t3_true;

	for (k = 0; k < n; k++) {
		if (win->paint_x + width + _T3_CHARDATA_TO_WIDTH(str[k]) > win->width)
			break;
		width += _T3_CHARDATA_TO_WIDTH(str[k]);
	}

	if (k < n)
		extra_spaces = win->width - win->paint_x - width;
	n = k;

	if (win->cached_pos_line != win->paint_y || win->cached_pos_width >= win->paint_x) {
		win->cached_pos_line = win->paint_y;
		win->cached_pos = 0;
		win->cached_pos_width = win->lines[win->paint_y].start;
	}

	if (width == 0) {
		int pos_width;
		/* Combining characters. */

		/* Simply drop characters that don't belong to any other character. */
		if (win->lines[win->paint_y].length == 0 ||
				win->paint_x <= win->lines[win->paint_y].start ||
				win->paint_x > win->lines[win->paint_y].start + win->lines[win->paint_y].width + 1)
			return t3_true;

		if (!ensure_space(win->lines + win->paint_y, n))
			return t3_false;

		pos_width = win->cached_pos_width;

		/* Locate the first character that at least partially overlaps the position
		   where this string is supposed to go. */
		for (i = win->cached_pos; i < win->lines[win->paint_y].length; i++) {
			pos_width += _T3_CHARDATA_TO_WIDTH(win->lines[win->paint_y].data[i]);
			if (pos_width >= win->paint_x)
				break;
		}

		/* Check whether we are being asked to add a zero-width character in the middle
		   of a double-width character. If so, ignore. */
		if (pos_width > win->paint_x)
			return t3_true;

		/* Skip to the next non-zero-width character. */
		if (i < win->lines[win->paint_y].length)
			for (i++; i < win->lines[win->paint_y].length && _T3_CHARDATA_TO_WIDTH(win->lines[win->paint_y].data[i]) == 0; i++) {}

		memmove(win->lines[win->paint_y].data + i + n, win->lines[win->paint_y].data + i,
			sizeof(t3_chardata_t) * (win->lines[win->paint_y].length - i));
		memcpy(win->lines[win->paint_y].data + i, str, n * sizeof(t3_chardata_t));
		win->lines[win->paint_y].length += n;
	} else if (win->lines[win->paint_y].length == 0) {
		/* Empty line. */
		if (!ensure_space(win->lines + win->paint_y, n))
			return t3_false;
		win->lines[win->paint_y].start = win->paint_x;
		memcpy(win->lines[win->paint_y].data, str, n * sizeof(t3_chardata_t));
		win->lines[win->paint_y].length += n;
		win->lines[win->paint_y].width = width;
		win->cached_pos_line = -1;
	} else if (win->lines[win->paint_y].start + win->lines[win->paint_y].width <= win->paint_x) {
		/* Add characters after existing characters. */
		int diff = win->paint_x - (win->lines[win->paint_y].start + win->lines[win->paint_y].width);

		if (!ensure_space(win->lines + win->paint_y, n + diff))
			return t3_false;
		for (i = diff; i > 0; i--) {
			win->lines[win->paint_y].data[win->lines[win->paint_y].length++] = WIDTH_TO_META(1) | ' ' |
				_t3_term_attr_to_chardata(win->default_attrs);
		}
		memcpy(win->lines[win->paint_y].data + win->lines[win->paint_y].length, str, n * sizeof(t3_chardata_t));
		win->lines[win->paint_y].length += n;
		win->lines[win->paint_y].width += width + diff;
	} else if (win->paint_x + width <= win->lines[win->paint_y].start) {
		/* Add characters before existing characters. */
		int diff = win->lines[win->paint_y].start - (win->paint_x + width);

		if (!ensure_space(win->lines + win->paint_y, n + diff))
			return t3_false;
		memmove(win->lines[win->paint_y].data + n + diff, win->lines[win->paint_y].data,
			sizeof(t3_chardata_t) * win->lines[win->paint_y].length);
		memcpy(win->lines[win->paint_y].data, str, n * sizeof(t3_chardata_t));
		for (i = diff; i > 0; i--)
			win->lines[win->paint_y].data[n++] = WIDTH_TO_META(1) | ' ' | _t3_term_attr_to_chardata(win->default_attrs);
		win->lines[win->paint_y].length += n;
		win->lines[win->paint_y].width += width + diff;
		win->lines[win->paint_y].start = win->paint_x;
		win->cached_pos_line = -1;
	} else {
		/* Character (partly) overwrite existing chars. */
		int pos_width = win->cached_pos_width;
		int next_pos_width;
		size_t start_replace = 0, start_space_meta, start_spaces, end_replace, end_space_meta, end_spaces;
		int sdiff;

		/* Locate the first character that at least partially overlaps the position
		   where this string is supposed to go. */
		for (i = win->cached_pos; i < win->lines[win->paint_y].length &&
				(next_pos_width = pos_width + _T3_CHARDATA_TO_WIDTH(win->lines[win->paint_y].data[i])) <= win->paint_x; i++)
			pos_width = next_pos_width;

		win->cached_pos = i;
		win->cached_pos_width = pos_width;

		start_replace = i;

		/* If the character only partially overlaps, we replace the first part with
		   spaces with the attributes of the old character. */
		start_space_meta = (win->lines[win->paint_y].data[start_replace] & _T3_ATTR_MASK) | WIDTH_TO_META(1);
		start_spaces = win->paint_x >= win->lines[win->paint_y].start ? win->paint_x - pos_width : 0;

		/* Now we need to find which other character(s) overlap. However, the current
		   string may overlap with a double width character but only for a single
		   position. In that case we will replace the trailing portion of the character
		   with spaces with the old character's attributes. */
		pos_width += _T3_CHARDATA_TO_WIDTH(win->lines[win->paint_y].data[start_replace]);

		i++;

		/* If the character where we start overwriting already fully overlaps with the
		   new string, then we need to only replace this and any spaces that result
		   from replacing the trailing portion need to use the start space attribute */
		if (pos_width >= win->paint_x + width) {
			end_space_meta = start_space_meta;
		} else {
			for (; i < win->lines[win->paint_y].length && pos_width < win->paint_x + width; i++)
				pos_width += _T3_CHARDATA_TO_WIDTH(win->lines[win->paint_y].data[i]);

			end_space_meta = (win->lines[win->paint_y].data[i - 1] & _T3_ATTR_MASK) | WIDTH_TO_META(1);
		}

		/* Skip any zero-width characters. */
		for (; i < win->lines[win->paint_y].length && _T3_CHARDATA_TO_WIDTH(win->lines[win->paint_y].data[i]) == 0; i++) {}
		end_replace = i;

		end_spaces = pos_width > win->paint_x + width ? pos_width - win->paint_x - width : 0;

		for (j = i; j < win->lines[win->paint_y].length && _T3_CHARDATA_TO_WIDTH(win->lines[win->paint_y].data[j]) == 0; j++) {}

		/* Move the existing characters out of the way. */
		sdiff = n + end_spaces + start_spaces - (end_replace - start_replace);
		if (sdiff > 0 && !ensure_space(win->lines + win->paint_y, sdiff))
			return t3_false;

		memmove(win->lines[win->paint_y].data + end_replace + sdiff, win->lines[win->paint_y].data + end_replace,
			sizeof(t3_chardata_t) * (win->lines[win->paint_y].length - end_replace));

		for (i = start_replace; start_spaces > 0; start_spaces--)
			win->lines[win->paint_y].data[i++] = start_space_meta | ' ';
		memcpy(win->lines[win->paint_y].data + i, str, n * sizeof(t3_chardata_t));
		i += n;
		for (; end_spaces > 0; end_spaces--)
			win->lines[win->paint_y].data[i++] = end_space_meta | ' ';

		win->lines[win->paint_y].length += sdiff;
		if (win->lines[win->paint_y].start + win->lines[win->paint_y].width < width + win->paint_x)
			win->lines[win->paint_y].width = width + win->paint_x - win->lines[win->paint_y].start;
		if (win->lines[win->paint_y].start > win->paint_x) {
			win->lines[win->paint_y].width += win->lines[win->paint_y].start - win->paint_x;
			win->lines[win->paint_y].start = win->paint_x;
		}
	}
	win->paint_x += width;

	for (i = 0; i < extra_spaces; i++)
		result &= _win_add_chardata(win, &space, 1);

	return result;
}

/** Add a string with explicitly specified size to a t3_window_t with specified attributes.
    @param win The t3_window_t to add the string to.
    @param str The string to add.
    @param n The size of @p str.
    @param attr The attributes to use.
    @retval ::T3_ERR_SUCCESS on succes
    @retval ::T3_ERR_NONPRINT if a control character was encountered.
    @retval ::T3_ERR_ERRNO otherwise.

    The default attributes are combined with the specified attributes, with
    @p attr used as the priority attributes. All other t3_win_add* functions are
    (indirectly) implemented using this function.
*/
int t3_win_addnstr(t3_window_t *win, const char *str, size_t n, t3_attr_t attr) {
	size_t bytes_read, i;
	t3_chardata_t cd_buf[UTF8_MAX_BYTES + 1];
	uint32_t c;
	uint8_t char_info;
	int retval = T3_ERR_SUCCESS;
	int width;

	attr = t3_term_combine_attrs(attr, win->default_attrs);
	/* WARNING: when changing the internal representation of attributes, this must also be changed!!! */
	attr = _t3_term_attr_to_chardata(attr) & _T3_ATTR_MASK;

	for (; n > 0; n -= bytes_read, str += bytes_read) {
		bytes_read = n;
		c = t3_unicode_get(str, &bytes_read);

		char_info = t3_unicode_get_info(c, INT_MAX);
		width = T3_UNICODE_INFO_TO_WIDTH(char_info);
		if ((char_info & (T3_UNICODE_GRAPH_BIT | T3_UNICODE_SPACE_BIT)) == 0 || width < 0) {
			retval = T3_ERR_NONPRINT;
			continue;
		}

		cd_buf[0] = attr | WIDTH_TO_META(width) | (unsigned char) str[0];
		for (i = 1; i < bytes_read; i++)
			cd_buf[i] = (unsigned char) str[i];

		if (bytes_read > 1) {
			cd_buf[0] &= ~(_T3_ATTR_ACS | _T3_ATTR_FALLBACK_ACS);
		} else if ((cd_buf[0] & _T3_ATTR_ACS) && !t3_term_acs_available(cd_buf[0] & _T3_CHAR_MASK)) {
			cd_buf[0] &= ~_T3_ATTR_ACS;
			cd_buf[0] |= _T3_ATTR_FALLBACK_ACS;
		}
		if (!_win_add_chardata(win, cd_buf, bytes_read))
			return T3_ERR_ERRNO;
	}
	return retval;
}

/** Add a nul-terminated string to a t3_window_t with specified attributes.
    @param win The t3_window_t to add the string to.
    @param str The nul-terminated string to add.
    @param attr The attributes to use.
    @return See ::t3_win_addnstr.

	See ::t3_win_addnstr for further information.
*/
int t3_win_addstr(t3_window_t *win, const char *str, t3_attr_t attr) { return t3_win_addnstr(win, str, strlen(str), attr); }

/** Add a single character to a t3_window_t with specified attributes.
    @param win The t3_window_t to add the string to.
    @param c The character to add.
    @param attr The attributes to use.
    @return See ::t3_win_addnstr.

	@p c must be an ASCII character. See ::t3_win_addnstr for further information.
*/
int t3_win_addch(t3_window_t *win, char c, t3_attr_t attr) { return t3_win_addnstr(win, &c, 1, attr); }

/** Add a string with explicitly specified size to a t3_window_t with specified attributes and repetition.
    @param win The t3_window_t to add the string to.
    @param str The string to add.
    @param n The size of @p str.
    @param attr The attributes to use.
    @param rep The number of times to repeat @p str.
    @return See ::t3_win_addnstr.

	All other t3_win_add*rep functions are (indirectly) implemented using this
    function. See ::t3_win_addnstr for further information.
*/
int t3_win_addnstrrep(t3_window_t *win, const char *str, size_t n, t3_attr_t attr, int rep) {
	int i, ret;

	for (i = 0; i < rep; i++) {
		ret = t3_win_addnstr(win, str, n, attr);
		if (ret != 0)
			return ret;
	}
	return 0;
}

/** Add a nul-terminated string to a t3_window_t with specified attributes and repetition.
    @param win The t3_window_t to add the string to.
    @param str The nul-terminated string to add.
    @param attr The attributes to use.
    @param rep The number of times to repeat @p str.
    @return See ::t3_win_addnstr.

    See ::t3_win_addnstrrep for further information.
*/
int t3_win_addstrrep(t3_window_t *win, const char *str, t3_attr_t attr, int rep) {
	return t3_win_addnstrrep(win, str, strlen(str), attr, rep);
}

/** Add a character to a t3_window_t with specified attributes and repetition.
    @param win The t3_window_t to add the string to.
    @param c The character to add.
    @param attr The attributes to use.
    @param rep The number of times to repeat @p c.
    @return See ::t3_win_addnstr.

    See ::t3_win_addnstrrep for further information.
*/
int t3_win_addchrep(t3_window_t *win, char c, t3_attr_t attr, int rep) { return t3_win_addnstrrep(win, &c, 1, attr, rep); }

/** Get the next t3_window_t, when iterating over the t3_window_t's for drawing.
    @param ptr The last t3_window_t that was handled.
*/
static t3_window_t *_get_previous_window(t3_window_t *ptr) {
	if (ptr->tail != NULL)
		return ptr->tail;

	if (ptr->prev != NULL)
		return ptr->prev;

	while (ptr->parent != NULL) {
		if (ptr->parent->prev != NULL)
			return ptr->parent->prev;
		ptr = ptr->parent;
	}
	return NULL;
}

/** @internal
    @brief Redraw a terminal line, based on all visible t3_window_t structs.
    @param terminal The t3_window_t representing the cached terminal contents.
    @param line The line to redraw.
    @return A boolean indicating whether redrawing succeeded without memory errors.
*/
t3_bool _t3_win_refresh_term_line(int line) {
	line_data_t *draw;
	t3_window_t *ptr;
	int y, x, parent_y, parent_x, parent_max_y, parent_max_x;
	int data_start, length, paint_x;
	t3_bool result = t3_true;

	_t3_terminal_window->paint_y = line;
	_t3_terminal_window->lines[line].width = 0;
	_t3_terminal_window->lines[line].length = 0;
	_t3_terminal_window->lines[line].start = 0;

	for (ptr = _t3_tail; ptr != NULL; ptr = _get_previous_window(ptr)) {
		if (ptr->lines == NULL)
			continue;

		if (!_t3_win_is_shown(ptr))
			continue;

		if (ptr->parent == NULL) {
			parent_y = 0;
			parent_max_y = _t3_terminal_window->height;
			parent_x = 0;
			parent_max_x = _t3_terminal_window->width;
		} else {
			t3_window_t *parent = ptr->parent;
			parent_y = INT_MIN;
			parent_max_y = INT_MAX;
			parent_x = INT_MIN;
			parent_max_x = INT_MAX;

			do {
				int tmp;
				tmp = t3_win_get_abs_y(parent);
				if (tmp > parent_y)
					parent_y = tmp;
				tmp += parent->height;
				if (tmp < parent_max_y)
					parent_max_y = tmp;

				tmp = t3_win_get_abs_x(parent);
				if (tmp > parent_x)
					parent_x = tmp;
				tmp += parent->width;
				if (tmp < parent_max_x)
					parent_max_x = tmp;

				parent = parent->parent;
			} while (parent != NULL);
		}

		y = t3_win_get_abs_y(ptr);

		/* Skip lines outside the visible area, or that are clipped by the parent window. */
		if (y > line || y + ptr->height <= line || line < parent_y || line >= parent_max_y)
			continue;

		if (parent_x < 0)
			parent_x = 0;
		if (parent_max_x > _t3_terminal_window->width)
			parent_max_x = _t3_terminal_window->width;

		draw = ptr->lines + line - y;
		x = t3_win_get_abs_x(ptr);

		/* Skip lines that are fully clipped by the parent window. */
		if (x >= parent_max_x || x + draw->start + draw->width < parent_x)
			continue;

		data_start = 0;
		/* Draw/skip unused leading part of line. */
		if (x + draw->start >= parent_x) {
			int start;
			if (x + draw->start > parent_max_x)
				start = parent_max_x - x;
			else
				start = draw->start;

			if (ptr->default_attrs == 0) {
				_t3_terminal_window->paint_x = x + start;
			} else if (x >= parent_x) {
				_t3_terminal_window->paint_x = x;
				result &= t3_win_addchrep(_t3_terminal_window, ' ', ptr->default_attrs, start) == 0;
			} else {
				_t3_terminal_window->paint_x = parent_x;
				result &= t3_win_addchrep(_t3_terminal_window, ' ', ptr->default_attrs, start - parent_x + x) == 0;
			}
		} else /* if (x < parent_x) */ {
			_t3_terminal_window->paint_x = parent_x;
			for (paint_x = x + draw->start;
				paint_x + _T3_CHARDATA_TO_WIDTH(draw->data[data_start]) <= _t3_terminal_window->paint_x && data_start < draw->length;
				paint_x += _T3_CHARDATA_TO_WIDTH(draw->data[data_start]), data_start++) {}
			/* Add a space for the multi-cell character that is crossed by the parent clipping. */
			if (data_start < draw->length && paint_x < _t3_terminal_window->paint_x) {
				/* WARNING: when changing the internal representation of attributes, this must also be changed!!! */
				result &= t3_win_addch(_t3_terminal_window, ' ', (draw->data[data_start] & _T3_ATTR_MASK) >> _T3_ATTR_SHIFT) == 0;
				for (data_start++;
					data_start < draw->length && _T3_CHARDATA_TO_WIDTH(draw->data[data_start]) == 0;
					data_start++) {}
			}
		}

		paint_x = _t3_terminal_window->paint_x;
		for (length = data_start;
			length < draw->length && paint_x + _T3_CHARDATA_TO_WIDTH(draw->data[length]) <= parent_max_x;
			paint_x += _T3_CHARDATA_TO_WIDTH(draw->data[length]), length++) {}

		result &= _win_add_chardata(_t3_terminal_window, draw->data + data_start, length - data_start);

		/* Add a space for the multi-cell character that is crossed by the parent clipping. */
		if (length < draw->length && paint_x == parent_max_x - 1)
			/* WARNING: when changing the internal representation of attributes, this must also be changed!!! */
			result &= t3_win_addch(_t3_terminal_window, ' ', (draw->data[length] & _T3_ATTR_MASK) >> _T3_ATTR_SHIFT) == 0;

		if (ptr->default_attrs != 0 && draw->start + draw->width < ptr->width &&
				x + draw->start + draw->width < parent_max_x)
		{
			if (x + ptr->width <= parent_max_x)
				result &= t3_win_addchrep(_t3_terminal_window, ' ', ptr->default_attrs, ptr->width - draw->start - draw->width) == 0;
			else
				result &= t3_win_addchrep(_t3_terminal_window, ' ', ptr->default_attrs, parent_max_x - x - draw->start - draw->width) == 0;
		}
	}

	/* If a line does not start at position 0, just make it do so. This makes the whole repainting
	   bit a lot easier. */
	if (_t3_terminal_window->lines[line].start != 0) {
		t3_chardata_t space = ' ' | WIDTH_TO_META(1) | _t3_term_attr_to_chardata(_t3_terminal_window->default_attrs);
		_t3_terminal_window->paint_x = 0;
		result &= _win_add_chardata(_t3_terminal_window, &space, 1);
	}
	if (_t3_terminal_window->lines[line].width + _t3_terminal_window->lines[line].start < _t3_terminal_window->width) {
		t3_chardata_t space = ' ' | WIDTH_TO_META(1) | _t3_term_attr_to_chardata(_t3_terminal_window->default_attrs);
		_t3_terminal_window->paint_x = _t3_terminal_window->width - 1;
		result &= _win_add_chardata(_t3_terminal_window, &space, 1);
	}

	return result;
}

/** Clear current t3_window_t painting line to end. */
void t3_win_clrtoeol(t3_window_t *win) {
	if (win->paint_y >= win->height || win->lines == NULL)
		return;

	if (win->paint_x <= win->lines[win->paint_y].start) {
		win->lines[win->paint_y].length = 0;
		win->lines[win->paint_y].width = 0;
		win->lines[win->paint_y].start = 0;
	} else if (win->paint_x < win->lines[win->paint_y].start + win->lines[win->paint_y].width) {
		int sumwidth = win->lines[win->paint_y].start, i;
		for (i = 0; i < win->lines[win->paint_y].length &&
				sumwidth + _T3_CHARDATA_TO_WIDTH(win->lines[win->paint_y].data[i]) <= win->paint_x; i++)
			sumwidth += _T3_CHARDATA_TO_WIDTH(win->lines[win->paint_y].data[i]);

		if (sumwidth < win->paint_x) {
			int spaces = win->paint_x - sumwidth;
			if (spaces < win->lines[win->paint_y].length - i ||
					ensure_space(win->lines + win->paint_y, spaces - win->lines[win->paint_y].length + i)) {
				for (; spaces > 0; spaces--)
					win->lines[win->paint_y].data[i++] = WIDTH_TO_META(1) | ' ';
				sumwidth = win->paint_x;
			}
		}

		win->lines[win->paint_y].length = i;
		win->lines[win->paint_y].width = win->paint_x - win->lines[win->paint_y].start;
	}
}

#define ABORT_ON_FAIL(x) do { int retval; if ((retval = (x)) != 0) return retval; } while (0)

/** Draw a box on a t3_window_t.
    @param win The t3_window_t to draw on.
    @param y The line of the t3_window_t to start drawing on.
    @param x The column of the t3_window_t to start drawing on.
    @param height The height of the box to draw.
    @param width The width of the box to draw.
    @param attr The attributes to use for drawing.
    @return See ::t3_win_addnstr.
*/
int t3_win_box(t3_window_t *win, int y, int x, int height, int width, t3_chardata_t attr) {
	int i;

	attr = t3_term_combine_attrs(attr | T3_ATTR_ACS, win->default_attrs);

	if (y >= win->height || y + height > win->height ||
			x >= win->width || x + width > win->width || win->lines == NULL)
		return -1;

	t3_win_set_paint(win, y, x);
	ABORT_ON_FAIL(t3_win_addch(win, T3_ACS_ULCORNER, attr));
	ABORT_ON_FAIL(t3_win_addchrep(win, T3_ACS_HLINE, attr, width - 2));
	ABORT_ON_FAIL(t3_win_addch(win, T3_ACS_URCORNER, attr));
	for (i = 1; i < height - 1; i++) {
		t3_win_set_paint(win, y + i, x);
		ABORT_ON_FAIL(t3_win_addch(win, T3_ACS_VLINE, attr));
		t3_win_set_paint(win, y + i, x + width - 1);
		ABORT_ON_FAIL(t3_win_addch(win, T3_ACS_VLINE, attr));
	}
	t3_win_set_paint(win, y + height - 1, x);
	ABORT_ON_FAIL(t3_win_addch(win, T3_ACS_LLCORNER, attr));
	ABORT_ON_FAIL(t3_win_addchrep(win, T3_ACS_HLINE, attr, width - 2));
	ABORT_ON_FAIL(t3_win_addch(win, T3_ACS_LRCORNER, attr));
	return T3_ERR_SUCCESS;
}

/** Clear current t3_window_t painting line to end and all subsequent lines fully. */
void t3_win_clrtobot(t3_window_t *win) {
	if (win->lines == NULL)
		return;

	t3_win_clrtoeol(win);
	for (win->paint_y++; win->paint_y < win->height; win->paint_y++) {
		win->lines[win->paint_y].length = 0;
		win->lines[win->paint_y].width = 0;
		win->lines[win->paint_y].start = 0;
	}
}

/** @} */
