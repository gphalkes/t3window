#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <wchar.h>
#include <ctype.h>

#include "window.h"

/* FIXME: do we really want the windows to have a backing store? This will mean
   that we end up with approximately 3 copies of the screen contents. At least
   two if we clear the copy of the main window each time instead of using a full
   copy.
   The other option is to have only a backing copy for the terminal, and
   repaint handlers for the windows. If we want to make it really fancy we can
   do both. But given that for the most part we have non-overlapping windows
   and repaints are triggered by changes in the edit window, there is not really
   any need to have a backing window. Note though, that when we paint something
   in a (partially) obscured window we have to redraw the overlapping window
   again. */
//FIXME: do repainting per line
//FIXME: hide cursor when repainting
//FIXME: add routine to position the visible cursor
//FIXME: do we want to make sure we don't end up outside the window when painting? That
// would require nasty stuff like string widths, which require conversion :-(
//FIXME: implement "hardware" scrolling for optimization
//FIXME: add scrolling, because it can save a lot of repainting
/*FIXME main reasons to save the data printed to a window are:
- easier to restrict printing to designated area
- optimization when updates are needed
*/

#define INITIAL_ALLOC 80

#define WIDTH_TO_META(_w) (((_w) & 3) << CHAR_BIT)
#define ATTR_MASK (~((1 << (CHAR_BIT + 2)) - 1))
#define GET_WIDTH(_c) (((_c) >> CHAR_BIT) & 3)
#define META_MASK (~((1 << CHAR_BIT) - 1))
#define CHAR_MASK ((1 << CHAR_BIT) - 1)
#define WIDTH_MASK (3 << CHAR_BIT)

enum {
	ERR_ILSEQ,
	ERR_INCOMPLETE,
	ERR_NONPRINT,
	ERR_TRUNCATED
};

//FIXME: make sure that the base type is the correct size to store all the attributes
typedef int CharData;

typedef struct {
	CharData *data;
	int start;
	int width;
	int length;
	int allocated;
} LineData;

struct Window {
	int x, y;
	int paint_x, paint_y;
	int width, height;
	int depth;
	int attr;
	Bool shown;
	LineData *lines;

	/* Pointers for linking into depth sorted list. */
	Window *next;
	Window *prev;
};

/* Head and tail of depth sorted Window list */
Window *head, *tail;

static void _win_del(Window *win);

Window *win_new(int height, int width, int y, int x, int depth) {
	Window *retval, *ptr;
	int i;

	//FIXME: check parameter validity

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
	retval->shown = false;

	if (head == NULL) {
		tail = head = retval;
		retval->next = retval->prev = NULL;
		return retval;
	}

	ptr = head;
	while (ptr != NULL && ptr->depth < depth)
		ptr = ptr->next;

	if (ptr == NULL) {
		retval->prev = tail;
		retval->next = NULL;
		tail->next = retval;
	} else if (ptr->prev == NULL) {
		retval->prev = NULL;
		retval->next = ptr;
		head = retval;
	} else {
		retval->prev = ptr->prev;
		retval->next = ptr;
		ptr->prev->next = retval;
		ptr->prev = retval;
	}
	return retval;
}

static void _win_del(Window *win) {
	int i;
	if (win->lines != NULL) {
		for (i = 0; i < win->height; i++)
			free(win->lines[i].data);
		free(win->lines);
	}
	free(win);
}

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

Bool win_resize(Window *win, int height, int width) {
	int i;
	//FIXME validate parameters
	if (height > win->height) {
		void *result;
		if ((result = realloc(win->lines, height * sizeof(LineData))) == NULL)
			return false;
		memset(win->lines + win->height, 0, sizeof(LineData) * (height - win->height));
		for (i = win->height; i < height; i++) {
			if ((win->lines[i].data = malloc(sizeof(CharData) * INITIAL_ALLOC)) == NULL) {
				for (i = win->height; i < height && win->lines[i].data != NULL; i++)
					free(win->lines[i].data);
				return false;
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
		for (i = 0; i < height; i++) {
			if (win->lines[i].start > width) {
				win->lines[i].length = 0;
				win->lines[i].start = 0;
			} else if (win->lines[i].start + win->lines[i].width > width) {
				int sumwidth = win->lines[i].start, j;
				for (j = 0; j < win->lines[i].length && sumwidth < width; j++)
					sumwidth += GET_WIDTH(win->lines[i].data[j]);

				if (j < win->lines[i].length)
					win->lines[i].length = j > 0 ? j - 1 : 0;
			}
		}
	}

	win->height = height;
	win->width = width;
	return true;
}

void win_move(Window *win, int y, int x) {
	win->y = y;
	win->x = x;
}

int win_get_width(Window *win) {
	return win->width;
}

int win_get_height(Window *win) {
	return win->height;
}

void win_set_cursor(Window *win, int y, int x) {
	set_cursor(win->y + y, win->x + x);
}

void win_set_paint(Window *win, int y, int x) {
	win->paint_x = x;
	win->paint_y = y;
}

void win_set_attr(Window *win, int attr) {
	win->attr = attr;
}

void win_show(Window *win) {
	win->shown = true;
}

void win_hide(Window *win) {
	win->shown = false;
}

static void copy_mb(CharData *dest, const char *src, size_t n, CharData meta) {
	*dest++ = ((unsigned char) *src++) | meta;
	n--;
	while (n > 0)
		*dest++ = (unsigned char) *src++;
}

static Bool ensureSpace(LineData *line, size_t n) {
	int newsize;
	CharData *resized;

	/* FIXME: ensure that n + line->length will fit in int */
	if (n > INT_MAX)
		return false;

	if ((unsigned) line->allocated > line->length + n)
		return true;

	newsize = line->allocated;

	do {
		newsize *= 2;
		/* Sanity check for overflow of allocated variable. Prevents infinite loops. */
		if (!(newsize > line->length))
			return -1;
	} while ((unsigned) newsize - line->length < n);

	if ((resized = realloc(line->data, sizeof(CharData) * newsize)) == NULL)
		return false;
	line->data = resized;
	line->allocated = newsize;
	return true;
}

static Bool _win_mbaddch(Window *win, const char *str, size_t n, CharData meta) {
	int i, j;

	if (win->paint_y >= win->height)
		return true;
	if (win->paint_x > win->width)
		return true;
	if (win->paint_x == win->width && GET_WIDTH(meta) != 0)
		return true;
	if (win->paint_x + GET_WIDTH(meta) > win->width) {
		/* Add spaces to cover the rest of the line. */
		Bool result = true;
		char space = ' ';
		while (win->paint_x < win->width && result)
			result &= _win_mbaddch(win, &space, 1,  (meta & ATTR_MASK) | WIDTH_TO_META(1));
		return result;
	}

	if (GET_WIDTH(meta) == 0) {
		int width;
		/* Combining characters. */

		/* Simply drop characters that don't belong to any other character. */
		if (win->lines[win->paint_y].length == 0 ||
				win->paint_x <= win->lines[win->paint_y].start ||
				win->paint_x > win->lines[win->paint_y].start + win->lines[win->paint_y].width + 1)
			return true;

		if (!ensureSpace(win->lines + win->paint_y, n))
			return false;

		width = win->lines[win->paint_y].start;

		/* Locate the first character that at least partially overlaps the position
		   where this string is supposed to go. */
		for (i = 0; i < win->lines[win->paint_y].length; i++) {
			width += GET_WIDTH(win->lines[win->paint_y].data[i]);
			if (GET_WIDTH(win->lines[win->paint_y].data[i]) >= win->paint_x)
				break;
		}

		/* Check whether we are being asked to add a zero-width character in the middle
		   of a double-width character. If so, ignore. */
		if (width > win->paint_x)
			return true;

		/* Skip to the next non-zero-width character. */
		if (i < win->lines[win->paint_y].length)
			for (i++; i < win->lines[win->paint_y].length && GET_WIDTH(win->lines[win->paint_y].data[i]) == 0; i++) {}

		memmove(win->lines[win->paint_y].data + i + n, win->lines[win->paint_y].data + i, sizeof(CharData) * (win->lines[win->paint_y].length - i));
		copy_mb(win->lines[win->paint_y].data + i, str, n, meta);
		win->lines[win->paint_y].length += n;
	} else if (win->lines[win->paint_y].length == 0) {
		/* Empty line. */
		if (!ensureSpace(win->lines + win->paint_y, n))
			return false;
		win->lines[win->paint_y].start = win->paint_x;
		copy_mb(win->lines[win->paint_y].data, str, n, meta);
		win->lines[win->paint_y].length += n;
		win->lines[win->paint_y].width = GET_WIDTH(meta);
	} else if (win->lines[win->paint_y].start + win->lines[win->paint_y].width < win->paint_x) {
		/* Add characters after existing characters. */
		int diff = win->paint_x - (win->lines[win->paint_y].start + win->lines[win->paint_y].width);

		if (!ensureSpace(win->lines + win->paint_y, n + diff))
			return false;
		for (i = diff; i > 0; i--)
			win->lines[win->paint_y].data[win->lines[win->paint_y].length++] =  WIDTH_TO_META(1) | ' ';
		copy_mb(win->lines[win->paint_y].data + win->lines[win->paint_y].length, str, n, meta);
		win->lines[win->paint_y].length += n;
		win->lines[win->paint_y].width += GET_WIDTH(meta) + diff;
	} else if (win->paint_x + GET_WIDTH(meta) <= win->lines[win->paint_y].start) {
		/* Add characters before existing characters. */
		int diff = win->lines[win->paint_y].start - (win->paint_x + GET_WIDTH(meta));

		if (!ensureSpace(win->lines + win->paint_y, n + diff))
			return false;
		memmove(win->lines[win->paint_y].data + n + diff, win->lines[win->paint_y].data, sizeof(CharData) * win->lines[win->paint_y].length);
		copy_mb(win->lines[win->paint_y].data, str, n, meta);
		for (i = diff; i > 0; i++)
			win->lines[win->paint_y].data[n++] = WIDTH_TO_META(1) | ' ';
		win->lines[win->paint_y].length += n;
		win->lines[win->paint_y].width += GET_WIDTH(meta) + diff;
	} else {
		/* Character (partly) overwrite existing chars. */
		int width = win->lines[win->paint_y].start;
		size_t start_replace = 0, start_space_meta, start_spaces, end_replace, end_space_meta, end_spaces;
		int sdiff;

		/* Locate the first character that at least partially overlaps the position
		   where this string is supposed to go. */
		for (i = 0; i < win->lines[win->paint_y].length; i++) {
			if (width + GET_WIDTH(win->lines[win->paint_y].data[i]) > win->paint_x)
				break;
			width += GET_WIDTH(win->lines[win->paint_y].data[i]);
			start_replace = i;
		}

		/* If the character only partially overlaps, we replace the first part with
		   spaces with the attributes of the old character. */
		start_space_meta = (win->lines[win->paint_y].data[start_replace] & ATTR_MASK) | WIDTH_TO_META(1);
		start_spaces = win->paint_x - width;

		/* Now we need to find which other character(s) overlap. However, the current
		   string may overlap with a double width character but only for a single
		   position. In that case we will replace the trailing portion of the character
		   with spaces with the old character's attributes. */
		width += GET_WIDTH(win->lines[win->paint_y].data[start_replace]);

		end_replace = start_replace + 1;

		/* If the character where we start overwriting already fully overlaps with the
		   new string, then we need to only replace this and any spaces that result
		   from replacing the trailing portion need to use the start space attribute */
		if (width >= win->paint_x + GET_WIDTH(meta)) {
			end_space_meta = start_space_meta;
		} else {
			for (i = end_replace; i < win->lines[win->paint_y].length && width < win->paint_x + GET_WIDTH(meta); i++)
				width += GET_WIDTH(win->lines[win->paint_y].data[i]);

			end_space_meta = (win->lines[win->paint_y].data[j - 1] & ATTR_MASK) | WIDTH_TO_META(1);
		}

		/* Skip any zero-width characters. */
		if (i < win->lines[win->paint_y].length)
			for (i++; i < win->lines[win->paint_y].length && GET_WIDTH(win->lines[win->paint_y].data[i]) == 0; i++) {}
		end_replace = i;

		end_spaces = width - win->paint_x - GET_WIDTH(meta);

		for (; j < win->lines[win->paint_y].length && GET_WIDTH(win->lines[win->paint_y].data[j]) == 0; j++) {}

		/* Move the existing characters out of the way. */
		sdiff = n + end_spaces + start_spaces - (j - i);
		if (sdiff > 0 && !ensureSpace(win->lines + win->paint_y, sdiff))
			return false;

		memmove(win->lines[win->paint_y].data + j, win->lines[win->paint_y].data + j + sdiff, sizeof(CharData) * (win->lines[win->paint_y].length - j));
		for (; start_spaces > 0; start_spaces--)
			win->lines[win->paint_y].data[i++] = start_space_meta | ' ';
		copy_mb(win->lines[win->paint_y].data + i, str, n, meta);
		i += n;
		for (; end_spaces > 0; end_spaces--)
			win->lines[win->paint_y].data[i++] = end_space_meta | ' ';
	}
	win->paint_x += GET_WIDTH(meta);
	return true;
}

int win_mbaddnstra(Window *win, const char *str, size_t n, int attr) {
	size_t result;
	int width;
	mbstate_t mbstate;
	wchar_t c[2];
	char buf[MB_LEN_MAX + 1];
	int retval = 0;

	memset(&mbstate, 0, sizeof(mbstate_t));
	attr = attr & ATTR_MASK;

	while (n > 0) {
		result = mbrtowc(c, str, n, &mbstate);
		/* Handle error conditions. Because embedded L'\0' characters have a
		   zero result, they cannot be skipped. */
		if (result == 0)
			return ERR_TRUNCATED;
		else if (result == (size_t)(-1))
			return ERR_ILSEQ;
		else if (result == (size_t)(-2))
			return ERR_INCOMPLETE;

		width = wcwidth(c[0]);
		if (width < 0) {
			retval = ERR_NONPRINT;
			str += result;
			n -= result;
			continue;
		}
		c[1] = L'\0';
		n -= result;
		str += result;

		/* Convert the wchar_t back to an mb string. The reason for doing this
		   is that we want to make sure that the encoding is in the initial
		   shift state before and after we print this character. This allows
		   separate printing of the character. */
		result = wcstombs(buf, c, MB_LEN_MAX + 1);
		/*FIXME: should we check for conversion errors? We probably do for
		  16 bit wchar_t's because those may need to be handled differently */

		_win_mbaddch(win, buf, result, WIDTH_TO_META(width) | attr);
	}
	return retval;
}

int win_mbaddnstr(Window *win, const char *str, size_t n) { return win_mbaddnstra(win, str, n, win->attr); }
int win_mbaddstra(Window *win, const char *str, int attr) { return win_mbaddnstra(win, str, strlen(str), attr); }
int win_mbaddstr(Window *win, const char *str) { return win_mbaddnstra(win, str, strlen(str), win->attr); }

static Bool _win_addnstra(Window *win, const char *str, size_t n, int attr) {
	size_t i;
	Bool result = true;

	/* FIXME: it would seem that this can be done more efficiently, especially
	   if no multibyte characters are used at all. */
	for (i = 0; i < n; i++)
		result &= _win_mbaddch(win, str + i, 1, WIDTH_TO_META(1) | attr);

	return result;
}

int win_addnstra(Window *win, const char *str, size_t n, int attr) {
	size_t i, print_from = 0;
	int retval = 0;

	attr &= ATTR_MASK;
	for (i = 0; i < n; i++) {
		if (!isprint(str[i])) {
			retval = ERR_NONPRINT;
			if (print_from < i)
				_win_addnstra(win, str + print_from, i - print_from, attr);
			print_from = i + 1;
		}
	}
	if (print_from < i)
		_win_addnstra(win, str + print_from, i - print_from, attr);
	return retval;
}

int win_addnstr(Window *win, const char *str, size_t n) { return win_addnstra(win, str, n, win->attr); }
int win_addstra(Window *win, const char *str, int attr) { return win_addnstra(win, str, strlen(str), attr); }
int win_addstr(Window *win, const char *str) { return win_addnstra(win, str, strlen(str), win->attr); }
