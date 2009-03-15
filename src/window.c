#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <ctype.h>

#include "window.h"
#include "internal.h"
//FIXME: implement "hardware" scrolling for optimization
//FIXME: add scrolling, because it can save a lot of repainting

enum {
	ERR_ILSEQ,
	ERR_INCOMPLETE,
	ERR_NONPRINT,
	ERR_TRUNCATED
};

/* Head and tail of depth sorted Window list */
Window *head, *tail;

static void _win_del(Window *win);
static Bool ensureSpace(LineData *line, size_t n);

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
		//FIXME: should we also try to resize the lines (as in realloc)?
		for (i = 0; i < height; i++) {
			if (win->lines[i].start > width) {
				win->lines[i].length = 0;
				win->lines[i].start = 0;
			} else if (win->lines[i].start + win->lines[i].width > width) {
				int sumwidth = win->lines[i].start, j;
				for (j = 0; j < win->lines[i].length && sumwidth + GET_WIDTH(win->lines[i].data[j]) <= width; j++)
					sumwidth += GET_WIDTH(win->lines[i].data[j]);

				if (sumwidth < width) {
					int spaces = width - sumwidth;
					if (spaces < win->lines[i].length - j ||
							ensureSpace(win->lines + i, spaces - win->lines[i].length + j)) {
						for (; spaces > 0; spaces--)
							win->lines[i].data[j++] = WIDTH_TO_META(1) | ' ';
						sumwidth = width;
					}
				}

				win->lines[i].length = j;
				win->lines[i].width = width - win->lines[i].start;
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
	term_set_cursor(win->y + y, win->x + x);
}

void win_set_paint(Window *win, int y, int x) {
	win->paint_x = x;
	win->paint_y = y;
}

void win_show(Window *win) {
	win->shown = true;
}

void win_hide(Window *win) {
	win->shown = false;
}

/*
static void copy_mb(CharData *dest, const char *src, size_t n, CharData meta) {
	*dest++ = ((unsigned char) *src++) | meta;
	n--;
	while (n > 0)
		*dest++ = (unsigned char) *src++;
}
*/
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

static Bool _win_add_chardata(Window *win, CharData *str, size_t n) {
	int width = 0;
	int extra_spaces = 0;
	int i, j;
	size_t k;
	Bool result = true;
	CharData space = ' ';

	if (win->paint_y >= win->height)
		return true;
	if (win->paint_x > win->width)
		return true;

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
			return true;

		if (!ensureSpace(win->lines + win->paint_y, n))
			return false;

		pos_width = win->lines[win->paint_y].start;

		/* Locate the first character that at least partially overlaps the position
		   where this string is supposed to go. */
		for (i = 0; i < win->lines[win->paint_y].length; i++) {
			pos_width += GET_WIDTH(win->lines[win->paint_y].data[i]);
			if (GET_WIDTH(win->lines[win->paint_y].data[i]) >= win->paint_x)
				break;
		}

		/* Check whether we are being asked to add a zero-width character in the middle
		   of a double-width character. If so, ignore. */
		if (pos_width > win->paint_x)
			return true;

		/* Skip to the next non-zero-width character. */
		if (i < win->lines[win->paint_y].length)
			for (i++; i < win->lines[win->paint_y].length && GET_WIDTH(win->lines[win->paint_y].data[i]) == 0; i++) {}

		memmove(win->lines[win->paint_y].data + i + n, win->lines[win->paint_y].data + i, sizeof(CharData) * (win->lines[win->paint_y].length - i));
		memcpy(win->lines[win->paint_y].data + i, str, n * sizeof(CharData));
		win->lines[win->paint_y].length += n;
	} else if (win->lines[win->paint_y].length == 0) {
		/* Empty line. */
		if (!ensureSpace(win->lines + win->paint_y, n))
			return false;
		win->lines[win->paint_y].start = win->paint_x;
		memcpy(win->lines[win->paint_y].data, str, n * sizeof(CharData));
		win->lines[win->paint_y].length += n;
		win->lines[win->paint_y].width = width;
	} else if (win->lines[win->paint_y].start + win->lines[win->paint_y].width <= win->paint_x) {
		/* Add characters after existing characters. */
		int diff = win->paint_x - (win->lines[win->paint_y].start + win->lines[win->paint_y].width);

		if (!ensureSpace(win->lines + win->paint_y, n + diff))
			return false;
		for (i = diff; i > 0; i--)
			win->lines[win->paint_y].data[win->lines[win->paint_y].length++] =  WIDTH_TO_META(1) | ' ';
		memcpy(win->lines[win->paint_y].data + win->lines[win->paint_y].length, str, n * sizeof(CharData));
		win->lines[win->paint_y].length += n;
		win->lines[win->paint_y].width += width + diff;
	} else if (win->paint_x + width <= win->lines[win->paint_y].start) {
		/* Add characters before existing characters. */
		int diff = win->lines[win->paint_y].start - (win->paint_x + width);

		if (!ensureSpace(win->lines + win->paint_y, n + diff))
			return false;
		memmove(win->lines[win->paint_y].data + n + diff, win->lines[win->paint_y].data, sizeof(CharData) * win->lines[win->paint_y].length);
		memcpy(win->lines[win->paint_y].data, str, n * sizeof(CharData));
		for (i = diff; i > 0; i--)
			win->lines[win->paint_y].data[n++] = WIDTH_TO_META(1) | ' ';
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
			return false;

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

int win_mbaddnstra(Window *win, const char *str, size_t n, CharData attr) {
	size_t result, i;
	int width;
	mbstate_t mbstate;
	wchar_t c[2];
	char buf[MB_LEN_MAX + 1];
	CharData cd_buf[MB_LEN_MAX + 1];
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
		cd_buf[0] = attr | WIDTH_TO_META(width) | (unsigned char) buf[0];
		for (i = 1; i < result; i++)
			cd_buf[i] = (unsigned char) buf[i];

		_win_add_chardata(win, cd_buf, result);
	}
	return retval;
}

int win_mbaddnstr(Window *win, const char *str, size_t n) { return win_mbaddnstra(win, str, n, 0); }
int win_mbaddstra(Window *win, const char *str, CharData attr) { return win_mbaddnstra(win, str, strlen(str), attr); }
int win_mbaddstr(Window *win, const char *str) { return win_mbaddnstra(win, str, strlen(str), 0); }

static Bool _win_addnstra(Window *win, const char *str, size_t n, CharData attr) {
	size_t i;
	Bool result = true;

	/* FIXME: it would seem that this can be done more efficiently, especially
	   if no multibyte characters are used at all. */
	for (i = 0; i < n; i++) {
		CharData c = WIDTH_TO_META(1) | attr | (unsigned char) str[i];
		result &= _win_add_chardata(win, &c, 1);
	}

	return result;
}

int win_addnstra(Window *win, const char *str, size_t n, CharData attr) {
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

int win_addnstr(Window *win, const char *str, size_t n) { return win_addnstra(win, str, n, 0); }
int win_addstra(Window *win, const char *str, CharData attr) { return win_addnstra(win, str, strlen(str), attr); }
int win_addstr(Window *win, const char *str) { return win_addnstra(win, str, strlen(str), 0); }

/* FIXME: assume clear background, such that we erase characters that are "invisible" */
Bool _win_refresh_term_line(struct Window *terminal, LineData *store, int line) {
	LineData save, *draw;
	Window *ptr;

	save = terminal->lines[line];
	terminal->lines[line] = *store;
	terminal->paint_y = line;

	for (ptr = tail; ptr != NULL; ptr = ptr->prev) {
		if (!ptr->shown)
			continue;

		if (ptr->y > line || ptr->y + ptr->height <= line)
			continue;

		draw = ptr->lines + line - ptr->y;
		terminal->paint_x = draw->start + ptr->x;
		_win_add_chardata(terminal, draw->data, draw->length);
	}

	*store = terminal->lines[line];
	terminal->lines[line] = save;
	return true;
}

void win_clrtoeol(Window *win) {
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
