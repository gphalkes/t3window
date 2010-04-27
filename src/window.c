#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <ctype.h>

#include "window.h"
#include "internal.h"

#include "unicode/tdunicode.h"

/* TODO list:
- ATTR_ACS should only be allowed on chars below 128 etc. Otherwise interpretation
  of width info may be weird.
- make win_clrtobol
*/

/** @internal
    @brief The maximum size of a UTF-8 character in bytes. Used in ::_win_addnstr.
*/
#define UTF8_MAX_BYTES 4

static Window *head, /**< Head of depth sorted Window list. */
	*tail; /**< Tail of depth sorted Window list. */

static void _win_del(Window *win);
static Bool ensureSpace(LineData *line, size_t n);

/** Create a new Window.
    @param height The desired height in terminal lines.
    @param width The desired width in terminal columns.
    @param y The vertical location of the window in terminal lines.
    @param x The horizontal location of the window in terminal columns.
	@param depth The depth of the window in the stack of windows.
	@return A pointer to a new Window struct or @c NULL on error.

    The @p depth parameter determines the z-order of the windows. Windows
	with lower depth will hide windows with higher depths.
*/
Window *win_new(int height, int width, int y, int x, int depth) {
	Window *retval, *ptr;
	int i;

	if (height <= 0 || width <= 0)
		return NULL;

	if ((retval = calloc(1, sizeof(Window))) == NULL)
		return NULL;

	if ((retval->lines = calloc(1, sizeof(LineData) * height)) == NULL) {
		_win_del(retval);
		return NULL;
	}

	for (i = 0; i < height; i++) {
		if ((retval->lines[i].data = malloc(sizeof(CharData) * INITIAL_ALLOC)) == NULL) {
			_win_del(retval);
			return NULL;
		}
		retval->lines[i].allocated = INITIAL_ALLOC;
	}

	retval->x = x;
	retval->y = y;
	retval->paint_x = 0;
	retval->paint_y = 0;
	retval->width = width;
	retval->height = height;
	retval->depth = depth;
	retval->shown = False;
	retval->default_attrs = 0;

	if (head == NULL) {
		tail = head = retval;
		retval->next = retval->prev = NULL;
		return retval;
	}

	/* Insert new window at the correct place in the sorted list of windows. */
	ptr = head;
	while (ptr != NULL && ptr->depth < depth)
		ptr = ptr->next;

	if (ptr == NULL) {
		retval->prev = tail;
		retval->next = NULL;
		tail->next = retval;
		tail = retval;
	} else if (ptr->prev == NULL) {
		retval->prev = NULL;
		retval->next = ptr;
		head->prev = retval;
		head = retval;
	} else {
		retval->prev = ptr->prev;
		retval->next = ptr;
		ptr->prev->next = retval;
		ptr->prev = retval;
	}
	return retval;
}

/** Create a new Window with relative position.
    @param height The desired height in terminal lines.
    @param width The desired width in terminal columns.
    @param y The vertical location of the window in terminal lines.
    @param x The horizontal location of the window in terminal columns.
	@param depth The depth of the window in the stack of windows.
    @param parent The window used as reference for relative positioning.
    @param relation The relation between this window and @p parent (see WinAnchor).
	@return A pointer to a new Window struct or @c NULL on error.

    The @p depth parameter determines the z-order of the windows. Windows
	with lower depth will hide windows with higher depths.
*/
Window *win_new_relative(int height, int width, int y, int x, int depth, Window *parent, int relation) {
	Window *retval;

	if (parent == NULL && GETPARENT(relation) != ANCHOR_ABSOLUTE)
			return NULL;

	if (GETPARENT(relation) != ANCHOR_TOPLEFT && GETPARENT(relation) != ANCHOR_TOPRIGHT &&
			GETPARENT(relation) != ANCHOR_BOTTOMLEFT && GETPARENT(relation) != ANCHOR_BOTTOMRIGHT &&
			GETPARENT(relation) != ANCHOR_ABSOLUTE) {
		return NULL;
	}

	if (GETCHILD(relation) != ANCHOR_TOPLEFT && GETCHILD(relation) != ANCHOR_TOPRIGHT &&
			GETCHILD(relation) != ANCHOR_BOTTOMLEFT && GETCHILD(relation) != ANCHOR_BOTTOMRIGHT &&
			GETCHILD(relation) != ANCHOR_ABSOLUTE) {
		return NULL;
	}

	retval = win_new(height, width, y, x, depth);
	if (retval == NULL)
		return retval;

	retval->parent = parent;
	retval->relation = relation;
	if (depth == INT_MIN && parent != NULL && parent->depth != INT_MIN)
		retval->depth = parent->depth - 1;
	return retval;
}

/** Free a Window struct.

    This function only @c free's the memory associated with the Window. For
    a full clean-up, call ::win_del.
*/
static void _win_del(Window *win) {
	int i;

	if (win == NULL)
		return;

	if (win->lines != NULL) {
		for (i = 0; i < win->height; i++)
			free(win->lines[i].data);
		free(win->lines);
	}
	free(win);
}

/** Set default attributes for a window.
    @param win The Window to set the default attributes for.
    @param attr The attributes to set.

    This function can be used to set a default background for the entire window, as
    well as any other attributes.
*/
void win_set_default_attrs(Window *win, CharData attr) {
	win->default_attrs = attr & ATTR_MASK;
}

/** Discard a Window. */
void win_del(Window *win) {
	if (win->next == NULL)
		tail = win->prev;
	else
		win->next->prev = win->prev;

	if (win->prev == NULL)
		head = win->next;
	else
		win->prev->next = win->next;
	_win_del(win);
}

/** Change a Window's size.
    @param win The Window to change the size of.
    @param height The desired new height of the Window in terminal lines.
    @param width The desired new width of the Window in terminal columns.
    @return A boolean indicating succes, depending on the validity of the
        parameters and whether reallocation of the internal data
        structures succeeds.
*/
Bool win_resize(Window *win, int height, int width) {
	int i;

	if (height <= 0 || width <= 0)
		return False;

	if (height > win->height) {
		void *result;
		if ((result = realloc(win->lines, height * sizeof(LineData))) == NULL)
			return False;
		win->lines = result;
		memset(win->lines + win->height, 0, sizeof(LineData) * (height - win->height));
		for (i = win->height; i < height; i++) {
			if ((win->lines[i].data = malloc(sizeof(CharData) * INITIAL_ALLOC)) == NULL) {
				for (i = win->height; i < height && win->lines[i].data != NULL; i++)
					free(win->lines[i].data);
				return False;
			}
			win->lines[i].allocated = INITIAL_ALLOC;
		}
	} else if (height < win->height) {
		for (i = height; i < win->height; i++)
			free(win->lines[i].data);
		memset(win->lines + height, 0, sizeof(LineData) * (win->height - height));
	}

	if (width < win->width) {
		/* Chop lines to maximum width */
		/* FIXME: should we also try to resize the lines (as in realloc)? */
		int paint_x = win->paint_x, paint_y = win->paint_y;

		for (i = 0; i < height; i++) {
			win_set_paint(win, i, width);
			win_clrtoeol(win);
		}

		win->paint_x = paint_x;
		win->paint_y = paint_y;
	}

	win->height = height;
	win->width = width;
	return True;
}

/** Change a Window's position.
    @param win The Window to change the position of.
    @param y The desired new vertical position of the Window in terminal lines.
    @param x The desired new horizontal position of the Window in terminal lines.

    This function will always succeed as it only updates the internal book keeping.
*/
void win_move(Window *win, int y, int x) {
	win->y = y;
	win->x = x;
}

/** Get a Window's width. */
int win_get_width(Window *win) {
	return win->width;
}

/** Get a Window's height. */
int win_get_height(Window *win) {
	return win->height;
}

/** Get a Window's horizontal position.

    The retrieved position may be relative to another window. Use ::win_get_abs_x
    to find the absolute position.
*/
int win_get_x(Window *win) {
	return win->x;
}

/** Get a Window's vertical position.

    The retrieved position may be relative to another window. Use ::win_get_abs_y
    to find the absolute position.
*/

int win_get_y(Window *win) {
	return win->y;
}

/** Get a Window's depth. */
int win_get_depth(Window *win) {
	return win->depth;
}

/** Get a Window's relative positioning information.
    @param win The Window to get the relative positioning information for.
    @param [out] parent The location to store the pointer to the Window relative to
	    which the position is specified.
    @return The relative positioning method.

    To retrieve the separate parts of the relative positioning information, use
    ::GETPARENT and ::GETCHILD.
*/
int win_get_relation(Window *win, Window **parent) {
	if (parent != NULL)
		*parent = win->parent;
	return win->relation;
}

/** Get a Window's absolute horizontal position. */
int win_get_abs_x(Window *win) {
	int result;
	switch (GETPARENT(win->relation)) {
		case ANCHOR_TOPLEFT:
		case ANCHOR_BOTTOMLEFT:
			result = win->x + win_get_abs_x(win->parent);
			break;
		case ANCHOR_TOPRIGHT:
		case ANCHOR_BOTTOMRIGHT:
			result = win_get_abs_x(win->parent) + win->parent->width + win->x;
			break;
		default:
			result = win->x;
			break;
	}

	switch (GETCHILD(win->relation)) {
		case ANCHOR_TOPRIGHT:
		case ANCHOR_BOTTOMRIGHT:
			return result - win->width;
		default:
			return result;
	}
}

/** Get a Window's absolute vertical position. */
int win_get_abs_y(Window *win) {
	int result;
	switch (GETPARENT(win->relation)) {
		case ANCHOR_TOPLEFT:
		case ANCHOR_TOPRIGHT:
			result = win->y + win_get_abs_y(win->parent);
			break;
		case ANCHOR_BOTTOMLEFT:
		case ANCHOR_BOTTOMRIGHT:
			result = win_get_abs_y(win->parent) + win->parent->height + win->y;
			break;
		default:
			result = win->y;
			break;
	}

	switch (GETCHILD(win->relation)) {
		case ANCHOR_BOTTOMLEFT:
		case ANCHOR_BOTTOMRIGHT:
			return result - win->height;
		default:
			return result;
	}
}

/** Position the cursor relative to a Window.
    @param win The Window to position the cursor in.
    @param y The line relative to @p win to position the cursor at.
    @param x The column relative to @p win to position the cursor at.

    The cursor is only moved if the window is currently shown.
*/
void win_set_cursor(Window *win, int y, int x) {
	if (win->shown)
		term_set_cursor(win_get_abs_y(win) + y, win_get_abs_x(win) + x);
}

/** Change the position where characters are written to the Window. */
void win_set_paint(Window *win, int y, int x) {
	win->paint_x = x < 0 ? 0 : x;
	win->paint_y = y < 0 ? 0 : y;
}

/** Make a Window visible. */
void win_show(Window *win) {
	win->shown = True;
}

/** Make a Window invisible. */
void win_hide(Window *win) {
	win->shown = False;
}

/** Ensure that a LineData struct has at least a specified number of
        bytes of unused space.
	@param line The LineData struct to check.
	@param n The required unused space in bytes.
    @return A boolean indicating whether, after possibly reallocating, the
        requested number of bytes is available.
*/
static Bool ensureSpace(LineData *line, size_t n) {
	int newsize;
	CharData *resized;

	/* FIXME: ensure that n + line->length will fit in int */
	if (n > INT_MAX)
		return False;

	if ((unsigned) line->allocated > line->length + n)
		return True;

	newsize = line->allocated;

	do {
		newsize *= 2;
		/* Sanity check for overflow of allocated variable. Prevents infinite loops. */
		if (!(newsize > line->length))
			return -1;
	} while ((unsigned) newsize - line->length < n);

	if ((resized = realloc(line->data, sizeof(CharData) * newsize)) == NULL)
		return False;
	line->data = resized;
	line->allocated = newsize;
	return True;
}

/** Add character data to a Window at the current painting position.
    @param win The Window to add the characters to.
    @param str The characters to add as CharData.
    @param n The length of @p str.
    @return A boolean indicating whether insertion succeeded (only fails on memory
        allocation errors).

    This is the most complex and most central function of the library. All character
    drawing eventually ends up here.
*/
static Bool _win_add_chardata(Window *win, CharData *str, size_t n) {
	int width = 0;
	int extra_spaces = 0;
	int i, j;
	size_t k;
	Bool result = True;
	CharData space = ' ' | win->default_attrs;

	if (win->paint_y >= win->height)
		return True;
	if (win->paint_x >= win->width)
		return True;

	for (k = 0; k < n; k++) {
		if (win->paint_x + width + GET_WIDTH(str[k]) > win->width)
			break;
		width += GET_WIDTH(str[k]);
	}

	if (k < n)
		extra_spaces = win->width - win->paint_x - width;
	n = k;

	if (width == 0) {
		int pos_width;
		/* Combining characters. */

		/* Simply drop characters that don't belong to any other character. */
		if (win->lines[win->paint_y].length == 0 ||
				win->paint_x <= win->lines[win->paint_y].start ||
				win->paint_x > win->lines[win->paint_y].start + win->lines[win->paint_y].width + 1)
			return True;

		if (!ensureSpace(win->lines + win->paint_y, n))
			return False;

		pos_width = win->lines[win->paint_y].start;

		/* Locate the first character that at least partially overlaps the position
		   where this string is supposed to go. */
		for (i = 0; i < win->lines[win->paint_y].length; i++) {
			pos_width += GET_WIDTH(win->lines[win->paint_y].data[i]);
			if (pos_width >= win->paint_x)
				break;
		}

		/* Check whether we are being asked to add a zero-width character in the middle
		   of a double-width character. If so, ignore. */
		if (pos_width > win->paint_x)
			return True;

		/* Skip to the next non-zero-width character. */
		if (i < win->lines[win->paint_y].length)
			for (i++; i < win->lines[win->paint_y].length && GET_WIDTH(win->lines[win->paint_y].data[i]) == 0; i++) {}

		memmove(win->lines[win->paint_y].data + i + n, win->lines[win->paint_y].data + i, sizeof(CharData) * (win->lines[win->paint_y].length - i));
		memcpy(win->lines[win->paint_y].data + i, str, n * sizeof(CharData));
		win->lines[win->paint_y].length += n;
	} else if (win->lines[win->paint_y].length == 0) {
		/* Empty line. */
		if (!ensureSpace(win->lines + win->paint_y, n))
			return False;
		win->lines[win->paint_y].start = win->paint_x;
		memcpy(win->lines[win->paint_y].data, str, n * sizeof(CharData));
		win->lines[win->paint_y].length += n;
		win->lines[win->paint_y].width = width;
	} else if (win->lines[win->paint_y].start + win->lines[win->paint_y].width <= win->paint_x) {
		/* Add characters after existing characters. */
		int diff = win->paint_x - (win->lines[win->paint_y].start + win->lines[win->paint_y].width);

		if (!ensureSpace(win->lines + win->paint_y, n + diff))
			return False;
		for (i = diff; i > 0; i--)
			win->lines[win->paint_y].data[win->lines[win->paint_y].length++] = WIDTH_TO_META(1) | ' ' | win->default_attrs;
		memcpy(win->lines[win->paint_y].data + win->lines[win->paint_y].length, str, n * sizeof(CharData));
		win->lines[win->paint_y].length += n;
		win->lines[win->paint_y].width += width + diff;
	} else if (win->paint_x + width <= win->lines[win->paint_y].start) {
		/* Add characters before existing characters. */
		int diff = win->lines[win->paint_y].start - (win->paint_x + width);

		if (!ensureSpace(win->lines + win->paint_y, n + diff))
			return False;
		memmove(win->lines[win->paint_y].data + n + diff, win->lines[win->paint_y].data, sizeof(CharData) * win->lines[win->paint_y].length);
		memcpy(win->lines[win->paint_y].data, str, n * sizeof(CharData));
		for (i = diff; i > 0; i--)
			win->lines[win->paint_y].data[n++] = WIDTH_TO_META(1) | ' ' | win->default_attrs;
		win->lines[win->paint_y].length += n;
		win->lines[win->paint_y].width += width + diff;
		win->lines[win->paint_y].start = win->paint_x;
	} else {
		/* Character (partly) overwrite existing chars. */
		int pos_width = win->lines[win->paint_y].start;
		size_t start_replace = 0, start_space_meta, start_spaces, end_replace, end_space_meta, end_spaces;
		int sdiff;

		/* Locate the first character that at least partially overlaps the position
		   where this string is supposed to go. */
		for (i = 0; i < win->lines[win->paint_y].length && pos_width + GET_WIDTH(win->lines[win->paint_y].data[i]) <= win->paint_x; i++)
			pos_width += GET_WIDTH(win->lines[win->paint_y].data[i]);
		start_replace = i;

		/* If the character only partially overlaps, we replace the first part with
		   spaces with the attributes of the old character. */
		start_space_meta = (win->lines[win->paint_y].data[start_replace] & ATTR_MASK) | WIDTH_TO_META(1);
		start_spaces = win->paint_x >= win->lines[win->paint_y].start ? win->paint_x - pos_width : 0;

		/* Now we need to find which other character(s) overlap. However, the current
		   string may overlap with a double width character but only for a single
		   position. In that case we will replace the trailing portion of the character
		   with spaces with the old character's attributes. */
		pos_width += GET_WIDTH(win->lines[win->paint_y].data[start_replace]);

		i++;

		/* If the character where we start overwriting already fully overlaps with the
		   new string, then we need to only replace this and any spaces that result
		   from replacing the trailing portion need to use the start space attribute */
		if (pos_width >= win->paint_x + width) {
			end_space_meta = start_space_meta;
		} else {
			for (; i < win->lines[win->paint_y].length && pos_width < win->paint_x + width; i++)
				pos_width += GET_WIDTH(win->lines[win->paint_y].data[i]);

			end_space_meta = (win->lines[win->paint_y].data[i - 1] & ATTR_MASK) | WIDTH_TO_META(1);
		}

		/* Skip any zero-width characters. */
		for (; i < win->lines[win->paint_y].length && GET_WIDTH(win->lines[win->paint_y].data[i]) == 0; i++) {}
		end_replace = i;

		end_spaces = pos_width > win->paint_x + width ? pos_width - win->paint_x - width : 0;

		for (j = i; j < win->lines[win->paint_y].length && GET_WIDTH(win->lines[win->paint_y].data[j]) == 0; j++) {}

		/* Move the existing characters out of the way. */
		sdiff = n + end_spaces + start_spaces - (end_replace - start_replace);
		if (sdiff > 0 && !ensureSpace(win->lines + win->paint_y, sdiff))
			return False;

		memmove(win->lines[win->paint_y].data + end_replace + sdiff, win->lines[win->paint_y].data + end_replace,
			sizeof(CharData) * (win->lines[win->paint_y].length - end_replace));

		for (i = start_replace; start_spaces > 0; start_spaces--)
			win->lines[win->paint_y].data[i++] = start_space_meta | ' ';
		memcpy(win->lines[win->paint_y].data + i, str, n * sizeof(CharData));
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

/** Add a string with explicitly specified size to a Window with specified attributes.
    @param win The Window to add the string to.
    @param str The string to add.
    @param n The size of @p str.
    @param attr The attributes to use.
    @retval ERR_SUCCESS on succes
    @retval ERR_NONPRINT if a control character was encountered.
    @retval ERR_ERRNO otherwise.

    The default attributes are combined with the specified attributes, with
    @p attr used as the priority attributes. All other win_add* functions are
    (indirectly) implemented using this function.
*/
int win_addnstr(Window *win, const char *str, size_t n, CharData attr) {
	size_t bytes_read, i;
	CharData cd_buf[UTF8_MAX_BYTES + 1];
	uint32_t c;
	uint8_t char_info;
	int retval = ERR_SUCCESS;
	int width;

	attr = term_combine_attrs(attr & ATTR_MASK, win->default_attrs);

	for (; n > 0; n -= bytes_read, str += bytes_read) {
		bytes_read = n;
		c = tdu_getuc(str, &bytes_read);

		char_info = tdu_get_info(c);
		width = TDU_INFO_TO_WIDTH(char_info);
		if ((char_info & (TDU_GRAPH_BIT | TDU_SPACE_BIT)) == 0 || width < 0) {
			retval = ERR_NONPRINT;
			continue;
		}

		cd_buf[0] = attr | WIDTH_TO_META(width) | (unsigned char) str[0];
		for (i = 1; i < bytes_read; i++)
			cd_buf[i] = (unsigned char) str[i];

		if (bytes_read > 1) {
			cd_buf[0] &= ~ATTR_ACS;
		} else if ((cd_buf[0] & ATTR_ACS) && !term_acs_available(cd_buf[0] & CHAR_MASK)) {
			int replacement = _term_get_default_acs(cd_buf[0] & CHAR_MASK);
			cd_buf[0] &= ~(ATTR_ACS | CHAR_MASK);
			cd_buf[0] |= replacement & CHAR_MASK;
		}
		if (!_win_add_chardata(win, cd_buf, bytes_read))
			return ERR_ERRNO;
	}
	return retval;
}

/** Add a nul-terminated string to a Window with specified attributes.
    @param win The Window to add the string to.
    @param str The nul-terminated string to add.
    @param attr The attributes to use.
    @return See ::win_addnstr.

	See ::win_addnstr for further information.
*/
int win_addstr(Window *win, const char *str, CharData attr) { return win_addnstr(win, str, strlen(str), attr); }

/** Add a single character to a Window with specified attributes.
    @param win The Window to add the string to.
    @param c The character to add.
    @param attr The attributes to use.
    @return See ::win_addnstr.

	@p c must be an ASCII character. See ::win_addnstr for further information.
*/
int win_addch(Window *win, char c, CharData attr) { return win_addnstr(win, &c, 1, attr); }

/** Add a string with explicitly specified size to a Window with specified attributes and repetition.
    @param win The Window to add the string to.
    @param str The string to add.
    @param n The size of @p str.
    @param attr The attributes to use.
    @param rep The number of times to repeat @p str.
    @return See ::win_addnstr.

	All other win_add*rep functions are (indirectly) implemented using this
    function. See ::win_addnstr for further information.
*/
int win_addnstrrep(Window *win, const char *str, size_t n, CharData attr, int rep) {
	int i, ret;

	for (i = 0; i < rep; i++) {
		ret = win_addnstr(win, str, n, attr);
		if (ret != 0)
			return ret;
	}
	return 0;
}

/** Add a nul-terminated string to a Window with specified attributes and repetition.
    @param win The Window to add the string to.
    @param str The nul-terminated string to add.
    @param attr The attributes to use.
    @param rep The number of times to repeat @p str.
    @return See ::win_addnstr.

    See ::win_addnstrrep for further information.
*/
int win_addstrrep(Window *win, const char *str, CharData attr, int rep) { return win_addnstrrep(win, str, strlen(str), attr, rep); }

/** Add a character to a Window with specified attributes and repetition.
    @param win The Window to add the string to.
    @param c The character to add.
    @param attr The attributes to use.
    @param rep The number of times to repeat @p c.
    @return See ::win_addnstr.

    See ::win_addnstrrep for further information.
*/
int win_addchrep(Window *win, char c, CharData attr, int rep) { return win_addnstrrep(win, &c, 1, attr, rep); }

/** @internal
    @brief Redraw a terminal line, based on all visible Window structs.
    @param terminal The Window representing the cached terminal contents.
    @param line The line to redraw.
    @return A boolean indicating whether redrawing succeeded without memory errors.
*/
Bool _win_refresh_term_line(Window *terminal, int line) {
	LineData *draw;
	Window *ptr;
	int y;
	Bool result = True;

	/* FIXME: check return value of called functions for memory allocation errors. */
	terminal->paint_y = line;
	terminal->lines[line].width = 0;
	terminal->lines[line].length = 0;
	terminal->lines[line].start = 0;

	for (ptr = tail; ptr != NULL; ptr = ptr->prev) {
		if (!ptr->shown)
			continue;

		y = win_get_abs_y(ptr);
		if (y > line || y + ptr->height <= line)
			continue;

		draw = ptr->lines + line - y;
		terminal->paint_x = win_get_abs_x(ptr);
		if (ptr->default_attrs == 0)
			terminal->paint_x += draw->start;
		else
			result &= win_addchrep(terminal, ' ', ptr->default_attrs, draw->start) == 0;
		result &= _win_add_chardata(terminal, draw->data, draw->length);
		if (ptr->default_attrs != 0 && draw->start + draw->width < ptr->width)
			result &= win_addchrep(terminal, ' ', ptr->default_attrs, ptr->width - draw->start - draw->width) == 0;
	}

	/* If a line does not start at position 0, just make it do so. This makes the whole repainting
	   bit a lot easier. */
	if (terminal->lines[line].start != 0) {
		CharData space = ' ' | WIDTH_TO_META(1);
		terminal->paint_x = 0;
		result &= _win_add_chardata(terminal, &space, 1);
	}

	return result;
}

/** Clear current Window painting line to end. */
void win_clrtoeol(Window *win) {
	if (win->paint_y >= win->height)
		return;

	if (win->paint_x <= win->lines[win->paint_y].start) {
		win->lines[win->paint_y].length = 0;
		win->lines[win->paint_y].width = 0;
		win->lines[win->paint_y].start = 0;
	} else if (win->paint_x < win->lines[win->paint_y].start + win->lines[win->paint_y].width) {
		int sumwidth = win->lines[win->paint_y].start, i;
		for (i = 0; i < win->lines[win->paint_y].length && sumwidth + GET_WIDTH(win->lines[win->paint_y].data[i]) <= win->paint_x; i++)
			sumwidth += GET_WIDTH(win->lines[win->paint_y].data[i]);

		if (sumwidth < win->paint_x) {
			int spaces = win->paint_x - sumwidth;
			if (spaces < win->lines[win->paint_y].length - i ||
					ensureSpace(win->lines + win->paint_y, spaces - win->lines[win->paint_y].length + i)) {
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

/** Draw a box on a Window.
    @param win The Window to draw on.
    @param y The line of the Window to start drawing on.
    @param x The column of the Window to start drawing on.
    @param height The height of the box to draw.
    @param width The width of the box to draw.
    @param attr The attributes to use for drawing.
    @return See ::win_addnstr.
*/
int win_box(Window *win, int y, int x, int height, int width, CharData attr) {
	int i;

	attr = term_combine_attrs(attr, win->default_attrs);

	if (y >= win->height || y + height > win->height ||
			x >= win->width || x + width > win->width)
		return -1;

	win_set_paint(win, y, x);
	ABORT_ON_FAIL(win_addch(win, TERM_ULCORNER, attr | ATTR_ACS));
	ABORT_ON_FAIL(win_addchrep(win, TERM_HLINE, attr | ATTR_ACS, width - 2));
	ABORT_ON_FAIL(win_addch(win, TERM_URCORNER, attr | ATTR_ACS));
	for (i = 1; i < height - 1; i++) {
		win_set_paint(win, y + i, x);
		ABORT_ON_FAIL(win_addch(win, TERM_VLINE, attr | ATTR_ACS));
		win_set_paint(win, y + i, x + width - 1);
		ABORT_ON_FAIL(win_addch(win, TERM_VLINE, attr | ATTR_ACS));
	}
	win_set_paint(win, y + height - 1, x);
	ABORT_ON_FAIL(win_addch(win, TERM_LLCORNER, attr | ATTR_ACS));
	ABORT_ON_FAIL(win_addchrep(win, TERM_HLINE, attr | ATTR_ACS, width - 2));
	ABORT_ON_FAIL(win_addch(win, TERM_LRCORNER, attr | ATTR_ACS));
	return ERR_SUCCESS;
}

/** Clear current Window painting line to end and all subsequent lines fully. */
void win_clrtobot(Window *win) {
	win_clrtoeol(win);
	for (win->paint_y++; win->paint_y < win->height; win->paint_y++) {
		win->lines[win->paint_y].length = 0;
		win->lines[win->paint_y].width = 0;
		win->lines[win->paint_y].start = 0;
	}
}
