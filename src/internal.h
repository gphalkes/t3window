#ifndef T3_INTERNAL_H
#define T3_INTERNAL_H

#include "window_api.h"

/** @typedef t3_chardata_t
    @brief Type to hold data about a single @c char, with attributes used for terminal display.
*/
#if INT_MAX < 2147483647L
typedef long t3_chardata_t;
#else
typedef int t3_chardata_t;
#endif

#define WIDTH_TO_META(_w) (((_w) & 3) << CHAR_BIT)

#define WIDTH_MASK (3 << CHAR_BIT)
#define META_MASK (~((1 << CHAR_BIT) - 1))

#define BASIC_ATTRS (_T3_ATTR_UNDERLINE | _T3_ATTR_BOLD | _T3_ATTR_REVERSE | _T3_ATTR_BLINK | _T3_ATTR_DIM | _T3_ATTR_ACS)

#define INITIAL_ALLOC 80

typedef struct {
	t3_chardata_t *data; /* Data bytes. */
	int start; /* Offset of data bytes in screen cells from the edge of the t3_window_t. */
	int width; /* Width in cells of the the data. */
	int length; /* Length in t3_chardata_t units of the data. */
	int allocated; /* Allocated number of t3_chardata_t units. */
} line_data_t;

struct t3_window_t {
	int x, y; /* X and Y coordinates of the t3_window_t. These may be relative to parent, depending on relation. */
	int paint_x, paint_y; /* Drawing cursor */
	int width, height; /* Height and width of the t3_window_t */
	int depth; /* Depth in stack. Higher values are deeper and thus obscured by Windows with lower depth. */
	int relation; /* Relation of this t3_window_t to parent. See window.h for values. */
	t3_chardata_t default_attrs; /* Default attributes to be combined with drawing attributes.
	                           Mostly useful for background specification. */
	t3_bool shown; /* Indicates whether this t3_window_t is visible. */
	line_data_t *lines; /* The contents of the t3_window_t. */
	t3_window_t *parent; /* t3_window_t for relative placment. */

	/* Pointers for linking into depth sorted list. */
	t3_window_t *next;
	t3_window_t *prev;
};

T3_WINDOW_LOCAL t3_bool _t3_win_refresh_term_line(struct t3_window_t *terminal, int line);
T3_WINDOW_LOCAL t3_chardata_t _t3_term_attr_to_chardata(t3_attr_t attr);
T3_WINDOW_LOCAL int _t3_term_get_default_acs(int idx);
T3_WINDOW_LOCAL t3_attr_t _t3_term_chardata_to_attr(t3_chardata_t chardata);

/** Bit number of the least significant attribute bit.

    By shifting a ::t3_chardata_t value to the right by _T3_ATTR_SHIFT, the attributes
    will be in the least significant bits. This will leave ::T3_ATTR_USER in the
    least significant bit. This allows using the attribute bits as a number instead
    of a bitmask.
*/
#define _T3_ATTR_SHIFT (CHAR_BIT + 2)
/** Bit number of the least significant color attribute bit. */
#define _T3_ATTR_COLOR_SHIFT (_T3_ATTR_SHIFT + 8)
/** Get the width in character cells encoded in a ::t3_chardata_t value. */
#define _T3_CHARDATA_TO_WIDTH(_c) (((_c) >> CHAR_BIT) & 3)

/** Bitmask to leave only the attributes in a ::t3_chardata_t value. */
#define _T3_ATTR_MASK ((t3_chardata_t) (~((1L << _T3_ATTR_SHIFT) - 1)))
/** Bitmask to leave only the character in a ::t3_chardata_t value. */
#define _T3_CHAR_MASK ((t3_chardata_t) ((1L << CHAR_BIT) - 1))
/** Bitmask to leave only the foreground color in a ::t3_chardata_t value. */
#define _T3_ATTR_FG_MASK (0xf << _T3_ATTR_COLOR_SHIFT)
/** Bitmask to leave only the background color in a ::t3_chardata_t value. */
#define _T3_ATTR_BG_MASK (0xf << (_T3_ATTR_COLOR_SHIFT + 4))

/** Use callback for drawing the characters.

    When T3_ATTR_USER is set all other attribute bits are ignored. These can be used by
    the callback to determine the drawing style. The callback is set with ::t3_term_set_callback.
	Note that the callback is responsible for outputing the characters as well (using ::t3_term_putc).
*/
#define _T3_ATTR_USER ((t3_chardata_t) (1L << _T3_ATTR_SHIFT))
/** Draw characters with underlining. */
#define _T3_ATTR_UNDERLINE ((t3_chardata_t) (1L << (_T3_ATTR_SHIFT + 1)))
/** Draw characters with bold face/bright appearance. */
#define _T3_ATTR_BOLD ((t3_chardata_t) (1L << (_T3_ATTR_SHIFT + 2)))
/** Draw characters with reverse video. */
#define _T3_ATTR_REVERSE ((t3_chardata_t) (1L << (_T3_ATTR_SHIFT + 3)))
/** Draw characters blinking. */
#define _T3_ATTR_BLINK ((t3_chardata_t) (1L << (_T3_ATTR_SHIFT + 4)))
/** Draw characters with dim appearance. */
#define _T3_ATTR_DIM ((t3_chardata_t) (1L << (_T3_ATTR_SHIFT + 5)))
/** Draw characters with alternate character set (for line drawing etc). */
#define _T3_ATTR_ACS ((t3_chardata_t) (1L << (_T3_ATTR_SHIFT + 6)))

/** Foreground color unspecified. */
#define _T3_ATTR_FG_UNSPEC ((t3_chardata_t) 0L)
/** Foreground color black. */
#define _T3_ATTR_FG_BLACK ((t3_chardata_t) (1L << T3_ATTR_COLOR_SHIFT))
/** Foreground color red. */
#define _T3_ATTR_FG_RED ((t3_chardata_t) (2L << T3_ATTR_COLOR_SHIFT))
/** Foreground color green. */
#define _T3_ATTR_FG_GREEN ((t3_chardata_t) (3L << T3_ATTR_COLOR_SHIFT))
/** Foreground color yellow. */
#define _T3_ATTR_FG_YELLOW ((t3_chardata_t) (4L << T3_ATTR_COLOR_SHIFT))
/** Foreground color blue. */
#define _T3_ATTR_FG_BLUE ((t3_chardata_t) (5L << T3_ATTR_COLOR_SHIFT))
/** Foreground color magenta. */
#define _T3_ATTR_FG_MAGENTA ((t3_chardata_t) (6L << T3_ATTR_COLOR_SHIFT))
/** Foreground color cyan. */
#define _T3_ATTR_FG_CYAN ((t3_chardata_t) (7L << T3_ATTR_COLOR_SHIFT))
/** Foreground color white. */
#define _T3_ATTR_FG_WHITE ((t3_chardata_t) (8L << T3_ATTR_COLOR_SHIFT))
/** Foreground color default. */
#define _T3_ATTR_FG_DEFAULT ((t3_chardata_t) (9L << T3_ATTR_COLOR_SHIFT))

/** Background color unspecified. */
#define _T3_ATTR_BG_UNSPEC ((t3_chardata_t) 0L)
/** Background color black. */
#define _T3_ATTR_BG_BLACK ((t3_chardata_t) (1L << (T3_ATTR_COLOR_SHIFT + 4)))
/** Background color red. */
#define _T3_ATTR_BG_RED ((t3_chardata_t) (2L << (T3_ATTR_COLOR_SHIFT + 4)))
/** Background color green. */
#define _T3_ATTR_BG_GREEN ((t3_chardata_t) (3L << (T3_ATTR_COLOR_SHIFT + 4)))
/** Background color yellow. */
#define _T3_ATTR_BG_YELLOW ((t3_chardata_t) (4L << (T3_ATTR_COLOR_SHIFT + 4)))
/** Background color blue. */
#define _T3_ATTR_BG_BLUE ((t3_chardata_t) (5L << (T3_ATTR_COLOR_SHIFT + 4)))
/** Background color magenta. */
#define _T3_ATTR_BG_MAGENTA ((t3_chardata_t) (6L << (T3_ATTR_COLOR_SHIFT + 4)))
/** Background color cyan. */
#define _T3_ATTR_BG_CYAN ((t3_chardata_t) (7L << (T3_ATTR_COLOR_SHIFT + 4)))
/** Background color white. */
#define _T3_ATTR_BG_WHITE ((t3_chardata_t) (8L << (T3_ATTR_COLOR_SHIFT + 4)))
/** Background color default. */
#define _T3_ATTR_BG_DEFAULT ((t3_chardata_t) (9L << (T3_ATTR_COLOR_SHIFT + 4)))


#endif
