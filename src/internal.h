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
#ifndef T3_INTERNAL_H
#define T3_INTERNAL_H

#include <limits.h>
#ifdef HAS_SELECT_H
#include <sys/select.h>
#else
#include <sys/time.h>
#include <sys/types.h>
#endif
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
	int cached_pos_line;
	int cached_pos;
	int cached_pos_width;
	t3_chardata_t default_attrs; /* Default attributes to be combined with drawing attributes.
	                           Mostly useful for background specification. */
	t3_bool shown; /* Indicates whether this t3_window_t is visible. */
	line_data_t *lines; /* The contents of the t3_window_t. */
	t3_window_t *parent; /* t3_window_t used for clipping. */
	t3_window_t *anchor; /* t3_window_t for relative placment. */
	t3_window_t *restrictw; /* t3_window_t for restricting the placement of the window. [restrict is seen as keyword by clang :-(]*/

	/* Pointers for linking into depth sorted list. */
	t3_window_t *next;
	t3_window_t *prev;

	t3_window_t *head;
	t3_window_t *tail;
};

T3_WINDOW_LOCAL t3_bool _t3_win_refresh_term_line(int line);
T3_WINDOW_LOCAL t3_chardata_t _t3_term_attr_to_chardata(t3_attr_t attr);
T3_WINDOW_LOCAL int _t3_term_get_default_acs(int idx);
T3_WINDOW_LOCAL void _t3_remove_window(t3_window_t *win);

T3_WINDOW_LOCAL extern t3_window_t *_t3_terminal_window;

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
/** Draw characters with fallback alternate character set (for line drawing etc). */
#define _T3_ATTR_FALLBACK_ACS ((t3_chardata_t) (1L << (_T3_ATTR_SHIFT + 7)))

/** Foreground color unspecified. */
#define _T3_ATTR_FG_UNSPEC ((t3_chardata_t) 0L)
/** Foreground color black. */
#define _T3_ATTR_FG_BLACK ((t3_chardata_t) (1L << _T3_ATTR_COLOR_SHIFT))
/** Foreground color red. */
#define _T3_ATTR_FG_RED ((t3_chardata_t) (2L << _T3_ATTR_COLOR_SHIFT))
/** Foreground color green. */
#define _T3_ATTR_FG_GREEN ((t3_chardata_t) (3L << _T3_ATTR_COLOR_SHIFT))
/** Foreground color yellow. */
#define _T3_ATTR_FG_YELLOW ((t3_chardata_t) (4L << _T3_ATTR_COLOR_SHIFT))
/** Foreground color blue. */
#define _T3_ATTR_FG_BLUE ((t3_chardata_t) (5L << _T3_ATTR_COLOR_SHIFT))
/** Foreground color magenta. */
#define _T3_ATTR_FG_MAGENTA ((t3_chardata_t) (6L << _T3_ATTR_COLOR_SHIFT))
/** Foreground color cyan. */
#define _T3_ATTR_FG_CYAN ((t3_chardata_t) (7L << _T3_ATTR_COLOR_SHIFT))
/** Foreground color white. */
#define _T3_ATTR_FG_WHITE ((t3_chardata_t) (8L << _T3_ATTR_COLOR_SHIFT))
/** Foreground color default. */
#define _T3_ATTR_FG_DEFAULT ((t3_chardata_t) (9L << _T3_ATTR_COLOR_SHIFT))

/** Background color unspecified. */
#define _T3_ATTR_BG_UNSPEC ((t3_chardata_t) 0L)
/** Background color black. */
#define _T3_ATTR_BG_BLACK ((t3_chardata_t) (1L << (_T3_ATTR_COLOR_SHIFT + 4)))
/** Background color red. */
#define _T3_ATTR_BG_RED ((t3_chardata_t) (2L << (_T3_ATTR_COLOR_SHIFT + 4)))
/** Background color green. */
#define _T3_ATTR_BG_GREEN ((t3_chardata_t) (3L << (_T3_ATTR_COLOR_SHIFT + 4)))
/** Background color yellow. */
#define _T3_ATTR_BG_YELLOW ((t3_chardata_t) (4L << (_T3_ATTR_COLOR_SHIFT + 4)))
/** Background color blue. */
#define _T3_ATTR_BG_BLUE ((t3_chardata_t) (5L << (_T3_ATTR_COLOR_SHIFT + 4)))
/** Background color magenta. */
#define _T3_ATTR_BG_MAGENTA ((t3_chardata_t) (6L << (_T3_ATTR_COLOR_SHIFT + 4)))
/** Background color cyan. */
#define _T3_ATTR_BG_CYAN ((t3_chardata_t) (7L << (_T3_ATTR_COLOR_SHIFT + 4)))
/** Background color white. */
#define _T3_ATTR_BG_WHITE ((t3_chardata_t) (8L << (_T3_ATTR_COLOR_SHIFT + 4)))
/** Background color default. */
#define _T3_ATTR_BG_DEFAULT ((t3_chardata_t) (9L << (_T3_ATTR_COLOR_SHIFT + 4)))

enum {
	_T3_TERM_UNKNOWN,
	_T3_TERM_UTF8,
	_T3_TERM_GB18030,
	_T3_TERM_SINGLE_BYTE, /* Generic single byte encoding. Pray that LC_CTYPE has been set correctly. */
	_T3_TERM_CJK, /* One of the CJK encodings has been detected. More detection required. */
	_T3_TERM_CJK_SHIFT_JIS,
	_T3_TERM_GBK
};

T3_WINDOW_LOCAL extern int _t3_term_encoding, _t3_term_combining, _t3_term_double_width;
T3_WINDOW_LOCAL extern char _t3_current_charset[80];
T3_WINDOW_LOCAL extern long _t3_detection_needs_finishing;
T3_WINDOW_LOCAL extern int _t3_terminal_fd;
T3_WINDOW_LOCAL extern fd_set _t3_inset;

T3_WINDOW_LOCAL extern char *_t3_cup,
	*_t3_sc,
	*_t3_rc,
	*_t3_clear,
	*_t3_home,
	*_t3_vpa,
	*_t3_hpa,
	*_t3_cud,
	*_t3_cud1,
	*_t3_cuf,
	*_t3_cuf1,
	*_t3_civis,
	*_t3_cnorm,
	*_t3_sgr,
	*_t3_setaf,
	*_t3_setab,
	*_t3_op,
	*_t3_smacs,
	*_t3_rmacs,
	*_t3_sgr0,
	*_t3_smul,
	*_t3_rmul,
	*_t3_rev,
	*_t3_bold,
	*_t3_blink,
	*_t3_dim,
	*_t3_setf,
	*_t3_setb,
	*_t3_el,
	*_t3_scp;
T3_WINDOW_LOCAL extern int _t3_lines, _t3_columns;
T3_WINDOW_LOCAL extern const char *_t3_default_alternate_chars[256];
T3_WINDOW_LOCAL extern t3_chardata_t _t3_attrs, _t3_ansi_attrs,	_t3_reset_required_mask;
T3_WINDOW_LOCAL extern t3_chardata_t _t3_ncv;
T3_WINDOW_LOCAL extern t3_bool _t3_bce;
T3_WINDOW_LOCAL extern int _t3_colors, _t3_pairs;
T3_WINDOW_LOCAL extern char _t3_alternate_chars[256];
T3_WINDOW_LOCAL extern line_data_t _t3_old_data;
T3_WINDOW_LOCAL extern t3_bool _t3_show_cursor;
T3_WINDOW_LOCAL extern int _t3_cursor_y, _t3_cursor_x;


T3_WINDOW_LOCAL void _t3_do_cup(int line, int col);
T3_WINDOW_LOCAL void _t3_set_alternate_chars_defaults(void);
T3_WINDOW_LOCAL void _t3_set_attrs(t3_chardata_t new_attrs);

T3_WINDOW_LOCAL extern t3_window_t *_t3_head, *_t3_tail;
T3_WINDOW_LOCAL t3_bool _t3_win_is_shown(t3_window_t *win);
#endif
