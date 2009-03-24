#ifndef INTERNAL_H
#define INTERNAL_H

#include <limits.h>
#define WIDTH_TO_META(_w) (((_w) & 3) << CHAR_BIT)
#define GET_WIDTH(_c) (((_c) >> CHAR_BIT) & 3)

#define WIDTH_MASK (3 << CHAR_BIT)
#define META_MASK (~((1 << CHAR_BIT) - 1))

#define BASIC_ATTRS (ATTR_UNDERLINE | ATTR_BOLD | ATTR_STANDOUT | ATTR_REVERSE | ATTR_BLINK | ATTR_DIM | ATTR_ACS)
#define FG_COLOR_ATTRS (0xf << _ATTR_COLOR_SHIFT)
#define BG_COLOR_ATTRS (0xf << (_ATTR_COLOR_SHIFT + 4))

#define INITIAL_ALLOC 80

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
	Bool shown;
	LineData *lines;

	/* Pointers for linking into depth sorted list. */
	Window *next;
	Window *prev;
};

Bool _win_refresh_term_line(struct Window *terminal, LineData *store, int line);

#endif
