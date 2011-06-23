/* Copyright (C) 2010 G.P. Halkes
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
#ifdef HAS_SELECT_H
#include <sys/select.h>
#else
#include <sys/time.h>
#include <sys/types.h>
#endif
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#if defined(HAS_WINSIZE_IOCTL) || defined(HAS_SIZE_IOCTL)
#include <sys/ioctl.h>
#endif
#include <assert.h>
#include <limits.h>
#include <transcript/transcript.h>
#include <unicode.h>

#include "window.h"
#include "internal.h"
#include "convert_output.h"


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

/** @internal States for parsing cursor position reports. */
typedef enum {
	STATE_INITIAL,
	STATE_ESC_SEEN,
	STATE_ROW,
	STATE_COLUMN
} detection_state_t;

/** @internal
    @brief Wrapper for strcmp which converts the return value to boolean. */
#define streq(a,b) (strcmp((a), (b)) == 0)

/** @internal
    @brief Add a separator for creating ANSI strings in ::set_attrs. */
#define ADD_ANSI_SEP() do { strcat(mode_string, sep); sep = ";"; } while(0)

/** @internal
    @brief Swap two line_data_t structures. Used in ::t3_term_update. */
#define SWAP_LINES(a, b) do { line_data_t save; save = (a); (a) = (b); (b) = save; } while (0)

static struct termios saved; /**< Terminal state as saved in ::t3_term_init */
static t3_bool initialised, /**< Boolean indicating whether the terminal has been initialised. */
	seqs_initialised; /**< Boolean indicating whether the terminal control sequences have been initialised. */
static fd_set inset; /**< File-descriptor set used for select in ::t3_term_get_keychar. */

static char *smcup, /**< Terminal control string: start cursor positioning mode. */
	*rmcup, /**< Terminal control string: stop cursor positioning mode. */
	*cup, /**< Terminal control string: position cursor. */
	*sc, /**< Terminal control string: save cursor position. */
	*rc, /**< Terminal control string: restore cursor position. */
	*clear, /**< Terminal control string: clear terminal. */
	*home, /**< Terminal control string: cursor to home position. */
	*vpa, /**< Terminal control string: set vertical cursor position. */
	*hpa, /**< Terminal control string: set horizontal cursor position. */
	*cud, /**< Terminal control string: move cursor up. */
	*cud1, /**< Terminal control string: move cursor up 1 line. */
	*cuf, /**< Terminal control string: move cursor forward. */
	*cuf1, /**< Terminal control string: move cursor forward one position. */
	*civis, /**< Terminal control string: hide cursor. */
	*cnorm, /**< Terminal control string: show cursor. */
	*sgr, /**< Terminal control string: set graphics rendition. */
	*setaf, /**< Terminal control string: set foreground color (ANSI). */
	*setab, /**< Terminal control string: set background color (ANSI). */
	*op, /**< Terminal control string: reset colors. */
	*smacs, /**< Terminal control string: start alternate character set mode. */
	*rmacs, /**< Terminal control string: stop alternate character set mode. */
	*sgr0, /**< Terminal control string: reset graphics rendition. */
	*smul, /**< Terminal control string: start underline mode. */
	*rmul, /**< Terminal control string: stop underline mode. */
	*rev, /**< Terminal control string: start reverse video. */
	*bold, /**< Terminal control string: start bold. */
	*blink, /**< Terminal control string: start blink. */
	*dim, /**< Terminal control string: start dim. */
	*setf, /**< Terminal control string: set foreground color. */
	*setb, /**< Terminal control string: set background color. */
	*el, /**< Terminal control string: clear to end of line. */
	*scp; /**< Terminal control string: set color pair. */
static t3_chardata_t ncv; /**< Terminal info: Non-color video attributes (encoded in t3_chardata_t). */
static t3_bool bce; /**< Terminal info: screen erased with background color. */
static int colors, pairs;

t3_window_t *_t3_terminal_window; /**< @internal t3_window_t struct representing the last drawn terminal state. */
static line_data_t old_data; /**< line_data_t struct used in terminal update to save previous line state. */

static int lines, /**< Size of terminal (lines). */
	columns, /**< Size of terminal (columns). */
	cursor_y, /**< Cursor position (y coordinate). */
	cursor_x, /**< Cursor position (x coordinate). */
	new_cursor_y, /**< New cursor position (y coordinate). */
	new_cursor_x; /**< New cursor position (x coordinate). */
static t3_bool show_cursor = t3_true, new_show_cursor = t3_true; /**< Boolean indicating whether the cursor is visible currently. */

/** Conversion table between color attributes and ANSI colors. */
static int attr_to_color[10] = { 9, 0, 1, 2, 3, 4, 5, 6, 7, 9 };
/** Conversion table between color attributes and non-ANSI colors. */
static int attr_to_alt_color[10] = { 0, 0, 4, 2, 6, 1, 5, 3, 7, 0 };
static t3_chardata_t attrs = 0, /**< Last used set of attributes. */
	ansi_attrs = 0, /**< Bit mask indicating which attributes should be drawn as ANSI colors. */
	/** Attributes for which the only way to turn of the attribute is to reset all attributes. */
	reset_required_mask = _T3_ATTR_BOLD | _T3_ATTR_REVERSE | _T3_ATTR_BLINK | _T3_ATTR_DIM;
/** Callback for _T3_ATTR_USER. */
static t3_attr_user_callback_t user_callback = NULL;

/** Alternate character set conversion table from TERM_* values to terminal ACS characters. */
static char alternate_chars[256];
/** Alternate character set fall-back characters for when the terminal does not
    provide a proper ACS character. */
static const char *default_alternate_chars[256];

/** File descriptor of the terminal. */
static int terminal_fd;

/** Boolean indicating whether the library is currently detecting the terminal capabilities. */
static t3_bool detecting_terminal_capabilities = t3_true;
/** Boolean indicating whether the terminal capbilities detection requires finishing.

    This variable is the only variable on which a race-condition can occur
	(provided that only the ::t3_term_get_keychar function is called from a
	separate thread). The @c _t3_term_(encoding|combining|double_width) values
    are also accessed from two different threads, but only written from one. And
    the updates to those are such that using an old value temporarily is not a
    problem.

    Note that to prevent interference with adjecent variables, we use a @c long,
    instead of a ::t3_bool.
*/
static long detection_needs_finishing;

/** Store whether the terminal is actually the screen program.

    If set, the terminal-capabilities detection will send extra pass-through
	markers before and after the cursor position request to ensure we get
	results. */
static t3_bool terminal_is_screen;

static int last_key = -1, /**< Last keychar returned from ::t3_term_get_keychar. Used in ::t3_term_unget_keychar. */
	stored_key = -1; /**< Location for storing "ungot" keys in ::t3_term_unget_keychar. */

int _t3_term_encoding = _T3_TERM_UNKNOWN, /**< @internal Detected terminal encoding/mode. */
	_t3_term_combining = -1, /**< @internal Terminal combining capabilities. */
	_t3_term_double_width = -1; /**< @internal Terminal double width character support level. */

static char current_charset[80];

#define SET_CHARACTER(_idx, _utf, _ascii) do { \
	if (t3_term_can_draw((_utf), strlen(_utf))) \
		default_alternate_chars[_idx] = _utf; \
	else \
		default_alternate_chars[_idx] = _ascii; \
} while (0)

/** Fill the defaults table with fall-back characters for the alternate character set.
    @param table The table to fill. */
static void set_alternate_chars_defaults(void) {
	SET_CHARACTER('}', "\xc2\xa3", "f"); /* U+00A3 POUND SIGN [1.1] */
	SET_CHARACTER('.', "\xe2\x96\xbc", "v"); /* U+25BC BLACK DOWN-POINTING TRIANGLE [1.1] */
	SET_CHARACTER(',', "\xe2\x97\x80", "<"); /* U+25C0 BLACK LEFT-POINTING TRIANGLE [1.1] */
	SET_CHARACTER('+', "\xe2\x96\xb6", ">"); /* U+25B6 BLACK RIGHT-POINTING TRIANGLE [1.1] */
	SET_CHARACTER('-', "\xe2\x96\xb2", "^"); /* U+25B2 BLACK UP-POINTING TRIANGLE [1.1] */
	SET_CHARACTER('h', "\xe2\x96\x92", "#"); /* U+2592 MEDIUM SHADE [1.1] */
	SET_CHARACTER('~', "\xc2\xb7", "o"); /* U+00B7 MIDDLE DOT [1.1] */
	SET_CHARACTER('a', "\xe2\x96\x92", ":"); /* U+2592 MEDIUM SHADE [1.1] */
	SET_CHARACTER('f', "\xc2\xb0", "\\"); /* U+00B0 DEGREE SIGN [1.1] */
	SET_CHARACTER('z', "\xe2\x89\xa5", ">"); /* U+2265 GREATER-THAN OR EQUAL TO [1.1] */
	SET_CHARACTER('{', "\xcf\x80", "*"); /* U+03C0 GREEK SMALL LETTER PI [1.1] */
	SET_CHARACTER('q', "\xe2\x94\x80", "-"); /* U+2500 BOX DRAWINGS LIGHT HORIZONTAL [1.1] */
	/* Should probably be something like a crossed box, for now keep #.
	   - ncurses maps to SNOWMAN!
	   - xterm shows 240B, which is not desirable either
	*/
	SET_CHARACTER('i', "#", "#");
	SET_CHARACTER('n', "\xe2\x94\xbc", "+"); /* U+253C BOX DRAWINGS LIGHT VERTICAL AND HORIZONTAL [1.1] */
	SET_CHARACTER('y', "\xe2\x89\xa4", "<"); /* U+2264 LESS-THAN OR EQUAL TO [1.1] */
	SET_CHARACTER('m', "\xe2\x94\x94", "+"); /* U+2514 BOX DRAWINGS LIGHT UP AND RIGHT [1.1] */
	SET_CHARACTER('j', "\xe2\x94\x98", "+"); /* U+2518 BOX DRAWINGS LIGHT UP AND LEFT [1.1] */
	SET_CHARACTER('|', "\xe2\x89\xa0", "!"); /* U+2260 NOT EQUAL TO [1.1] */
	SET_CHARACTER('g', "\xc2\xb1", "#"); /* U+00B1 PLUS-MINUS SIGN [1.1] */
	SET_CHARACTER('o', "\xe2\x8e\xba", "~"); /* U+23BA HORIZONTAL SCAN LINE-1 [3.2] */
	SET_CHARACTER('p', "\xe2\x8e\xbb", "-"); /* U+23BB HORIZONTAL SCAN LINE-3 [3.2] */
	SET_CHARACTER('r', "\xe2\x8e\xbc", "-"); /* U+23BC HORIZONTAL SCAN LINE-7 [3.2] */
	SET_CHARACTER('s', "\xe2\x8e\xbd", "_"); /* U+23BD HORIZONTAL SCAN LINE-9 [3.2] */
	SET_CHARACTER('0', "\xe2\x96\xae", "#"); /* U+25AE BLACK VERTICAL RECTANGLE [1.1] */
	SET_CHARACTER('w', "\xe2\x94\xac", "+"); /* U+252C BOX DRAWINGS LIGHT DOWN AND HORIZONTAL [1.1] */
	SET_CHARACTER('u', "\xe2\x94\xa4", "+"); /* U+2524 BOX DRAWINGS LIGHT VERTICAL AND LEFT [1.1] */
	SET_CHARACTER('t', "\xe2\x94\x9c", "+"); /* U+251C BOX DRAWINGS LIGHT VERTICAL AND RIGHT [1.1] */
	SET_CHARACTER('v', "\xe2\x94\xb4", "+"); /* U+2534 BOX DRAWINGS LIGHT UP AND HORIZONTAL [1.1] */
	SET_CHARACTER('l', "\xe2\x94\x8c", "+"); /* U+250C BOX DRAWINGS LIGHT DOWN AND RIGHT [1.1] */
	SET_CHARACTER('k', "\xe2\x94\x90", "+"); /* U+2510 BOX DRAWINGS LIGHT DOWN AND LEFT [1.1] */
	SET_CHARACTER('x', "\xe2\x94\x82", "|"); /* U+2502 BOX DRAWINGS LIGHT VERTICAL [1.1] */
	SET_CHARACTER('`', "\xe2\x97\x86", "+"); /* U+25C6 BLACK DIAMOND [1.1] */
}

/** Get fall-back character for alternate character set character (internal use only).
    @param idx The character to retrieve the fall-back character for.
    @return The fall-back character.
*/
static const char *get_default_acs(int idx) {
	if (idx < 0 || idx > 255)
		return " ";
	return default_alternate_chars[idx] != NULL ? default_alternate_chars[idx] : " ";
}

static void set_attrs(t3_chardata_t new_attrs);
static t3_attr_t chardata_to_attr(t3_chardata_t chardata);

#ifndef HAS_STRDUP
static char *strdup_impl(const char *str) {
	char *result;
	size_t len = strlen(str) + 1;

	if ((result = malloc(len)) == NULL)
		return NULL;
	memcpy(result, str, len);
	return result;
}
#else
#define strdup_impl strdup
#endif

/** Get a terminfo string.
    @param name The name of the requested terminfo string.
    @return The value of the string @p name, or @a NULL if not available.

    Strings returned must be free'd.
*/
static char *get_ti_string(const char *name) {
	char *result = _t3_tigetstr(name);
	if (result == (char *) 0 || result == (char *) -1)
		return NULL;

	return strdup_impl(result);
}

/** Move cursor to screen position.
    @param line The screen line to move the cursor to.
    @param col The screen column to move the cursor to.

	This function uses the @c cup terminfo string if available, and emulates
    it through other means if necessary.
*/
static void do_cup(int line, int col) {
	if (cup != NULL) {
		_t3_putp(_t3_tparm(cup, 2, line, col));
		return;
	}
	if (vpa != NULL) {
		_t3_putp(_t3_tparm(vpa, 1, line));
		_t3_putp(_t3_tparm(hpa, 1, col));
		return;
	}
	if (home != NULL) {
		int i;

		_t3_putp(home);
		if (line > 0) {
			if (cud != NULL) {
				_t3_putp(_t3_tparm(cud, 1, line));
			} else {
				for (i = 0; i < line; i++)
					_t3_putp(cud1);
			}
		}
		if (col > 0) {
			if (cuf != NULL) {
				_t3_putp(_t3_tparm(cuf, 1, col));
			} else {
				for (i = 0; i < col; i++)
					_t3_putp(cuf1);
			}
		}
	}
}

/** Start cursor positioning mode.

    If @c smcup is not available for the terminal, a simple @c clear is used instead.
*/
static void do_smcup(void) {
	if (smcup != NULL) {
		_t3_putp(smcup);
		return;
	}
	if (clear != NULL) {
		_t3_putp(clear);
		return;
	}
}

/** Stop cursor positioning mode.

    If @c rmcup is not available for the terminal, a @c clear is used instead,
    followed by positioning the cursor in the bottom left corner.
*/
static void do_rmcup(void) {
	if (rmcup != NULL) {
		_t3_putp(rmcup);
		return;
	}
	if (clear != NULL) {
		_t3_putp(clear);
		do_cup(lines - 1, 0);
		return;
	}
}

/** Detect to what extent a terminal description matches the ANSI terminal standard.

    For (partially) ANSI compliant terminals optimization of the output can be done
    such that fewer characters need to be sent to the terminal than by using @c sgr.
*/
static void detect_ansi(void) {
	t3_chardata_t non_existent = 0;

	if (op != NULL && (streq(op, "\033[39;49m") || streq(op, "\033[49;39m"))) {
		if (setaf != NULL && streq(setaf, "\033[3%p1%dm") &&
				setab != NULL && streq(setab, "\033[4%p1%dm"))
			ansi_attrs |= _T3_ATTR_FG_MASK | _T3_ATTR_BG_MASK;
	}
	if (smul != NULL && rmul != NULL && streq(smul, "\033[4m") && streq(rmul, "\033[24m"))
		ansi_attrs |= _T3_ATTR_UNDERLINE;
	if (smacs != NULL && rmacs != NULL && streq(smacs, "\033[11m") && streq(rmacs, "\033[10m"))
		ansi_attrs |= _T3_ATTR_ACS;

	/* So far, we have been able to check that the "exit mode" operation was ANSI compatible as well.
	   However, for bold, dim, reverse and blink we can't check this, so we will only accept them
	   as attributes if the terminal uses ANSI colors, and they all match in as far as they exist.
	*/
	if ((ansi_attrs & (_T3_ATTR_FG_MASK | _T3_ATTR_BG_MASK)) == 0 || (ansi_attrs & (_T3_ATTR_UNDERLINE | _T3_ATTR_ACS)) == 0)
		return;

	if (rev != NULL) {
		if (streq(rev, "\033[7m")) {
			/* On many terminals smso is also reverse video. If it is, we can
			   verify that rmso removes the reverse video. Otherwise, we just
			   assume that if rev matches ANSI reverse video, the inverse ANSI
			   sequence also works. */
			char *smso = get_ti_string("smso");
			if (streq(smso, rev)) {
				char *rmso = get_ti_string("rmso");
				if (streq(rmso, "\033[27m"))
					ansi_attrs |= _T3_ATTR_REVERSE;
				free(rmso);
			} else {
				ansi_attrs |= _T3_ATTR_REVERSE;
			}
			free(smso);
		}
	} else {
		non_existent |= _T3_ATTR_REVERSE;
	}

	if (bold != NULL) {
		if (streq(bold, "\033[1m"))
			ansi_attrs |= _T3_ATTR_BOLD;
	} else {
		non_existent |= _T3_ATTR_BOLD;
	}

	if (dim != NULL) {
		if (streq(dim, "\033[2m"))
			ansi_attrs |= _T3_ATTR_DIM;
	} else {
		non_existent |= _T3_ATTR_DIM;
	}

	if (blink != NULL) {
		if (streq(blink, "\033[5m"))
			ansi_attrs |= _T3_ATTR_BLINK;
	} else {
		non_existent |= _T3_ATTR_BLINK;
	}

	/* Only accept as ANSI if all attributes accept ACS are either non specified or ANSI. */
	if (((non_existent | ansi_attrs) & (_T3_ATTR_REVERSE | _T3_ATTR_BOLD | _T3_ATTR_DIM | _T3_ATTR_BLINK)) !=
			(_T3_ATTR_REVERSE | _T3_ATTR_BOLD | _T3_ATTR_DIM | _T3_ATTR_BLINK))
		ansi_attrs &= ~(_T3_ATTR_REVERSE | _T3_ATTR_BOLD | _T3_ATTR_DIM | _T3_ATTR_BLINK);
}

static void send_test_string(const char *str) {
	/* Move cursor to the begining of the line. Use cup if hpa is not available.
	   Also, make sure we use line 1, iso line 0, because xterm uses \e[1;<digit>R for
	   some combinations of F3 with modifiers and high-numbered function keys. :-( */
	if (hpa != NULL)
		_t3_putp(_t3_tparm(hpa, 1, 0));
	else
		do_cup(1, 0);

	fputs(str, _t3_putp_file);
	/* Send ANSI cursor reporting string. */
	if (terminal_is_screen)
		_t3_putp("\033P\033[6n\033\\");
	else
		_t3_putp("\033[6n");
}

static int isreset_single(const char *str, const char *reset_string) {
	do {
		/* Note: we don't have to check *reset_string for 0, because then it will
		   automatically be either unequal to *str, or *str will also be 0 which
		   will already have been checked. */
		for (; *str != 0 && *reset_string == *str; str++, reset_string++) {}
		if (*str == '$' && str[1] == '<') {
			str += 2;
			for (; *str != 0 && *str != '>'; str++) {}
			if (*str == '>')
				str++;
		}
	} while (*str != 0 && *reset_string == *str);
	return *str == *reset_string;
}

static int isreset(const char *str) {
	return (sgr0 != NULL && streq(str, sgr0)) || isreset_single(str, "\033[m") || isreset_single(str, "\033[0m");
}

/** Initialize the terminal.
    @param fd The file descriptor of the terminal or -1 for default/last used.
    @param term The name of the terminal, or @c NULL to use the @c TERM environment variable.
    @return One of: ::T3_ERR_SUCCESS, ::T3_ERR_NOT_A_TTY, ::T3_ERR_ERRNO, ::T3_ERR_HARDCOPY_TERMINAL,
        ::T3_ERR_TERMINFODB_NOT_FOUND, ::T3_ERR_UNKNOWN, ::T3_ERR_TERMINAL_TOO_LIMITED,
        ::T3_ERR_NO_SIZE_INFO.

    This function depends on the correct setting of the @c LC_CTYPE property
    by the @c setlocale function. Therefore, the @c setlocale function should
    be called before this function.

    If standard input/output should be used as the terminal, -1 should be
    passed. However, if a previous call to ::t3_term_init has set the terminal
    to another value, using -1 as the file descriptor will use the previous
    value passed unless the previous call resulted in ::T3_ERR_NOT_A_TTY.

    The terminal is initialized to raw mode such that echo is disabled
    (characters typed are not shown), control characters are passed to the
    program (i.e. ctrl-c will result in a character rather than a TERM signal)
    and generally all characters typed are passed to the program immediately
    and with a minimum of pre-processing.
*/
int t3_term_init(int fd, const char *term) {
	static t3_bool detection_done;
#if defined(HAS_WINSIZE_IOCTL)
	struct winsize wsz;
#elif defined(HAS_SIZE_IOCTL)
	struct ttysize wsz;
#endif
	char *enacs;
	struct termios new_params;
	int ncv_int;

	if (initialised)
		return T3_ERR_SUCCESS;

	if (fd >= 0) {
		if ((_t3_putp_file = fdopen(fd, "w")) == NULL)
			return T3_ERR_ERRNO;
		terminal_fd = fd;
	} else if (_t3_putp_file == NULL) {
		/* Unfortunately stdout is not a constant, and _putp_file can therefore not be
		   initialized statically. */
		terminal_fd = STDOUT_FILENO;
		_t3_putp_file = stdout;
	}

	if (!isatty(terminal_fd))
		return T3_ERR_NOT_A_TTY;

	FD_ZERO(&inset);
	FD_SET(terminal_fd, &inset);

	/* FIXME: we should check whether the same value is passed for term each time,
	   or tell the user that only the first time is relevant. */
	if (!seqs_initialised) {
		int error;
		char *acsc;

		if ((error = _t3_setupterm(term, terminal_fd)) != 0) {
			if (error == 3)
				return T3_ERR_HARDCOPY_TERMINAL;
			else if (error == 1)
				return T3_ERR_TERMINFODB_NOT_FOUND;
			else if (error == 2)
				return T3_ERR_TERMINAL_TOO_LIMITED;
			return T3_ERR_UNKNOWN;
		}

		if ((smcup = get_ti_string("smcup")) == NULL || (rmcup = get_ti_string("rmcup")) == NULL) {
			if (smcup != NULL) {
				free(smcup);
				smcup = NULL;
			}
		}
		if ((clear = get_ti_string("clear")) == NULL)
			return T3_ERR_TERMINAL_TOO_LIMITED;

		if ((cup = get_ti_string("cup")) == NULL) {
			if ((hpa = get_ti_string("hpa")) == NULL || ((vpa = get_ti_string("vpa")) == NULL))
				return T3_ERR_TERMINAL_TOO_LIMITED;
		}
		if (hpa == NULL)
			hpa = get_ti_string("hpa");

		sgr = get_ti_string("sgr");
		sgr0 = get_ti_string("sgr0");

		if ((smul = get_ti_string("smul")) != NULL) {
			if ((rmul = get_ti_string("rmul")) == NULL || isreset(rmul))
				reset_required_mask |= _T3_ATTR_UNDERLINE;
		}
		bold = get_ti_string("bold");
		rev = get_ti_string("rev");
		blink = get_ti_string("blink");
		dim = get_ti_string("dim");
		smacs = get_ti_string("smacs");
		if (smacs != NULL && ((rmacs = get_ti_string("rmacs")) == NULL || isreset(rmacs)))
			reset_required_mask |= T3_ATTR_ACS;

		/* If rmul and rmacs are the same, there is a good chance it simply
		   resets everything. */
		if (rmul != NULL && rmacs != NULL && streq(rmul, rmacs))
			reset_required_mask |= T3_ATTR_UNDERLINE | T3_ATTR_ACS;

		if ((setaf = get_ti_string("setaf")) == NULL)
			setf = get_ti_string("setf");

		if ((setab = get_ti_string("setab")) == NULL)
			setb = get_ti_string("setb");

		if (setaf == NULL && setf == NULL && setab == NULL && setb == NULL) {
			if ((scp = get_ti_string("scp")) != NULL) {
				colors = _t3_tigetnum("colors");
				pairs = _t3_tigetnum("colors");
			}
		} else {
			colors = _t3_tigetnum("colors");
			pairs = _t3_tigetnum("colors");
		}

		if (colors < 0) colors = 0;
		if (pairs < 0) pairs = 0;

		op = get_ti_string("op");

		detect_ansi();

		/* If sgr0 and sgr are not defined, don't go into modes in reset_required_mask. */
		if (sgr0 == NULL && sgr == NULL) {
			reset_required_mask = 0;
			rev = NULL;
			bold = NULL;
			blink = NULL;
			dim = NULL;
			if (rmul == NULL) smul = NULL;
			if (rmacs == NULL) smacs = NULL;
		}

		bce = _t3_tigetflag("bce");
		if ((el = get_ti_string("el")) == NULL)
			bce = t3_true;

		if ((sc = get_ti_string("sc")) != NULL && (rc = get_ti_string("rc")) == NULL)
			sc = NULL;

		civis = get_ti_string("civis");
		cnorm = get_ti_string("cnorm");

		if ((acsc = get_ti_string("acsc")) != NULL) {
			if (sgr != NULL || smacs != NULL) {
				size_t i;
				for (i = 0; i < strlen(acsc); i += 2)
					alternate_chars[(unsigned int) acsc[i]] = acsc[i + 1];
			}
			free(acsc);
		}

		ncv_int = _t3_tigetnum("ncv");
		if (ncv_int >= 0) {
			if (ncv_int & (1<<1)) ncv |= T3_ATTR_UNDERLINE;
			if (ncv_int & (1<<2)) ncv |= T3_ATTR_REVERSE;
			if (ncv_int & (1<<3)) ncv |= T3_ATTR_BLINK;
			if (ncv_int & (1<<4)) ncv |= T3_ATTR_DIM;
			if (ncv_int & (1<<5)) ncv |= T3_ATTR_BOLD;
			if (ncv_int & (1<<8)) ncv |= T3_ATTR_ACS;
		}

		seqs_initialised = t3_true;
	}

	/* Get terminal size. First try ioctl, then environment, then terminfo. */
#if defined(HAS_WINSIZE_IOCTL)
	if (ioctl(terminal_fd, TIOCGWINSZ, &wsz) == 0) {
		lines = wsz.ws_row;
		columns = wsz.ws_col;
	} else
#elif defined(HAS_SIZE_IOCTL)
	if (ioctl(terminal_fd, TIOCGSIZE, &wsz) == 0) {
		lines = wsz.ts_lines;
		columns = wsz.ts_cols;
	} else
#endif
	{
		char *lines_env = getenv("LINES");
		char *columns_env = getenv("COLUMNS");
		if (lines_env == NULL || columns_env == NULL || (lines = atoi(lines_env)) == 0 || (columns = atoi(columns_env)) == 0) {
			if ((lines = _t3_tigetnum("lines")) < 0 || (columns = _t3_tigetnum("columns")) < 0)
				return T3_ERR_NO_SIZE_INFO;
		}
	}

	if (!detection_done) {
		const char *charset = transcript_get_codeset();
		strncpy(current_charset, charset, sizeof(current_charset) - 1);
		current_charset[sizeof(current_charset) - 1] = '\0';
		if (!_t3_init_output_convertor(current_charset))
			return T3_ERR_CHARSET_ERROR;

		set_alternate_chars_defaults();
	}

	/* Create or resize terminal window */
	if (_t3_terminal_window == NULL) {
		if ((_t3_terminal_window = t3_win_new(NULL, lines, columns, 0, 0, 0)) == NULL)
			return T3_ERR_ERRNO;
		if ((old_data.data = malloc(sizeof(t3_chardata_t) * INITIAL_ALLOC)) == NULL)
			return T3_ERR_ERRNO;
		old_data.allocated = INITIAL_ALLOC;
		/* Remove terminal window from the window stack. */
		_t3_remove_window(_t3_terminal_window);
	} else {
		if (!t3_win_resize(_t3_terminal_window, lines, columns))
			return T3_ERR_ERRNO;
	}

	if (tcgetattr(terminal_fd, &saved) < 0)
		return T3_ERR_ERRNO;

	new_params = saved;
	new_params.c_iflag &= ~(IXON | IXOFF | IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
	new_params.c_lflag &= ~(ISIG | ICANON | ECHO);
	new_params.c_oflag &= ~OPOST;
	new_params.c_cflag &= ~(CSIZE | PARENB);
	new_params.c_cflag |= CS8;
	new_params.c_cc[VMIN] = 1;

	if (tcsetattr(terminal_fd, TCSADRAIN, &new_params) < 0)
		return T3_ERR_ERRNO;

	/* Start cursor positioning mode. */
	do_smcup();

	if (!detection_done) {
		detection_done = t3_true;
		/* Make sure we use line 1, iso line 0, because xterm uses \e[1;<digit>R for
		   some combinations of F3 with modifiers and high-numbered function keys. :-( */
		if (hpa != NULL) {
			if (vpa != NULL)
				_t3_putp(_t3_tparm(vpa, 1, 1));
			else
				do_cup(1, 0);
		}

		if (term != NULL || (term = getenv("TERM")) != NULL)
			if (strcmp(term , "screen") == 0)
				terminal_is_screen = t3_true;

		#define GENERATE_STRINGS
		#include "terminal_detection.h"
		#undef GENERATE_STRINGS
		_t3_putp(clear);
		fflush(_t3_putp_file);
	}

	/* Make sure the cursor is visible */
	_t3_putp(cnorm);
	do_cup(cursor_y, cursor_x);

	/* Enable alternate character set if required by terminal. */
	if ((enacs = get_ti_string("enacs")) != NULL) {
		_t3_putp(enacs);
		free(enacs);
	}

	/* Set the attributes of the terminal to a known value. */
	set_attrs(0);

	_t3_init_output_buffer();

	initialised = t3_true;
	return T3_ERR_SUCCESS;
}

/** Disable the ANSI terminal control sequence optimization. */
void t3_term_disable_ansi_optimization(void) {
	ansi_attrs = 0;
}

/** Restore terminal state (de-initialize). */
void t3_term_restore(void) {
	if (initialised) {
		/* Ensure complete repaint of the terminal on re-init (if required) */
		t3_win_set_paint(_t3_terminal_window, 0, 0);
		t3_win_clrtobot(_t3_terminal_window);
		if (seqs_initialised) {
			do_rmcup();
			/* Restore cursor to visible state. */
			if (!show_cursor)
				_t3_putp(cnorm);
			/* Make sure attributes are reset */
			set_attrs(0);
			attrs = 0;
			fflush(_t3_putp_file);
		}
		tcsetattr(terminal_fd, TCSADRAIN, &saved);
		initialised = t3_false;
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
	return current_charset;
}

/** Complete the detection of terminal capabilities.

	This routine handles changing the current encoding, if the results of the
    terminal-capabilities detection indicate a need for it.
*/
static void finish_detection(void) {
	t3_bool set_ascii = t3_false;
	const char *current_encoding = transcript_get_codeset();

	/* If the currently set encoding should have been detected, just set to ASCII. */
	switch (_t3_term_encoding) {
		case _T3_TERM_UNKNOWN:
		case _T3_TERM_SINGLE_BYTE:
		case _T3_TERM_GBK: // FIXME: once we can detect this better, handle as known encoding
			if (transcript_equal(current_encoding, "utf8") || transcript_equal(current_encoding, "gb18030") ||
					transcript_equal(current_encoding, "eucjp") || transcript_equal(current_encoding, "euctw") ||
					transcript_equal(current_encoding, "euckr") || transcript_equal(current_encoding, "shiftjis"))
				set_ascii = t3_true;
			break;
		case _T3_TERM_UTF8:
			if (!transcript_equal(current_encoding, "utf8")) {
				strcpy(current_charset, "UTF-8");
				detection_needs_finishing = t3_true;
			} else if (_t3_term_double_width != -1 || _t3_term_combining != -1) {
				detection_needs_finishing = t3_true;
			}
			break;
		case _T3_TERM_CJK:
			/* We would love to be able to say which encoding should have been set by the
			   user, but we simply don't know which ones are valid. So just filter out those
			   that are known to be invalid. */
			if (transcript_equal(current_encoding, "utf8") || transcript_equal(current_encoding, "shiftjis"))
				set_ascii = t3_true;
			break;
		case _T3_TERM_CJK_SHIFT_JIS:
			if (!transcript_equal(current_encoding, "shiftjis")) {
				strcpy(current_charset, "Shift_JIS");
				detection_needs_finishing = t3_true;
			}
			break;
		case _T3_TERM_GB18030:
			if (!transcript_equal(current_encoding, "gb18030")) {
				strcpy(current_charset, "GB18030");
				detection_needs_finishing = t3_true;
			} else if (_t3_term_double_width != -1 || _t3_term_combining != -1) {
				detection_needs_finishing = t3_true;
			}
			break;
		default:
			break;
	}

	if (set_ascii) {
		strcpy(current_charset, "ASCII");
		detection_needs_finishing = t3_true;
	}
}

/** Process a position report triggered by the initialization.
    @arg row The reported row.
    @arg column The reported column.

    The postion reports may generated as a response to the terminal
    initialization. They are used to determine the used character set and
    capabilities of the terminal.
*/
static t3_bool process_position_report(int row, int column) {
	static int report_nr;
	t3_bool result = t3_false;
	(void) row;

	column--;
	#define GENERATE_CODE
	#include "terminal_detection.h"
	#undef GENERATE_CODE

	if (report_nr < INT_MAX)
		report_nr++;
	return result;
}

/** @internal Check if a characters is a digit, in a locale independent way. */
#define non_locale_isdigit(_c) (strchr("0123456789", _c) != NULL)

/** Convert a digit character to an @c int value. */
static int digit_value(int c) {
	const char *digits = "0123456789";
	return (int) (strchr(digits, c) - digits);
}

/** Handle a character read from the terminal to check for position reports.
    @arg c The character read from the terminal.

    The postion reports may generated as a response to the terminal
    initialization. They are used to determine the used character set and
    capabilities of the terminal.
*/
static t3_bool parse_position_reports(int c) {
	static detection_state_t detection_state = STATE_INITIAL;
	static int row, column;

	switch (detection_state) {
		case STATE_INITIAL:
			if (c == 27) {
				detection_state = STATE_ESC_SEEN;
				row = 0;
				column = 0;
			}
			break;
		case STATE_ESC_SEEN:
			if (c == '[') {
				detection_state = STATE_ROW;
				row = 0;
			} else {
				detection_state = STATE_INITIAL;
			}
			break;
		case STATE_ROW:
			if (non_locale_isdigit(c))
				row = row * 10 + digit_value(c);
			else if (c == ';')
				detection_state = STATE_COLUMN;
			else
				detection_state = STATE_INITIAL;
			break;
		case STATE_COLUMN:
			if (non_locale_isdigit(c)) {
				column = column * 10 + digit_value(c);
			} else if (c == 'R') {
				detection_state = STATE_INITIAL;
				return process_position_report(row, column);
			} else {
				detection_state = STATE_INITIAL;
			}
			break;
		default:
			detection_state = STATE_INITIAL;
			break;
	}
	return t3_false;
}

/** Read a character from @c stdin, continueing after interrupts.
    @retval A @c char read from stdin.
    @retval T3_ERR_ERRNO if an error occurred.
	@retval T3_ERR_EOF on end of file.
*/
static int safe_read_char(void) {
	char c;
	while (1) {
		ssize_t retval = read(terminal_fd, &c, 1);
		if (retval < 0 && errno == EINTR) {
			continue;
		} else if (retval >= 1) {
			if (detecting_terminal_capabilities) {
				if (parse_position_reports((int) c)) {
					stored_key = c;
					return T3_WARN_UPDATE_TERMINAL;
				}
			}
			return (int) (unsigned char) c;
		} else if (retval == 0) {
			return T3_ERR_EOF;
		}
		return T3_ERR_ERRNO;
	}
}

/** Get a key @c char from stdin with timeout.
    @param msec The timeout in miliseconds, or a value <= 0 for indefinite wait.
    @retval >=0 A @c char read from stdin.
    @retval ::T3_ERR_ERRNO on error, with @c errno set to the error.
    @retval ::T3_ERR_EOF on end of file.
    @retval ::T3_ERR_TIMEOUT if there was no character to read within the specified timeout.
    @retval ::T3_WARN_UPDATE_TERMINAL if the terminal-feature detection has finished
    	and requires that the terminal is updated. @b Note: this is not an error,
    	but a signal to update the terminal. To check for errors, use:
    @code
    	t3_term_get_keychar(msec) < T3_WARN_MIN
    @endcode
*/
int t3_term_get_keychar(int msec) {
	int retval;
	fd_set _inset;
	struct timeval timeout;

	if (stored_key >= 0) {
		last_key = stored_key;
		stored_key = -1;
		return last_key;
	}

	while (1) {
		_inset = inset;
		if (msec > 0) {
			timeout.tv_sec = msec / 1000;
			timeout.tv_usec = (msec % 1000) * 1000;
		}

		retval = select(terminal_fd + 1, &_inset, NULL, NULL, msec > 0 ? &timeout : NULL);

		if (retval < 0) {
			if (errno == EINTR)
				continue;
			return T3_ERR_ERRNO;
		} else if (retval == 0) {
			return T3_ERR_TIMEOUT;
		} else {
			return last_key = safe_read_char();
		}
	}
}

/** Push a @c char back for later retrieval with ::t3_term_get_keychar.
    @param c The @c char to push back.
    @return The @c char pushed back or ::T3_ERR_BAD_ARG.

    Only a @c char just read from stdin with ::t3_term_get_keychar can be pushed back.
*/
int t3_term_unget_keychar(int c) {
	if (c == last_key && c >= 0) {
		stored_key = c;
		return c;
	}
	return T3_ERR_BAD_ARG;
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
		*height = lines;
	if (width != NULL)
		*width = columns;
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

	if (ioctl(terminal_fd, TIOCGWINSZ, &wsz) < 0)
		return t3_true;

	lines = wsz.ws_row;
	columns = wsz.ws_col;

	if (columns == _t3_terminal_window->width && lines == _t3_terminal_window->height)
		return t3_true;

	if (columns < _t3_terminal_window->width || lines != _t3_terminal_window->height) {
		/* Clear the cache of the terminal contents and the actual terminal. This
		   is necessary because shrinking the terminal tends to cause all kinds of
		   weird corruption of the on screen state. */
		t3_term_redraw();
	}

	return t3_win_resize(_t3_terminal_window, lines, columns);
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
static void set_attrs_non_ansi(t3_chardata_t new_attrs) {
	t3_chardata_t attrs_basic_non_ansi = attrs & BASIC_ATTRS & ~ansi_attrs;
	t3_chardata_t new_attrs_basic_non_ansi = new_attrs & BASIC_ATTRS & ~ansi_attrs;

	if (attrs_basic_non_ansi != new_attrs_basic_non_ansi) {
		t3_chardata_t changed;
		if (attrs_basic_non_ansi & ~new_attrs & reset_required_mask) {
			if (sgr != NULL) {
				_t3_putp(_t3_tparm(sgr, 9,
					0,
					new_attrs & _T3_ATTR_UNDERLINE,
					new_attrs & _T3_ATTR_REVERSE,
					new_attrs & _T3_ATTR_BLINK,
					new_attrs & _T3_ATTR_DIM,
					new_attrs & _T3_ATTR_BOLD,
					0,
					0,
					new_attrs & _T3_ATTR_ACS));
				attrs = new_attrs & ~(_T3_ATTR_FG_MASK | _T3_ATTR_BG_MASK);
				attrs_basic_non_ansi = attrs & ~ansi_attrs;
			} else {
				/* Note that this will not be NULL if it is required because of
				   tests in the initialization. */
				_t3_putp(sgr0);
				attrs_basic_non_ansi = attrs = 0;
			}
		}

		/* Set any attributes required. If sgr was previously used, the calculation
		   of 'changed' results in 0. */
		changed = attrs_basic_non_ansi ^ new_attrs_basic_non_ansi;
		if (changed) {
			if (changed & _T3_ATTR_UNDERLINE)
				_t3_putp(new_attrs & _T3_ATTR_UNDERLINE ? smul : rmul);
			if (changed & _T3_ATTR_REVERSE)
				_t3_putp(rev);
			if (changed & _T3_ATTR_BLINK)
				_t3_putp(blink);
			if (changed & _T3_ATTR_DIM)
				_t3_putp(dim);
			if (changed & _T3_ATTR_BOLD)
				_t3_putp(bold);
			if (changed & _T3_ATTR_ACS)
				_t3_putp(new_attrs & _T3_ATTR_ACS ? smacs : rmacs);
		}
	}

	/* If colors are set using ANSI sequences, we are done here. */
	if ((~ansi_attrs & (_T3_ATTR_FG_MASK | _T3_ATTR_BG_MASK)) == 0)
		return;

	/* Specifying DEFAULT as color is the same as not specifying anything. However,
	   for ::t3_term_combine_attrs there is a distinction between an explicit and an
	   implicit color. Here we don't care about that distinction so we remove it. */
	if ((new_attrs & _T3_ATTR_FG_MASK) == _T3_ATTR_FG_DEFAULT)
		new_attrs &= ~(_T3_ATTR_FG_MASK);
	if ((new_attrs & _T3_ATTR_BG_MASK) == _T3_ATTR_BG_DEFAULT)
		new_attrs &= ~(_T3_ATTR_BG_MASK);

	if (scp == NULL) {
		/* Set default color through op string */
		if (((attrs & _T3_ATTR_FG_MASK) != (new_attrs & _T3_ATTR_FG_MASK) && (new_attrs & _T3_ATTR_FG_MASK) == 0) ||
				((attrs & _T3_ATTR_BG_MASK) != (new_attrs & _T3_ATTR_BG_MASK) && (new_attrs & _T3_ATTR_BG_MASK) == 0)) {
			if (op != NULL) {
				_t3_putp(op);
				attrs = new_attrs & ~(_T3_ATTR_FG_MASK | _T3_ATTR_BG_MASK);
			}
		}

		if ((attrs & _T3_ATTR_FG_MASK) != (new_attrs & _T3_ATTR_FG_MASK)) {
			if (setaf != NULL)
				_t3_putp(_t3_tparm(setaf, 1, attr_to_color[(new_attrs >> _T3_ATTR_COLOR_SHIFT) & 0xf]));
			else if (setf != NULL)
				_t3_putp(_t3_tparm(setf, 1, attr_to_alt_color[(new_attrs >> _T3_ATTR_COLOR_SHIFT) & 0xf]));
		}

		if ((attrs & _T3_ATTR_BG_MASK) != (new_attrs & _T3_ATTR_BG_MASK)) {
			if (setab != NULL)
				_t3_putp(_t3_tparm(setab, 1, attr_to_color[(new_attrs >> (_T3_ATTR_COLOR_SHIFT + 4)) & 0xf]));
			else if (setb != NULL)
				_t3_putp(_t3_tparm(setb, 1, attr_to_alt_color[(new_attrs >> (_T3_ATTR_COLOR_SHIFT + 4)) & 0xf]));
		}
	} else {
		if ((new_attrs & _T3_ATTR_FG_MASK) == 0)
			_t3_putp(op);
		else
			_t3_putp(_t3_tparm(scp, 1, (new_attrs >> _T3_ATTR_COLOR_SHIFT) & 0xf));
	}
}

/** @internal
    @brief Set terminal drawing attributes.
    @param new_attrs The new attributes that should be used for subsequent character display.

    The state of ::attrs is updated to reflect the new state.
*/
static void set_attrs(t3_chardata_t new_attrs) {
	char mode_string[30]; /* Max is (if I counted correctly) 24. Use 30 for if I miscounted. */
	t3_chardata_t changed_attrs;
	const char *sep = "[";

	/* Flush any characters accumulated in the output buffer before switching attributes. */
	_t3_output_buffer_print();

	/* Just in case the caller forgot */
	new_attrs &= _T3_ATTR_MASK & ~_T3_ATTR_FALLBACK_ACS;

	if (new_attrs == 0 && (sgr0 != NULL || sgr != NULL)) {
		/* Use sgr instead of sgr0 as this is probably more tested (see rxvt-unicode terminfo bug) */
		if (sgr != NULL)
			_t3_putp(_t3_tparm(sgr, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0));
		else
			_t3_putp(sgr0);
		attrs = 0;
		return;
	}

	changed_attrs = (new_attrs ^ attrs) & ~ansi_attrs;
	if (changed_attrs != 0)
		set_attrs_non_ansi(new_attrs);

	changed_attrs = (new_attrs ^ attrs) & ansi_attrs;
	if (changed_attrs == 0) {
		attrs = new_attrs;
		return;
	}

	mode_string[0] = '\033';
	mode_string[1] = 0;

	if (changed_attrs & _T3_ATTR_UNDERLINE) {
		ADD_ANSI_SEP();
		strcat(mode_string, new_attrs & _T3_ATTR_UNDERLINE ? "4" : "24");
	}

	if (changed_attrs & (_T3_ATTR_BOLD | _T3_ATTR_DIM)) {
		ADD_ANSI_SEP();
		strcat(mode_string, new_attrs & _T3_ATTR_BOLD ? "1" : (new_attrs & _T3_ATTR_DIM ? "2" : "22"));
	}

	if (changed_attrs & _T3_ATTR_REVERSE) {
		ADD_ANSI_SEP();
		strcat(mode_string, new_attrs & _T3_ATTR_REVERSE ? "7" : "27");
	}

	if (changed_attrs & _T3_ATTR_BLINK) {
		ADD_ANSI_SEP();
		strcat(mode_string, new_attrs & _T3_ATTR_BLINK ? "5" : "25");
	}

	if (changed_attrs & _T3_ATTR_ACS) {
		ADD_ANSI_SEP();
		strcat(mode_string, new_attrs & _T3_ATTR_ACS ? "11" : "10");
	}

	if (changed_attrs & _T3_ATTR_FG_MASK) {
		char color[3];
		color[0] = '3';
		color[1] = '0' + attr_to_color[(new_attrs >> _T3_ATTR_COLOR_SHIFT) & 0xf];
		color[2] = 0;
		ADD_ANSI_SEP();
		strcat(mode_string, color);
	}

	if (changed_attrs & _T3_ATTR_BG_MASK) {
		char color[3];
		color[0] = '4';
		color[1] = '0' + attr_to_color[(new_attrs >> (_T3_ATTR_COLOR_SHIFT + 4)) & 0xf];
		color[2] = 0;
		ADD_ANSI_SEP();
		strcat(mode_string, color);
	}
	strcat(mode_string, "m");
	_t3_putp(mode_string);
	attrs = new_attrs;
}

/** Set terminal drawing attributes.
    @param new_attrs The new attributes that should be used for subsequent character display.
*/
void t3_term_set_attrs(t3_attr_t new_attrs) {
	set_attrs(_t3_term_attr_to_chardata(new_attrs));
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
	if (new_show_cursor != show_cursor) {
		show_cursor = new_show_cursor;
		if (show_cursor) {
			do_cup(new_cursor_y, new_cursor_x);
			cursor_y = new_cursor_y;
			cursor_x = new_cursor_x;
			_t3_putp(cnorm);
		} else {
			_t3_putp(civis);
		}
	} else {
		if (new_cursor_y != cursor_y || new_cursor_x != cursor_x) {
			do_cup(new_cursor_y, new_cursor_x);
			cursor_y = new_cursor_y;
			cursor_x = new_cursor_x;
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
	t3_chardata_t new_attrs;
	int i, j;

	if (detection_needs_finishing) {
		_t3_init_output_convertor(current_charset);
		set_alternate_chars_defaults();
		t3_win_set_paint(_t3_terminal_window, 0, 0);
		t3_win_clrtobot(_t3_terminal_window);
		detection_needs_finishing = t3_false;
	}

	if (civis != NULL) {
		if (new_show_cursor != show_cursor) {
			/* If the cursor should now be invisible, hide it before drawing. If the
			   cursor should now be visible, leave it invisible until after drawing. */
			if (!new_show_cursor)
				_t3_putp(civis);
		} else if (show_cursor) {
			if (new_cursor_y == cursor_y && new_cursor_x == cursor_x)
				_t3_putp(sc);
			_t3_putp(civis);
		}
	}

	/* There may be another optimization possibility here: if the new line is
	   shorter than the old line, we could detect that the end of the new line
	   matches the old line. In that case we could skip printing the end of the
	   new line. Of course the question is how often this will actually happen.
	   It also brings with it several issues with the clearing of the end of
	   the line. */
	for (i = 0; i < lines; i++) {
		int new_idx, old_idx = _t3_terminal_window->lines[i].length, width = 0;
		SWAP_LINES(old_data, _t3_terminal_window->lines[i]);
		_t3_win_refresh_term_line(i);

		new_idx = _t3_terminal_window->lines[i].length;

		/* Find the last character that is different. */
		if (old_data.width == _t3_terminal_window->lines[i].width) {
			for (new_idx--, old_idx--; new_idx >= 0 &&
					old_idx >= 0 && _t3_terminal_window->lines[i].data[new_idx] == old_data.data[old_idx];
					new_idx--, old_idx--)
			{}
			if (new_idx == -1) {
				assert(old_idx == -1);
				goto done;
			}
			assert(old_idx >= 0);
			for (new_idx++; new_idx < _t3_terminal_window->lines[i].length &&
					_T3_CHARDATA_TO_WIDTH(_t3_terminal_window->lines[i].data[new_idx]) == 0; new_idx++) {}
			for (old_idx++; old_idx < old_data.length &&
					_T3_CHARDATA_TO_WIDTH(old_data.data[old_idx]) == 0; old_idx++) {}
		}

		/* Find the first character that is different */
		for (j = 0; j < new_idx && j < old_idx && _t3_terminal_window->lines[i].data[j] == old_data.data[j]; j++)
				width += _T3_CHARDATA_TO_WIDTH(_t3_terminal_window->lines[i].data[j]);

		/* Go back to the last non-zero-width character, because that is the one we want to print first. */
		if ((j < new_idx && _T3_CHARDATA_TO_WIDTH(_t3_terminal_window->lines[i].data[j]) == 0) ||
				(j < old_idx && _T3_CHARDATA_TO_WIDTH(old_data.data[j]) == 0))
		{
			for (; j > 0 && (_T3_CHARDATA_TO_WIDTH(_t3_terminal_window->lines[i].data[j]) == 0 ||
					_T3_CHARDATA_TO_WIDTH(old_data.data[j]) == 0); j--) {}
			width -= _T3_CHARDATA_TO_WIDTH(_t3_terminal_window->lines[i].data[j]);
		}

		/* Position the cursor */
		do_cup(i, width);
		for (; j < new_idx; j++) {
			if (_T3_CHARDATA_TO_WIDTH(_t3_terminal_window->lines[i].data[j]) > 0) {
				if (width + _T3_CHARDATA_TO_WIDTH(_t3_terminal_window->lines[i].data[j]) > _t3_terminal_window->width)
					break;

				new_attrs = _t3_terminal_window->lines[i].data[j] & (_T3_ATTR_MASK & ~_T3_ATTR_FALLBACK_ACS);

				width += _T3_CHARDATA_TO_WIDTH(_t3_terminal_window->lines[i].data[j]);
				if (user_callback != NULL && (new_attrs & _T3_ATTR_USER)) {
					/* Let the user draw this character because they want funky attributes */
					int start = j, k;
					char *str;
					for (j++; j < new_idx && _T3_CHARDATA_TO_WIDTH(_t3_terminal_window->lines[i].data[j]) == 0; j++) {}
					if ((str = malloc(j - start)) != NULL) {
						for (k = start; k < j; k++)
							str[k - start] = _t3_terminal_window->lines[i].data[k] & _T3_CHAR_MASK;

						user_callback(str, j - start,
							_T3_CHARDATA_TO_WIDTH(_t3_terminal_window->lines[i].data[start]),
							chardata_to_attr(_t3_terminal_window->lines[i].data[start]));
						free(str);
					}
					if (j < new_idx)
						j--;
					continue;
				} else if (new_attrs != attrs) {
					set_attrs(new_attrs);
				}
			}
			if (attrs & _T3_ATTR_ACS) {
				t3_term_putc(alternate_chars[_t3_terminal_window->lines[i].data[j] & _T3_CHAR_MASK]);
			} else if (_t3_terminal_window->lines[i].data[j] & _T3_ATTR_FALLBACK_ACS) {
				t3_term_puts(get_default_acs(_t3_terminal_window->lines[i].data[j] & _T3_CHAR_MASK));
			} else {
				t3_term_putc(_t3_terminal_window->lines[i].data[j] & _T3_CHAR_MASK);
			}
		}

		/* Clear the terminal line if the new line is shorter than the old one. */
		if ((_t3_terminal_window->lines[i].width < old_data.width || j < new_idx) && width < _t3_terminal_window->width) {
			if (bce && (attrs & ~_T3_ATTR_FG_MASK) != 0)
				set_attrs(0);

			if (el != NULL) {
				_t3_putp(el);
			} else {
				int max = old_data.width < _t3_terminal_window->width ? old_data.width : _t3_terminal_window->width;
				for (; width < max; width++)
					t3_term_putc(' ');
			}
		}
		_t3_output_buffer_print();

done: /* Add empty statement to shut up compilers */ ;
	}

	set_attrs(0);

	if (civis == NULL) {
		show_cursor = new_show_cursor;
		if (!show_cursor)
			do_cup(_t3_terminal_window->height, _t3_terminal_window->width);
	} else {
		if (new_show_cursor != show_cursor) {
			/* If the cursor should now be visible, move it to the right position and
			   show it. Otherwise, it was already hidden at the start of this routine. */
			if (new_show_cursor) {
				do_cup(new_cursor_y, new_cursor_x);
				cursor_y = new_cursor_y;
				cursor_x = new_cursor_x;
				_t3_putp(cnorm);
			}
			show_cursor = new_show_cursor;
		} else if (show_cursor) {
			if (new_cursor_y == cursor_y && new_cursor_x == cursor_x && rc != NULL)
				_t3_putp(rc);
			else
				do_cup(new_cursor_y, new_cursor_x);
			cursor_y = new_cursor_y;
			cursor_x = new_cursor_x;
			_t3_putp(cnorm);
		}
	}

	fflush(_t3_putp_file);
}

/** Redraw the entire terminal from scratch. */
void t3_term_redraw(void) {
	/* The clear action destroys the current cursor position, so we make sure
	   that it has to be repositioned afterwards. Because we are redrawing, we
	   definately also want to ensure that the cursor is in the right place. */
	if (new_show_cursor && show_cursor)
		cursor_x = new_cursor_x + 1;
	set_attrs(0);
	_t3_putp(clear);
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
	uint8_t char_info;

	for (; n > 0; n -= bytes_read, str += bytes_read) {
		bytes_read = n;
		c = t3_unicode_get(str, &bytes_read);

		char_info = t3_unicode_get_info(c, INT_MAX);
		width = T3_UNICODE_INFO_TO_WIDTH(char_info);
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
	if (idx < 0 || idx > 255)
		return t3_false;
	return alternate_chars[idx] != 0;
}

/** Combine attributes, with priority.
    @param a The first set of attributes to combine (priority).
    @param b The second set of attributes to combine (no priority).
    @return The combined attributes.

    This function combines @p a and @p b, with the color attributes from @p a overriding
	the color attributes from @p b if both specify colors.
*/
t3_attr_t t3_term_combine_attrs(t3_attr_t a, t3_attr_t b) {
	t3_chardata_t result = b | (a & ~(T3_ATTR_FG_MASK | T3_ATTR_BG_MASK));
	if ((a & T3_ATTR_FG_MASK) != 0)
		result = ((result & ~(T3_ATTR_FG_MASK)) | (a & T3_ATTR_FG_MASK)) & ~ncv;
	if ((a & T3_ATTR_BG_MASK) != 0)
		result = ((result & ~(T3_ATTR_BG_MASK)) | (a & T3_ATTR_BG_MASK)) & ~ncv;
	/* If ncv prevented ACS, then use fallbacks instead. */
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
	return ncv;
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

	if (smul != NULL) caps->highlights |= T3_ATTR_UNDERLINE;
	if (bold != NULL) caps->highlights |= T3_ATTR_BOLD;
	if (rev != NULL) caps->highlights |= T3_ATTR_REVERSE;
	if (blink != NULL) caps->highlights |= T3_ATTR_BLINK;
	if (dim != NULL) caps->highlights |= T3_ATTR_DIM;
	if (smacs != NULL) caps->highlights |= T3_ATTR_ACS;

	caps->colors = colors;
	caps->pairs = pairs;

	caps->cap_flags = 0;
	if (setaf != NULL || setf != NULL) caps->cap_flags |= T3_TERM_CAP_FG;
	if (setab != NULL || setb != NULL) caps->cap_flags |= T3_TERM_CAP_BG;
	if (scp != NULL) caps->cap_flags |= T3_TERM_CAP_CP;
}

/** @internal
    @brief Convert attributes specified as ::t3_attr_t to the internal representation in a ::t3_chardata_t.
    @param attr The ::t3_attr_t to convert.
*/
t3_chardata_t _t3_term_attr_to_chardata(t3_attr_t attr) {
	return ((attr & ((1 << (T3_ATTR_COLOR_SHIFT + 8)) - 1)) << _T3_ATTR_SHIFT)
	|
		(((attr >> T3_ATTR_COLOR_SHIFT) & 0x1ff) > 8 ?
			_T3_ATTR_FG_DEFAULT :
			((attr >> T3_ATTR_COLOR_SHIFT) & 0xf) << _T3_ATTR_COLOR_SHIFT)
	|
		(((attr >> (T3_ATTR_COLOR_SHIFT + 9)) & 0x1ff) > 8 ?
			_T3_ATTR_BG_DEFAULT :
			((attr >> (T3_ATTR_COLOR_SHIFT + 9)) & 0xf) << (_T3_ATTR_COLOR_SHIFT + 4))
	;
}

static t3_attr_t chardata_to_attr(t3_chardata_t chardata) {
	return ((chardata & (0x7f << _T3_ATTR_SHIFT)) >> _T3_ATTR_SHIFT)
	|
		(((chardata >> _T3_ATTR_COLOR_SHIFT) & 0xf) > 8 ?
			T3_ATTR_FG_DEFAULT :
			((chardata >> _T3_ATTR_COLOR_SHIFT) & 0xf) << T3_ATTR_COLOR_SHIFT)
	|
		(((chardata >> (_T3_ATTR_COLOR_SHIFT + 4)) & 0xf) > 8 ?
			T3_ATTR_BG_DEFAULT :
			((chardata >> (_T3_ATTR_COLOR_SHIFT + 4)) & 0xf) << (T3_ATTR_COLOR_SHIFT + 9))
	;
}

/** @} */
