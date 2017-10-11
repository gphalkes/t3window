/* Copyright (C) 2011-2012 G.P. Halkes
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
#include <stdint.h>

#include "window_api.h"

#define WIDTH_TO_META(_w) (((_w) & 3) << CHAR_BIT)

#define WIDTH_MASK (3 << CHAR_BIT)
#define META_MASK (~((1 << CHAR_BIT) - 1))

#define BASIC_ATTRS (T3_ATTR_UNDERLINE | T3_ATTR_BOLD | T3_ATTR_REVERSE | T3_ATTR_BLINK | T3_ATTR_DIM | T3_ATTR_ACS)

#define INITIAL_ALLOC 80

#define _T3_BLOCK_SIZE_TO_WIDTH(x) ((int)((x & 1) + 1))

typedef struct {
	char *data; /* Data bytes. */
	int start; /* Offset of data bytes in screen cells from the edge of the t3_window_t. */
	int width; /* Width in cells of the the data. */
	int length; /* Length in bytes. */
	int allocated; /* Allocated number of bytes. */
} line_data_t;

struct t3_window_t {
	int x, y; /* X and Y coordinates of the t3_window_t. These may be relative to parent, depending on relation. */
	int previous_x, previous_y; /* Previous X and Y coordinates of the t3_window_t. For tracking movements between calls to t3_term_update. */
	int paint_x, paint_y; /* Drawing cursor */
	int width, height; /* Height and width of the t3_window_t */
	int depth; /* Depth in stack. Higher values are deeper and thus obscured by Windows with lower depth. */
	int relation; /* Relation of this t3_window_t to parent. See window.h for values. */
	int cached_pos_line;
	int cached_pos;
	int cached_pos_width;
	t3_attr_t default_attrs; /* Default attributes to be combined with drawing attributes.
	                           Mostly useful for background specification. */
	t3_bool shown; /* Indicates whether this t3_window_t is visible. */
	line_data_t *lines; /* The contents of the t3_window_t. */
	t3_window_t *parent; /* t3_window_t used for clipping. */
	t3_window_t *anchor; /* t3_window_t for relative placment. */
	t3_window_t *restrictw; /* t3_window_t for restricting the placement of the window. [restrict is seen as keyword by clang :-(]*/

	/* Pointers for linking into depth sorted list. */
	t3_window_t *next;
	t3_window_t *prev;

	/* Pointers for the depth sorted list of child windows. Note that the child windows are not included in the main list. */
	t3_window_t *head;
	t3_window_t *tail;
};

T3_WINDOW_LOCAL t3_bool _t3_win_refresh_term_line(int line);
T3_WINDOW_LOCAL int _t3_term_get_default_acs(int idx);
T3_WINDOW_LOCAL void _t3_remove_window(t3_window_t *win);

T3_WINDOW_LOCAL extern t3_window_t *_t3_terminal_window;
T3_WINDOW_LOCAL extern t3_window_t *_t3_scratch_terminal_window;

enum {
	_T3_TERM_UNKNOWN,
	_T3_TERM_UTF8,
	_T3_TERM_GB18030,
	_T3_TERM_SINGLE_BYTE, /* Generic single byte encoding. Pray that LC_CTYPE has been set correctly. */
	_T3_TERM_CJK, /* One of the CJK encodings has been detected. More detection required. */
	_T3_TERM_CJK_SHIFT_JIS,
	_T3_TERM_GBK
};

enum {
	_T3_MODHACK_NONE,
	_T3_MODHACK_LINUX
};

typedef enum {
	_T3_ACS_AUTO,
	_T3_ACS_ASCII,
	_T3_ACS_UTF8
} t3_acs_override_t;

T3_WINDOW_LOCAL extern int _t3_term_encoding, _t3_term_combining, _t3_term_double_width;
T3_WINDOW_LOCAL extern char _t3_current_charset[80];
T3_WINDOW_LOCAL extern long _t3_detection_needs_finishing;
T3_WINDOW_LOCAL extern int _t3_terminal_in_fd;
T3_WINDOW_LOCAL extern int _t3_terminal_out_fd;
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
T3_WINDOW_LOCAL extern t3_attr_t _t3_attrs, _t3_ansi_attrs,	_t3_reset_required_mask;
T3_WINDOW_LOCAL extern t3_attr_t _t3_ncv;
T3_WINDOW_LOCAL extern t3_bool _t3_bce;
T3_WINDOW_LOCAL extern int _t3_colors, _t3_pairs;
T3_WINDOW_LOCAL extern char _t3_alternate_chars[256];
T3_WINDOW_LOCAL extern t3_bool _t3_show_cursor;
T3_WINDOW_LOCAL extern int _t3_cursor_y, _t3_cursor_x;
T3_WINDOW_LOCAL extern t3_acs_override_t _t3_acs_override;

T3_WINDOW_LOCAL void _t3_do_cup(int line, int col);
T3_WINDOW_LOCAL void _t3_set_alternate_chars_defaults(void);
T3_WINDOW_LOCAL void _t3_set_attrs(t3_attr_t new_attrs);

T3_WINDOW_LOCAL extern t3_window_t *_t3_head, *_t3_tail;
T3_WINDOW_LOCAL t3_bool _t3_win_is_shown(t3_window_t *win);
T3_WINDOW_LOCAL t3_attr_t _t3_term_sanitize_attrs(t3_attr_t attrs);

T3_WINDOW_LOCAL int _t3_map_attr(t3_attr_t attr);
T3_WINDOW_LOCAL t3_attr_t _t3_get_attr(int idx);
T3_WINDOW_LOCAL void _t3_init_attr_map(void);
T3_WINDOW_LOCAL void _t3_free_attr_map(void);

#define _t3_get_value(s, size) (((s)[0] & 0x80) ? _t3_get_value_int(s, size) : (uint32_t) (*(size) = 1, (s)[0]))
T3_WINDOW_LOCAL uint32_t _t3_get_value_int(const char *s, size_t *size);
T3_WINDOW_LOCAL size_t _t3_put_value(uint32_t c, char *dst);
T3_WINDOW_LOCAL int _t3_modifier_hack;

T3_WINDOW_LOCAL void _t3_optimize_terminal(const t3_window_t *current_window, const t3_window_t *new_window);

typedef struct copy_hint_t {
	t3_window_t *win; /* t3_window_t in which the copy took place. */
	int x, y, width, height; /* Description of the region that is copied. */
	int scroll, shift; /* Rows of scroll and columns of shift that were applied. */
	struct copy_hint_t *next; /* Link to the next copy hint. */
} copy_hint_t;

T3_WINDOW_LOCAL copy_hint_t *_t3_copy_hint_head;
#endif
