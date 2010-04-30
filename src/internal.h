#ifndef T3_INTERNAL_H
#define T3_INTERNAL_H

#define WIDTH_TO_META(_w) (((_w) & 3) << CHAR_BIT)

#define WIDTH_MASK (3 << CHAR_BIT)
#define META_MASK (~((1 << CHAR_BIT) - 1))

#define BASIC_ATTRS (T3_ATTR_UNDERLINE | T3_ATTR_BOLD | T3_ATTR_REVERSE | T3_ATTR_BLINK | T3_ATTR_DIM | T3_ATTR_ACS)
#define FG_COLOR_ATTRS (0xf << T3_ATTR_COLOR_SHIFT)
#define BG_COLOR_ATTRS (0xf << (T3_ATTR_COLOR_SHIFT + 4))

#define INITIAL_ALLOC 80

typedef struct {
	T3CharData *data; /* Data bytes. */
	int start; /* Offset of data bytes in screen cells from the edge of the T3Window. */
	int width; /* Width in cells of the the data. */
	int length; /* Length in T3CharData units of the data. */
	int allocated; /* Allocated number of T3CharData units. */
} LineData;

struct T3Window {
	int x, y; /* X and Y coordinates of the T3Window. These may be relative to parent, depending on relation. */
	int paint_x, paint_y; /* Drawing cursor */
	int width, height; /* Height and width of the T3Window */
	int depth; /* Depth in stack. Higher values are deeper and thus obscured by Windows with lower depth. */
	int relation; /* Relation of this T3Window to parent. See window.h for values. */
	T3CharData default_attrs; /* Default attributes to be combined with drawing attributes.
	                           Mostly useful for background specification. */
	T3Bool shown; /* Indicates whether this T3Window is visible. */
	LineData *lines; /* The contents of the T3Window. */
	T3Window *parent; /* T3Window for relative placment. */

	/* Pointers for linking into depth sorted list. */
	T3Window *next;
	T3Window *prev;
};

T3Bool _win_refresh_term_line(struct T3Window *terminal, int line);
int _t3_term_get_default_acs(int idx);

#endif
