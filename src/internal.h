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
	t3_chardata_t *data; /* Data bytes. */
	int start; /* Offset of data bytes in screen cells from the edge of the t3_window_t. */
	int width; /* Width in cells of the the data. */
	int length; /* Length in t3_chardata_t units of the data. */
	int allocated; /* Allocated number of t3_chardata_t units. */
} LineData;

struct t3_window_t {
	int x, y; /* X and Y coordinates of the t3_window_t. These may be relative to parent, depending on relation. */
	int paint_x, paint_y; /* Drawing cursor */
	int width, height; /* Height and width of the t3_window_t */
	int depth; /* Depth in stack. Higher values are deeper and thus obscured by Windows with lower depth. */
	int relation; /* Relation of this t3_window_t to parent. See window.h for values. */
	t3_chardata_t default_attrs; /* Default attributes to be combined with drawing attributes.
	                           Mostly useful for background specification. */
	t3_bool shown; /* Indicates whether this t3_window_t is visible. */
	LineData *lines; /* The contents of the t3_window_t. */
	t3_window_t *parent; /* t3_window_t for relative placment. */

	/* Pointers for linking into depth sorted list. */
	t3_window_t *next;
	t3_window_t *prev;
};

t3_bool _win_refresh_term_line(struct t3_window_t *terminal, int line);
int _t3_term_get_default_acs(int idx);

#endif
