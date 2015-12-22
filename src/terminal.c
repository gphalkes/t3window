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
/** @file */

#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#if defined(HAS_WINSIZE_IOCTL) || defined(HAS_SIZE_IOCTL) || defined(HAS_TIOCLINUX)
#include <sys/ioctl.h>
#endif
#ifdef HAS_TIOCLINUX
#include <linux/keyboard.h>
#include <linux/tiocl.h>
#endif
#include <assert.h>
#include <limits.h>

#include "window.h"
#include "internal.h"
#include "convert_output.h"
#include "utf8.h"
/* The curses header file defines too many symbols that get in the way of our
   own, so we have a separate C file which exports only those functions that
   we actually use. */
#include "curses_interface.h"

/*
TODO list:
- Error to localized string conversion routine
- line drawing for UTF-8 may require that we use the UTF-8 line drawing
  characters as apparently the linux console does not do alternate character set
  drawing. On the other hand if it does do proper UTF-8 line drawing there is not
  really a problem anyway. Simply the question of which default to use may be
  of interest.
- do proper cleanup on failure, especially on term_init
- km property indicates that 8th bit *may* be alt. Then smm and rmm may help.
  (it seems the smm and rmm properties are not often filled in though).
- we need to do some checking which header files we need
*/

/** @addtogroup t3window_term */
/** @{ */

/** @internal
    @brief Add a separator for creating ANSI strings in ::_t3_set_attrs. */
#define ADD_ANSI_SEP() do { strcat(mode_string, sep); sep = ";"; } while(0)

/** @internal
    @brief Swap two line_data_t structures. Used in ::t3_term_update. */
#define SWAP_LINES(a, b) do { line_data_t save; save = (a); (a) = (b); (b) = save; } while (0)

char *_t3_smcup, /**< @internal Terminal control string: start cursor positioning mode. */
	*_t3_rmcup, /**< @internal Terminal control string: stop cursor positioning mode. */
	*_t3_cup, /**< @internal Terminal control string: position cursor. */
	*_t3_sc, /**< @internal Terminal control string: save cursor position. */
	*_t3_rc, /**< @internal Terminal control string: restore cursor position. */
	*_t3_clear, /**< @internal Terminal control string: clear terminal. */
	*_t3_home, /**< @internal Terminal control string: cursor to home position. */
	*_t3_vpa, /**< @internal Terminal control string: set vertical cursor position. */
	*_t3_hpa, /**< @internal Terminal control string: set horizontal cursor position. */
	*_t3_cud, /**< @internal Terminal control string: move cursor up. */
	*_t3_cud1, /**< @internal Terminal control string: move cursor up 1 line. */
	*_t3_cuf, /**< @internal Terminal control string: move cursor forward. */
	*_t3_cuf1, /**< @internal Terminal control string: move cursor forward one position. */
	*_t3_civis, /**< @internal Terminal control string: hide cursor. */
	*_t3_cnorm, /**< @internal Terminal control string: show cursor. */
	*_t3_sgr, /**< @internal Terminal control string: set graphics rendition. */
	*_t3_setaf, /**< @internal Terminal control string: set foreground color (ANSI). */
	*_t3_setab, /**< @internal Terminal control string: set background color (ANSI). */
	*_t3_op, /**< @internal Terminal control string: reset colors. */
	*_t3_smacs, /**< @internal Terminal control string: start alternate character set mode. */
	*_t3_rmacs, /**< @internal Terminal control string: stop alternate character set mode. */
	*_t3_sgr0, /**< @internal Terminal control string: reset graphics rendition. */
	*_t3_smul, /**< @internal Terminal control string: start underline mode. */
	*_t3_rmul, /**< @internal Terminal control string: stop underline mode. */
	*_t3_rev, /**< @internal Terminal control string: start reverse video. */
	*_t3_bold, /**< @internal Terminal control string: start bold. */
	*_t3_blink, /**< @internal Terminal control string: start blink. */
	*_t3_dim, /**< @internal Terminal control string: start dim. */
	*_t3_setf, /**< @internal Terminal control string: set foreground color. */
	*_t3_setb, /**< @internal Terminal control string: set background color. */
	*_t3_el, /**< @internal Terminal control string: clear to end of line. */
	*_t3_scp; /**< @internal Terminal control string: set color pair. */
t3_attr_t _t3_ncv; /**< @internal Terminal info: Non-color video attributes (encoded in t3_attr_t). */
t3_bool _t3_bce; /**< @internal Terminal info: screen erased with background color. */
int _t3_colors, /**< @internal Terminal info: number of colors supported. */
	_t3_pairs; /**< @internal Terminal info: number of color pairs supported. */

t3_window_t *_t3_terminal_window; /**< @internal t3_window_t struct representing the last drawn terminal state. */
t3_window_t *_t3_scratch_terminal_window; /**< @internal t3_window_t struct used in the terminal update. */

int _t3_lines, /**< @internal Size of terminal (lines). */
	_t3_columns; /**< @internal Size of terminal (columns). */
int _t3_cursor_y, /**< @internal Cursor position (y coordinate). */
	_t3_cursor_x; /**< @internal Cursor position (x coordinate). */
static int new_cursor_y, /**< New cursor position (y coordinate). */
	new_cursor_x; /**< New cursor position (x coordinate). */
t3_bool _t3_show_cursor = t3_true; /**< @internal Boolean indicating whether the cursor is visible currently. */
static t3_bool new_show_cursor = t3_true; /**< Boolean indicating whether the cursor is will be visible after the next update. */

/** Conversion table between color attributes and non-ANSI colors. */
static int attr_to_alt_color[8] = { 0, 4, 2, 6, 1, 5, 3, 7 };
t3_attr_t _t3_attrs = 0, /**< @internal Last used set of attributes. */
	_t3_ansi_attrs = 0, /**< @internal Bit mask indicating which attributes should be drawn as ANSI colors. */
	/** @internal Attributes for which the only way to turn of the attribute is to reset all attributes. */
	_t3_reset_required_mask = T3_ATTR_BOLD | T3_ATTR_REVERSE | T3_ATTR_BLINK | T3_ATTR_DIM;
/** Callback for T3_ATTR_USER. */
static t3_attr_user_callback_t user_callback = NULL;

/** @internal Alternate character set conversion table from TERM_* values to terminal ACS characters. */
char _t3_alternate_chars[256];

/** @internal Alternate character set fall-back characters for when the terminal does not
    provide a proper ACS character. */
const char *_t3_default_alternate_chars[256];

/** @internal File descriptor of the terminal for input. */
int _t3_terminal_in_fd;
/** @internal File descriptor of the terminal for output. */
int _t3_terminal_out_fd;

/** @internal Boolean indicating whether the terminal capbilities detection requires finishing.

    This variable is the only variable on which a race-condition can occur
	(provided that only the ::t3_term_get_keychar function is called from a
	separate thread). The @c _t3_term_(encoding|combining|double_width) values
    are also accessed from two different threads, but only written from one. And
    the updates to those are such that using an old value temporarily is not a
    problem.

    Note that to prevent interference with adjecent variables, we use a @c long,
    instead of a ::t3_bool.
*/
long _t3_detection_needs_finishing;

int _t3_term_encoding = _T3_TERM_UNKNOWN, /**< @internal Detected terminal encoding/mode. */
	_t3_term_combining = -1, /**< @internal Terminal combining capabilities. */
	_t3_term_double_width = -1; /**< @internal Terminal double width character support level. */

/** @internal Buffer holding the current character set.

    The current character set may either have been retrieved from libtranscript,
    or it was detected by the terminal capabilities detection code.
*/
char _t3_current_charset[80];

/** @internal Variable indicating if and if so how the ACS should be overriden. */
t3_acs_override_t _t3_acs_override;

/** @internal Variable indicating what hack to obtain modifiers should be used, if any. */
int _t3_modifier_hack;

/** Get fall-back character for alternate character set character (internal use only).
    @param idx The character to retrieve the fall-back character for.
    @return The fall-back character.
*/
static const char *get_default_acs(int idx) {
	static const char *acs_ascii_defaults[128] = {
		" ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ",
		" ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ",
		" ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", ">", "<", "^", "v", " ",
		"#", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ",
		" ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ",
		" ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ",
		"+", ":", " ", " ", " ", " ", "\\", "#", "#", "#", "+", "+", "+", "+", "+", "~",
		"-", "-", "-", "_", "+", "+", "+", "+", "|", "<", ">", "*", "!", "f", "o", " "
	};
	const char *retval;

	if (idx < 0 || idx > 127)
		return " ";
	switch (_t3_acs_override) {
		default:
		case _T3_ACS_AUTO:
		case _T3_ACS_UTF8:
			retval = _t3_default_alternate_chars[idx];
			break;
		case _T3_ACS_ASCII:
			retval = acs_ascii_defaults[idx];
			break;
	}

	return retval != NULL ? retval : acs_ascii_defaults[idx];
}

/** Move cursor to screen position.
    @param line The screen line to move the cursor to.
    @param col The screen column to move the cursor to.

	This function uses the @c _t3_cup terminfo string if available, and emulates
    it through other means if necessary.
*/
void _t3_do_cup(int line, int col) {
	if (_t3_cup != NULL) {
		_t3_putp(_t3_tparm(_t3_cup, 2, line, col));
		return;
	}
	if (_t3_vpa != NULL) {
		_t3_putp(_t3_tparm(_t3_vpa, 1, line));
		_t3_putp(_t3_tparm(_t3_hpa, 1, col));
		return;
	}
	if (_t3_home != NULL) {
		int i;

		_t3_putp(_t3_home);
		if (line > 0) {
			if (_t3_cud != NULL) {
				_t3_putp(_t3_tparm(_t3_cud, 1, line));
			} else {
				for (i = 0; i < line; i++)
					_t3_putp(_t3_cud1);
			}
		}
		if (col > 0) {
			if (_t3_cuf != NULL) {
				_t3_putp(_t3_tparm(_t3_cuf, 1, col));
			} else {
				for (i = 0; i < col; i++)
					_t3_putp(_t3_cuf1);
			}
		}
	}
}

/** Get the string describing the current character set used by the library.

    The reason this function is provided, is that although the library initially
    will use the result of @c transcript_get_codeset, the terminal-capabilities
    detection may result in a different character set being used. After
    receiving ::T3_WARN_UPDATE_TERMINAL from ::t3_term_get_keychar, this routine
    may return a different value.
*/
const char *t3_term_get_codeset(void) {
	return _t3_current_charset;
}

/** Move cursor.
    @param y The terminal line to move the cursor to.
    @param x The terminal column to move the cursor to.

    If the cursor is invisible the new position is recorded, but not actually moved yet.
    Moving the cursor takes effect immediately.
*/
void t3_term_set_cursor(int y, int x) {
	new_cursor_y = y;
	new_cursor_x = x;
}

/** Hide the cursor.

    Instructs the terminal to make the cursor invisible. If the terminal does not provide
    the required functionality, the cursor is moved to the bottom right.
*/
void t3_term_hide_cursor(void) {
	new_show_cursor = t3_false;
}

/** Show the cursor.

    Instructs the terminal to make the cursor visible.
*/
void t3_term_show_cursor(void) {
	new_show_cursor = t3_true;
}

/** Retrieve the terminal size.
    @param height The location to store the terminal height in lines.
    @param width The location to store the terminal height in columns.

    Neither @p height nor @p width may be @c NULL.
*/
void t3_term_get_size(int *height, int *width) {
	if (height != NULL)
		*height = _t3_lines;
	if (width != NULL)
		*width = _t3_columns;
}

/** Handle resizing of the terminal.
    @return A boolean indicating success of the resizing operation, which depends on
		memory allocation success.

    Should be called after a @c SIGWINCH. Retrieves the size of the terminal and
    resizes the backing structures. After calling ::t3_term_resize,
    ::t3_term_get_size can be called to retrieve the new terminal size.
*/
t3_bool t3_term_resize(void) {
#ifdef HAS_WINSIZE_IOCTL
	struct winsize wsz;

	if (ioctl(_t3_terminal_out_fd, TIOCGWINSZ, &wsz) < 0)
		return t3_true;

	_t3_lines = wsz.ws_row;
	_t3_columns = wsz.ws_col;

	if (_t3_columns == _t3_terminal_window->width && _t3_lines == _t3_terminal_window->height)
		return t3_true;

	if (_t3_columns < _t3_terminal_window->width || _t3_lines != _t3_terminal_window->height) {
		/* Clear the cache of the terminal contents and the actual terminal. This
		   is necessary because shrinking the terminal tends to cause all kinds of
		   weird corruption of the on screen state. */
		t3_term_redraw();
	}

	return t3_win_resize(_t3_terminal_window, _t3_lines, _t3_columns) && t3_win_resize(_t3_scratch_terminal_window, _t3_lines, _t3_columns);
#else
	return t3_true;
#endif
}

/** Set the non-ANSI terminal drawing attributes.
    @param new_attrs The new attributes that should be used for subsequent character display.

    This function updates ::t3_attrs to reflect any changes made to the terminal attributes.
    Note that this function may reset all attributes if an attribute was previously set for
    which no independent reset is available.
*/
static void set_attrs_non_ansi(t3_attr_t new_attrs) {
	t3_attr_t attrs_basic_non_ansi = _t3_attrs & BASIC_ATTRS & ~_t3_ansi_attrs;
	t3_attr_t new_attrs_basic_non_ansi = new_attrs & BASIC_ATTRS & ~_t3_ansi_attrs;
	int color_nr;

	if (attrs_basic_non_ansi != new_attrs_basic_non_ansi) {
		t3_attr_t changed;
		if (attrs_basic_non_ansi & ~new_attrs & _t3_reset_required_mask) {
			if (_t3_sgr != NULL) {
				_t3_putp(_t3_tparm(_t3_sgr, 9,
					0,
					new_attrs & T3_ATTR_UNDERLINE,
					new_attrs & T3_ATTR_REVERSE,
					new_attrs & T3_ATTR_BLINK,
					new_attrs & T3_ATTR_DIM,
					new_attrs & T3_ATTR_BOLD,
					0,
					0,
					new_attrs & T3_ATTR_ACS));
				/* sgr tends to reset the colors as well. To ensure that we set
				   the colors regardless of what sgr does, we force the colors in
				   _t3_attrs to the DEFAULT, which can never be actually set. */
				_t3_attrs = (new_attrs & ~(T3_ATTR_FG_MASK | T3_ATTR_BG_MASK)) | T3_ATTR_FG_DEFAULT | T3_ATTR_BG_DEFAULT;
				attrs_basic_non_ansi = _t3_attrs & ~_t3_ansi_attrs;
			} else {
				/* Note that this will not be NULL if it is required because of
				   tests in the initialization. */
				_t3_putp(_t3_sgr0);
				attrs_basic_non_ansi = _t3_attrs = 0;
			}
		}

		/* Set any attributes required. If sgr was previously used, the calculation
		   of 'changed' results in 0. */
		changed = attrs_basic_non_ansi ^ new_attrs_basic_non_ansi;
		if (changed) {
			if (changed & T3_ATTR_UNDERLINE)
				_t3_putp(new_attrs & T3_ATTR_UNDERLINE ? _t3_smul : _t3_rmul);
			if (changed & T3_ATTR_REVERSE)
				_t3_putp(_t3_rev);
			if (changed & T3_ATTR_BLINK)
				_t3_putp(_t3_blink);
			if (changed & T3_ATTR_DIM)
				_t3_putp(_t3_dim);
			if (changed & T3_ATTR_BOLD)
				_t3_putp(_t3_bold);
			if (changed & T3_ATTR_ACS)
				_t3_putp(new_attrs & T3_ATTR_ACS ? _t3_smacs : _t3_rmacs);
		}
	}

	/* If colors are set using ANSI sequences, we are done here. */
	if ((~_t3_ansi_attrs & (T3_ATTR_FG_MASK | T3_ATTR_BG_MASK)) == 0)
		return;

	/* Specifying DEFAULT as color is the same as not specifying anything. However,
	   for ::t3_term_combine_attrs there is a distinction between an explicit and an
	   implicit color. Here we don't care about that distinction so we remove it. */
	if ((new_attrs & T3_ATTR_FG_MASK) == T3_ATTR_FG_DEFAULT)
		new_attrs &= ~(T3_ATTR_FG_MASK);
	if ((new_attrs & T3_ATTR_BG_MASK) == T3_ATTR_BG_DEFAULT)
		new_attrs &= ~(T3_ATTR_BG_MASK);

	if (_t3_scp == NULL) {
		/* Set default color through op string */
		if (((_t3_attrs & T3_ATTR_FG_MASK) != (new_attrs & T3_ATTR_FG_MASK) && (new_attrs & T3_ATTR_FG_MASK) == 0) ||
				((_t3_attrs & T3_ATTR_BG_MASK) != (new_attrs & T3_ATTR_BG_MASK) && (new_attrs & T3_ATTR_BG_MASK) == 0)) {
			if (_t3_op != NULL) {
				_t3_putp(_t3_op);
				_t3_attrs &= ~(T3_ATTR_FG_MASK | T3_ATTR_BG_MASK);
			}
		}

		if ((_t3_attrs & T3_ATTR_FG_MASK) != (new_attrs & T3_ATTR_FG_MASK) && (new_attrs & T3_ATTR_FG_MASK) != 0) {
			color_nr = ((new_attrs & T3_ATTR_FG_MASK) >> T3_ATTR_COLOR_SHIFT) - 1;
			if (_t3_setaf != NULL)
				_t3_putp(_t3_tparm(_t3_setaf, 1, color_nr));
			else if (_t3_setf != NULL)
				_t3_putp(_t3_tparm(_t3_setf, 1, color_nr < 8 ? attr_to_alt_color[color_nr] : color_nr));
		}

		if ((_t3_attrs & T3_ATTR_BG_MASK) != (new_attrs & T3_ATTR_BG_MASK) && (new_attrs & T3_ATTR_BG_MASK) != 0) {
			color_nr = ((new_attrs & T3_ATTR_BG_MASK) >> (T3_ATTR_COLOR_SHIFT + 9)) - 1;
			if (_t3_setab != NULL)
				_t3_putp(_t3_tparm(_t3_setab, 1, color_nr));
			else if (_t3_setb != NULL)
				_t3_putp(_t3_tparm(_t3_setb, 1, color_nr < 8 ? attr_to_alt_color[color_nr] : color_nr ));
		}
	} else {
		color_nr = (new_attrs & T3_ATTR_FG_MASK) >> T3_ATTR_COLOR_SHIFT;
		if (color_nr == 0)
			_t3_putp(_t3_op);
		else
			_t3_putp(_t3_tparm(_t3_scp, 1, color_nr - 1));
	}
}

/** @internal
    @brief Set terminal drawing attributes.
    @param new_attrs The new attributes that should be used for subsequent character display.

    The state of ::_t3_attrs is updated to reflect the new state.
*/
void _t3_set_attrs(t3_attr_t new_attrs) {
	char mode_string[30]; /* Max is (if I counted correctly) 24. Use 30 for if I miscounted. */
	t3_attr_t changed_attrs;
	const char *sep = "[";

	/* Flush any characters accumulated in the output buffer before switching attributes. */
	_t3_output_buffer_print();

	/* Just in case the caller forgot */
	new_attrs &= ~T3_ATTR_FALLBACK_ACS;

	if (new_attrs == 0) {
		if (_t3_attrs == 0)
			return;
		if (_t3_sgr0 != NULL || _t3_sgr != NULL) {
			/* Use sgr instead of sgr0 as this is probably more tested (see rxvt-unicode terminfo bug) */
			if (_t3_sgr != NULL)
				_t3_putp(_t3_tparm(_t3_sgr, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0));
			else
				_t3_putp(_t3_sgr0);
			_t3_attrs = 0;
			return;
		}
	}

	changed_attrs = (new_attrs ^ _t3_attrs) & ~_t3_ansi_attrs;
	if (changed_attrs != 0)
		set_attrs_non_ansi(new_attrs);

	changed_attrs = (new_attrs ^ _t3_attrs) & _t3_ansi_attrs;
	if (changed_attrs == 0) {
		_t3_attrs = new_attrs;
		return;
	}

	mode_string[0] = '\033';
	mode_string[1] = 0;

	if (changed_attrs & T3_ATTR_UNDERLINE) {
		ADD_ANSI_SEP();
		strcat(mode_string, new_attrs & T3_ATTR_UNDERLINE ? "4" : "24");
	}

	if (changed_attrs & (T3_ATTR_BOLD | T3_ATTR_DIM)) {
		ADD_ANSI_SEP();
		strcat(mode_string, new_attrs & T3_ATTR_BOLD ? "1" : (new_attrs & T3_ATTR_DIM ? "2" : "22"));
	}

	if (changed_attrs & T3_ATTR_REVERSE) {
		ADD_ANSI_SEP();
		strcat(mode_string, new_attrs & T3_ATTR_REVERSE ? "7" : "27");
	}

	if (changed_attrs & T3_ATTR_BLINK) {
		ADD_ANSI_SEP();
		strcat(mode_string, new_attrs & T3_ATTR_BLINK ? "5" : "25");
	}

	if (changed_attrs & T3_ATTR_ACS) {
		ADD_ANSI_SEP();
		strcat(mode_string, new_attrs & T3_ATTR_ACS ? "11" : "10");
	}

	if (changed_attrs & T3_ATTR_FG_MASK) {
		char color[9];
		int color_nr = ((new_attrs & T3_ATTR_FG_MASK) >> T3_ATTR_COLOR_SHIFT) - 1;
		if (color_nr < 8 || color_nr == 256) {
			color[0] = '3';
			color[1] = '0' + (color_nr >= 0 && color_nr < 8 ? color_nr : 9);
			color[2] = 0;
		} else if (color_nr < 16) {
			sprintf(color, "9%d", color_nr - 8);
		} else {
			sprintf(color, "38;5;%d", color_nr);
		}
		ADD_ANSI_SEP();
		strcat(mode_string, color);
	}

	if (changed_attrs & T3_ATTR_BG_MASK) {
		char color[9];
		int color_nr = ((new_attrs & T3_ATTR_BG_MASK) >> (T3_ATTR_COLOR_SHIFT + 9)) - 1;
		if (color_nr < 8 || color_nr == 256) {
			color[0] = '4';
			color[1] = '0' + (color_nr >= 0 && color_nr < 8 ? color_nr : 9);
			color[2] = 0;
		} else if (color_nr < 16) {
			sprintf(color, "10%d", color_nr - 8);
		} else {
			sprintf(color, "48;5;%d", color_nr);
		}
		ADD_ANSI_SEP();
		strcat(mode_string, color);
	}
	strcat(mode_string, "m");
	_t3_putp(mode_string);
	_t3_attrs = new_attrs;
}

/** Set terminal drawing attributes.
    @param new_attrs The new attributes that should be used for subsequent character display.
*/
void t3_term_set_attrs(t3_attr_t new_attrs) {
	_t3_set_attrs(new_attrs);
}

/** Set callback for drawing characters with ::T3_ATTR_USER attribute.
    @param callback The function to call for drawing.
*/
void t3_term_set_user_callback(t3_attr_user_callback_t callback) {
	user_callback = callback;
}

/** Update the cursor, not drawing anything. */
void t3_term_update_cursor(void) {
	/* Only move the cursor if it is to be shown after the update. */
	if (new_show_cursor != _t3_show_cursor) {
		_t3_show_cursor = new_show_cursor;
		if (_t3_show_cursor) {
			_t3_do_cup(new_cursor_y, new_cursor_x);
			_t3_cursor_y = new_cursor_y;
			_t3_cursor_x = new_cursor_x;
			_t3_putp(_t3_cnorm);
		} else {
			_t3_putp(_t3_civis);
		}
	} else {
		if (new_cursor_y != _t3_cursor_y || new_cursor_x != _t3_cursor_x) {
			_t3_do_cup(new_cursor_y, new_cursor_x);
			_t3_cursor_y = new_cursor_y;
			_t3_cursor_x = new_cursor_x;
		}
	}
	fflush(_t3_putp_file);
}

/** Update the terminal, drawing all changes since last refresh.

    After changing window contents, this function should be called to make those
    changes visible on the terminal. The refresh is not done automatically to allow
    programs to bunch many separate updates. Generally this is called right before
    ::t3_term_get_keychar.
*/
void t3_term_update(void) {
	int i;

	if (_t3_detection_needs_finishing) {
		_t3_init_output_converter(_t3_current_charset);
		_t3_set_alternate_chars_defaults();
		t3_term_redraw();
		_t3_detection_needs_finishing = t3_false;
	}

	if (_t3_civis != NULL) {
		if (new_show_cursor != _t3_show_cursor) {
			/* If the cursor should now be invisible, hide it before drawing. If the
			   cursor should now be visible, leave it invisible until after drawing. */
			if (!new_show_cursor)
				_t3_putp(_t3_civis);
		} else if (_t3_show_cursor) {
			if (new_cursor_y == _t3_cursor_y && new_cursor_x == _t3_cursor_x)
				_t3_putp(_t3_sc);
			_t3_putp(_t3_civis);
		}
	}

	for (i = 0; i < _t3_lines; i++) {
		SWAP_LINES(_t3_scratch_terminal_window->lines[i], _t3_terminal_window->lines[i]);
		_t3_win_refresh_term_line(i);
	}

	for (i = 0; i < _t3_lines; i++) {
		int old_idx = 0, new_idx = 0, width, old_width, last_width = -1;
		uint32_t old_block_size, new_block_size;
		size_t old_block_size_bytes, new_block_size_bytes;
		line_data_t *old_data = &_t3_scratch_terminal_window->lines[i];
		line_data_t *new_data = &_t3_terminal_window->lines[i];

		width = new_data->start;
		old_width = old_data->start;

		if (width > old_width && old_data->width > 0) {
			int spaces;
			_t3_do_cup(i, old_data->start);
			_t3_set_attrs(0);

			if (old_data->start + old_data->width < width) {
				spaces = old_data->width;
				old_idx = old_data->length;
				old_width = old_data->start + old_data->width;
				last_width = old_width;
			} else {
				spaces = new_data->start - old_data->start;
				while (old_idx < old_data->length) {
					old_block_size = _t3_get_value(old_data->data + old_idx, &old_block_size_bytes);
					if (old_width + _T3_BLOCK_SIZE_TO_WIDTH(old_block_size) > width)
						break;
					old_width += _T3_BLOCK_SIZE_TO_WIDTH(old_block_size);
					old_idx += (old_block_size >> 1) + old_block_size_bytes;
				}
				last_width = width;
			}

			for (spaces = new_data->start - old_data->start; spaces > 0; spaces--)
				t3_term_putc(' ');
		}

		while (new_idx != new_data->length) {
			int saved_old_idx, saved_new_idx, saved_width, same_count = 0;

			/* Only check if old and new are the same if we are checking the same position. */
			if (old_width == width) {
				saved_old_idx = old_idx;
				saved_new_idx = new_idx;
				saved_width = width;

				while (new_idx < new_data->length && old_idx < old_data->length) {
					old_block_size = _t3_get_value(old_data->data + old_idx, &old_block_size_bytes);
					new_block_size = _t3_get_value(new_data->data + new_idx, &new_block_size_bytes);

					/* Check if the next blocks are equal. If not, break. */
					if (old_block_size != new_block_size || memcmp(old_data->data + old_idx + old_block_size_bytes,
							new_data->data + new_idx + new_block_size_bytes, old_block_size >> 1) != 0)
						break;
					same_count++;
					width += _T3_BLOCK_SIZE_TO_WIDTH(old_block_size);
					old_width = width;
					old_idx += (old_block_size >> 1) + old_block_size_bytes;
					new_idx += (new_block_size >> 1) + new_block_size_bytes;
				}

				if (new_idx >= new_data->length)
					break;

				if (same_count < 3 && old_idx < old_data->length) {
					old_idx = saved_old_idx;
					new_idx = saved_new_idx;
					old_width = width = saved_width;
					same_count++;
				} else {
					/* Erase same_count, so we don't print unnecessary characters below. */
					same_count = 0;
				}
			}

			if (width != last_width) {
				if (last_width < 0 || _t3_hpa == NULL)
					_t3_do_cup(i, width);
				else
					_t3_putp(_t3_tparm(_t3_hpa, 1, width));
			}

			do {
				t3_attr_t new_attrs;
				size_t new_attrs_bytes;

				new_block_size = _t3_get_value(new_data->data + new_idx, &new_block_size_bytes);
				new_idx += new_block_size_bytes;
				new_attrs = _t3_get_attr(_t3_get_value(new_data->data + new_idx, &new_attrs_bytes));

				if ((new_attrs & T3_ATTR_USER) && user_callback != NULL) {
					user_callback(new_data->data + new_idx + new_attrs_bytes, (new_block_size >> 1) - new_attrs_bytes,
						_T3_BLOCK_SIZE_TO_WIDTH(new_block_size), new_attrs);
				} else {
					if (new_attrs & T3_ATTR_ACS) {
						if (!t3_term_acs_available(new_data->data[new_idx + new_attrs_bytes])) {
							new_attrs &= ~T3_ATTR_ACS;
							if (new_attrs != _t3_attrs)
								_t3_set_attrs(new_attrs);
							t3_term_puts(get_default_acs(new_data->data[new_idx + new_attrs_bytes]));
						} else {
							if (new_attrs != _t3_attrs)
								_t3_set_attrs(new_attrs);
							/* ACS characters should be passed directly to the terminal, without
							   character-set conversion. */
							_t3_output_buffer_print();
							fwrite(new_data->data + new_idx + new_attrs_bytes, 1,
								(new_block_size >> 1) - new_attrs_bytes, _t3_putp_file);
						}
					} else {
						if (new_attrs != _t3_attrs)
							_t3_set_attrs(new_attrs);
						t3_term_putn(new_data->data + new_idx + new_attrs_bytes,
							(new_block_size >> 1) - new_attrs_bytes);
					}
				}
				new_idx += new_block_size >> 1;
				width += _T3_BLOCK_SIZE_TO_WIDTH(new_block_size);
				same_count--;

				while (old_idx < old_data->length) {
					old_block_size = _t3_get_value(old_data->data + old_idx, &old_block_size_bytes);
					if (old_width + _T3_BLOCK_SIZE_TO_WIDTH(old_block_size) > width)
						break;
					old_width += _T3_BLOCK_SIZE_TO_WIDTH(old_block_size);
					old_idx += (old_block_size >> 1) + old_block_size_bytes;
				}
			} while ((old_width != width || same_count > 0) && new_idx < new_data->length);
			last_width = width;
			_t3_output_buffer_print();
		}

		/* Clear the terminal line if the new line is shorter than the old one. */
		if (new_data->start + new_data->width < old_data->start + old_data->width &&
				width < _t3_terminal_window->width)
		{
			if (last_width < 0)
				_t3_do_cup(i, 0);

			if (_t3_bce && (_t3_attrs & ~T3_ATTR_FG_MASK) != 0)
				_t3_set_attrs(0);

			if (_t3_el != NULL) {
				_t3_putp(_t3_el);
			} else {
				int max = old_data->start + old_data->width;
				for (; width < max; width++)
					t3_term_putc(' ');
			}
		}
		_t3_output_buffer_print();
	}

	/* _t3_set_attrs(0); */

	if (_t3_civis == NULL) {
		_t3_show_cursor = new_show_cursor;
		if (!_t3_show_cursor)
			_t3_do_cup(_t3_terminal_window->height, _t3_terminal_window->width);
	} else {
		if (new_show_cursor != _t3_show_cursor) {
			/* If the cursor should now be visible, move it to the right position and
			   show it. Otherwise, it was already hidden at the start of this routine. */
			if (new_show_cursor) {
				_t3_do_cup(new_cursor_y, new_cursor_x);
				_t3_cursor_y = new_cursor_y;
				_t3_cursor_x = new_cursor_x;
				_t3_putp(_t3_cnorm);
			}
			_t3_show_cursor = new_show_cursor;
		} else if (_t3_show_cursor) {
			if (new_cursor_y == _t3_cursor_y && new_cursor_x == _t3_cursor_x && _t3_rc != NULL)
				_t3_putp(_t3_rc);
			else
				_t3_do_cup(new_cursor_y, new_cursor_x);
			_t3_cursor_y = new_cursor_y;
			_t3_cursor_x = new_cursor_x;
			_t3_putp(_t3_cnorm);
		}
	}

	fflush(_t3_putp_file);
}

/** Redraw the entire terminal from scratch. */
void t3_term_redraw(void) {
	/* The clear action destroys the current cursor position, so we make sure
	   that it has to be repositioned afterwards. Because we are redrawing, we
	   definately also want to ensure that the cursor is in the right place. */
	if (new_show_cursor && _t3_show_cursor)
		_t3_cursor_x = new_cursor_x + 1;
	_t3_set_attrs(0);
	_t3_putp(_t3_clear);
	t3_win_set_paint(_t3_terminal_window, 0, 0);
	t3_win_clrtobot(_t3_terminal_window);
}

/** Send a terminal control string to the terminal, with correct padding.

    @note This function should only be called in very special circumstances in
    a registered user callback (see ::t3_term_set_user_callback), when
    you want to do something which the library can not. Make sure that any state
    changes are undone before returning from the callback.
*/
void t3_term_putp(const char *str) {
	_t3_output_buffer_print();
	_t3_putp(str);
}

/** Calculate the cell width of a string.
    @param str The string to calculate the width of.
    @return The width of the string in character cells.

    Using @c strlen on a string will not give one the correct width of a UTF-8 string
    on the terminal screen. This function is provided to calculate that value.
*/
int t3_term_strwidth(const char *str) {
	size_t bytes_read, n = strlen(str);
	int width, retval = 0;
	uint32_t c;

	for (; n > 0; n -= bytes_read, str += bytes_read) {
		bytes_read = n;
		c = t3_utf8_get(str, &bytes_read);

		width = t3_utf8_wcwidth(c);
		if (width < 0)
			continue;
		retval += width;
	}
	return retval;
}

/** Check if a character is available in the alternate character set (internal use mostly).
    @param idx The character to check.
    @return ::t3_true if the character is available in the alternate character set.

    Characters for which an alternate character is generally available are documented in
    terminfo(5), but most are encoded in T3_ACS_* constants.
*/
t3_bool t3_term_acs_available(int idx) {
	if (idx < 0 || idx > 127)
		return t3_false;
	return _t3_alternate_chars[idx] != 0 && _t3_acs_override == _T3_ACS_AUTO;
}

/** Combine attributes, with priority.
    @param a The first set of attributes to combine (priority).
    @param b The second set of attributes to combine (no priority).
    @return The combined attributes.

    This function combines @p a and @p b, with the color attributes from @p a overriding
	the color attributes from @p b if both specify colors.
*/
t3_attr_t t3_term_combine_attrs(t3_attr_t a, t3_attr_t b) {
	t3_attr_t result = b | (a & ~(T3_ATTR_FG_MASK | T3_ATTR_BG_MASK));
	if ((a & T3_ATTR_FG_MASK) != 0)
		result = ((result & ~(T3_ATTR_FG_MASK)) | (a & T3_ATTR_FG_MASK)) & ~_t3_ncv;
	if ((a & T3_ATTR_BG_MASK) != 0)
		result = ((result & ~(T3_ATTR_BG_MASK)) | (a & T3_ATTR_BG_MASK)) & ~_t3_ncv;
	/* If _t3_ncv prevented ACS, then use fallbacks instead. */
	if (((a | b) & T3_ATTR_ACS) && !(result & T3_ATTR_ACS))
		result |= T3_ATTR_FALLBACK_ACS;
	return result;
}

/** Get the set of non-color video attributes.
    @return Attributes bits from the T3_ATTR_* set indicating which attributes can not be
        combined with video attributes.

    Non-color video attributes are attributes that can not be combined with the color
    attributes. It is unspecified what will happen when the are combined.
*/
t3_attr_t t3_term_get_ncv(void) {
	return _t3_ncv;
}

/** Get the terminal capabilities.
    @param caps The location to store the capabilities.
    @param version The version of the library used when compiling (should be ::T3_WINDOW_VERSION).

    @note Do not call this function directly, but use ::t3_term_get_caps which automatically uses
    ::T3_WINDOW_VERSION as the second argument.

    This function can be used to obtain the supported video attributes and other information about
    the capabilities of the terminal. To allow different ABI versions to live together, the version
    number of the library used when compiling the call to this function must be passed.
*/
void t3_term_get_caps_internal(t3_term_caps_t *caps, int version) {
	(void) version;

	caps->highlights = 0;

	if (_t3_smul != NULL) caps->highlights |= T3_ATTR_UNDERLINE;
	if (_t3_bold != NULL) caps->highlights |= T3_ATTR_BOLD;
	if (_t3_rev != NULL) caps->highlights |= T3_ATTR_REVERSE;
	if (_t3_blink != NULL) caps->highlights |= T3_ATTR_BLINK;
	if (_t3_dim != NULL) caps->highlights |= T3_ATTR_DIM;
	if (_t3_smacs != NULL) caps->highlights |= T3_ATTR_ACS;

	caps->colors = _t3_colors;
	caps->pairs = _t3_pairs;

	caps->cap_flags = 0;
	if (_t3_setaf != NULL || _t3_setf != NULL) caps->cap_flags |= T3_TERM_CAP_FG;
	if (_t3_setab != NULL || _t3_setb != NULL) caps->cap_flags |= T3_TERM_CAP_BG;
	if (_t3_scp != NULL) caps->cap_flags |= T3_TERM_CAP_CP;
}

/** @internal
    @brief Set the attributes to sane values, removing conflicting values.
*/
t3_attr_t _t3_term_sanitize_attrs(t3_attr_t attrs) {
	/* Set color to unspecified if it is out of range. */
	if (_t3_scp == NULL) {
		if (((attrs & T3_ATTR_FG_MASK) >> T3_ATTR_COLOR_SHIFT) > (_t3_colors + 1) &&
				(attrs & T3_ATTR_FG_MASK) != T3_ATTR_FG_DEFAULT)
			attrs &= ~T3_ATTR_FG_MASK;
		if (((attrs & T3_ATTR_BG_MASK) >> (T3_ATTR_COLOR_SHIFT + 9)) > (_t3_colors + 1) &&
				(attrs & T3_ATTR_BG_MASK) != T3_ATTR_BG_DEFAULT)
			attrs &= ~T3_ATTR_BG_MASK;
	} else {
		if (((attrs & T3_ATTR_FG_MASK) >> T3_ATTR_COLOR_SHIFT) > (_t3_pairs + 1) &&
				(attrs & T3_ATTR_FG_MASK) != T3_ATTR_FG_DEFAULT)
			attrs &= ~T3_ATTR_FG_MASK;
	}
	return attrs;
}

/** Retrieve the state of the modifiers using terminal specific hacks.

    This function can be used to retrieve the modifier state from the terminal,
    if the terminal provides a method for querying the corrent modifier state.
    One example of such a terminal is the linux console. Using this function
    is basically a hack to get at state that is not encoded in the key sequence,
    and is not the prefered way of accessing this data. For some terminals this
    is however the only way to get at this data.
*/
int t3_term_get_modifiers_hack(void) {
	switch (_t3_modifier_hack) {
		case _T3_MODHACK_NONE:
			return 0;
#ifdef HAS_TIOCLINUX
		case _T3_MODHACK_LINUX: {
			int cmd = TIOCL_GETSHIFTSTATE;
			int result = 0;
			if (ioctl(_t3_terminal_in_fd, TIOCLINUX, &cmd) != 0)
				return 0;
			if (cmd & (1 << KG_SHIFT))
				result |= T3_TERM_KEY_SHIFT;
			if (cmd & (1 << KG_CTRL))
				result |= T3_TERM_KEY_CTRL;
			if (cmd & ((1 << KG_ALT) | (1 << KG_ALTGR)))
				result |= T3_TERM_KEY_META;
			return result;
		}
#endif
		default:
			return 0;
	}
}

/** @} */
