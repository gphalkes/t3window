#ifndef INTERNAL_H
#define INTERNAL_H

#define WIDTH_TO_META(_w) (((_w) & 3) << CHAR_BIT)
#define GET_WIDTH(_c) (((_c) >> CHAR_BIT) & 3)

#define WIDTH_MASK (3 << CHAR_BIT)
#define META_MASK (~((1 << CHAR_BIT) - 1))

#define BASIC_ATTRS (ATTR_UNDERLINE | ATTR_BOLD | /* ATTR_STANDOUT |  */ATTR_REVERSE | ATTR_BLINK | ATTR_DIM | ATTR_ACS)
#define FG_COLOR_ATTRS (0xf << _ATTR_COLOR_SHIFT)
#define BG_COLOR_ATTRS (0xf << (_ATTR_COLOR_SHIFT + 4))

#define INITIAL_ALLOC 80

typedef struct {
	CharData *data; /* Data bytes. */
	int start; /* Offset of data bytes in screen cells from the edge of the Window. */
	int width; /* Width in cells of the the data. */
	int length; /* Length in CharData units of the data. */
	int allocated; /* Allocated number of CharData units. */
} LineData;

struct Window {
	int x, y; /* X and Y coordinates of the Window. These may be relative to parent, depending on relation. */
	int paint_x, paint_y; /* Drawing cursor */
	int width, height; /* Height and width of the Window */
	int depth; /* Depth in stack. Higher values are deeper and thus obscured by Windows with lower depth. */
	int relation; /* Relation of this Window to parent. See window.h for values. */
	CharData default_attrs; /* Default attributes to be combined with drawing attributes.
	                           Mostly useful for background specification. */
	Bool shown; /* Indicates whether this Window is visible. */
	LineData *lines; /* The contents of the Window. */
	Window *parent; /* Window for relative placment. */

	/* Pointers for linking into depth sorted list. */
	Window *next;
	Window *prev;
};

Bool _win_refresh_term_line(struct Window *terminal, int line);
int _term_get_default_acs(int idx);

#endif
