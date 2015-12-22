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

#include <string.h>
#include <termios.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#if defined(HAS_WINSIZE_IOCTL) || defined(HAS_SIZE_IOCTL) || defined(HAS_TIOCLINUX)
#include <sys/ioctl.h>
#endif

#ifdef HAS_TIOCLINUX
#include <linux/tiocl.h>
#ifdef HAS_KDGKBENT
#include <linux/kd.h>
#include <linux/keyboard.h>
#endif
#endif

#include <transcript/transcript.h>

#include "window.h"
#include "internal.h"
#include "convert_output.h"
#include "log.h"

/* The curses header file defines too many symbols that get in the way of our
   own, so we have a separate C file which exports only those functions that
   we actually use. */
#include "curses_interface.h"

/** @internal
    @brief Wrapper for strcmp which converts the return value to boolean. */
#define streq(a,b) (strcmp((a), (b)) == 0)

/** @internal
    @brief Call a function to free a pointer and subsequently set it to NULL. */
#define CLEAR(x, func) do { if (x != NULL) { func(x); x = NULL; } } while (0)

static struct termios saved; /**< Terminal state as saved in ::t3_term_init */
static t3_bool initialised, /**< Boolean indicating whether the terminal has been initialised. */
	seqs_initialised, /**< Boolean indicating whether the terminal control sequences have been initialised. */
	transcript_init_done; /**< Boolean indicating whether @c transcript_init was called. */

/** Store whether the terminal is actually the screen program.

    If set, the terminal-capabilities detection will send extra pass-through
	markers before and after the cursor position request to ensure we get
	results. */
static t3_bool terminal_is_screen;


static char *smcup, /**< Terminal control string: start cursor positioning mode. */
	*rmcup; /**< Terminal control string: stop cursor positioning mode. */


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

/** Start cursor positioning mode.

    If @c smcup is not available for the terminal, a simple @c _t3_clear is used instead.
*/
static void do_smcup(void) {
	if (smcup != NULL) {
		_t3_putp(smcup);
		return;
	}
	if (_t3_clear != NULL) {
		_t3_putp(_t3_clear);
		return;
	}
}

/** Stop cursor positioning mode.

    If @c rmcup is not available for the terminal, a @c _t3_clear is used instead,
    followed by positioning the cursor in the bottom left corner.
*/
static void do_rmcup(void) {
	if (rmcup != NULL) {
		_t3_putp(rmcup);
		return;
	}
	if (_t3_clear != NULL) {
		_t3_putp(_t3_clear);
		_t3_do_cup(_t3_lines - 1, 0);
		return;
	}
}

#define SET_CHARACTER(_idx, _utf, _ascii) do { \
	if (t3_term_can_draw((_utf), strlen(_utf))) \
		_t3_default_alternate_chars[_idx] = _utf; \
	else \
		_t3_default_alternate_chars[_idx] = _ascii; \
} while (0)

/** Fill the defaults table with fall-back characters for the alternate character set.
    @param table The table to fill. */
void _t3_set_alternate_chars_defaults(void) {
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

/** Detect to what extent a terminal description matches the ANSI terminal standard.

    For (partially) ANSI compliant terminals optimization of the output can be done
    such that fewer characters need to be sent to the terminal than by using @c sgr.
*/
static void detect_ansi(void) {
	t3_attr_t non_existent = 0;

	if (_t3_op != NULL && (streq(_t3_op, "\033[39;49m") || streq(_t3_op, "\033[49;39m"))) {
		if (_t3_setaf != NULL && (streq(_t3_setaf, "\033[3%p1%dm") ||
				streq(_t3_setaf, "\033[%?%p1%{8}%<%t3%p1%d%e%p1%{16}%<%t9%p1%{8}%-%d%e38;5;%p1%d%;m")) &&
				_t3_setab != NULL && (streq(_t3_setab, "\033[4%p1%dm") ||
				streq(_t3_setab, "\033[%?%p1%{8}%<%t4%p1%d%e%p1%{16}%<%t10%p1%{8}%-%d%e48;5;%p1%d%;m")))
			_t3_ansi_attrs |= T3_ATTR_FG_MASK | T3_ATTR_BG_MASK;
	}
	if (_t3_smul != NULL && _t3_rmul != NULL && streq(_t3_smul, "\033[4m") && streq(_t3_rmul, "\033[24m"))
		_t3_ansi_attrs |= T3_ATTR_UNDERLINE;
	if (_t3_smacs != NULL && _t3_rmacs != NULL && streq(_t3_smacs, "\033[11m") && streq(_t3_rmacs, "\033[10m"))
		_t3_ansi_attrs |= T3_ATTR_ACS;

	/* So far, we have been able to check that the "exit mode" operation was ANSI compatible as well.
	   However, for bold, dim, reverse and blink we can't check this, so we will only accept them
	   as attributes if the terminal uses ANSI colors, and they all match in as far as they exist.
	*/
	if ((_t3_ansi_attrs & (T3_ATTR_FG_MASK | T3_ATTR_BG_MASK)) == 0 || (_t3_ansi_attrs & (T3_ATTR_UNDERLINE | T3_ATTR_ACS)) == 0)
		return;

	if (_t3_rev != NULL) {
		if (streq(_t3_rev, "\033[7m")) {
			/* On many terminals smso is also reverse video. If it is, we can
			   verify that rmso removes the reverse video. Otherwise, we just
			   assume that if rev matches ANSI reverse video, the inverse ANSI
			   sequence also works. */
			char *smso = get_ti_string("smso");
			if (streq(smso, _t3_rev)) {
				char *rmso = get_ti_string("rmso");
				if (streq(rmso, "\033[27m"))
					_t3_ansi_attrs |= T3_ATTR_REVERSE;
				free(rmso);
			} else {
				_t3_ansi_attrs |= T3_ATTR_REVERSE;
			}
			free(smso);
		}
	} else {
		non_existent |= T3_ATTR_REVERSE;
	}

	if (_t3_bold != NULL) {
		if (streq(_t3_bold, "\033[1m"))
			_t3_ansi_attrs |= T3_ATTR_BOLD;
	} else {
		non_existent |= T3_ATTR_BOLD;
	}

	if (_t3_dim != NULL) {
		if (streq(_t3_dim, "\033[2m"))
			_t3_ansi_attrs |= T3_ATTR_DIM;
	} else {
		non_existent |= T3_ATTR_DIM;
	}

	if (_t3_blink != NULL) {
		if (streq(_t3_blink, "\033[5m"))
			_t3_ansi_attrs |= T3_ATTR_BLINK;
	} else {
		non_existent |= T3_ATTR_BLINK;
	}

	/* Only accept as ANSI if all attributes accept ACS are either non specified or ANSI. */
	if (((non_existent | _t3_ansi_attrs) & (T3_ATTR_REVERSE | T3_ATTR_BOLD | T3_ATTR_DIM | T3_ATTR_BLINK)) !=
			(T3_ATTR_REVERSE | T3_ATTR_BOLD | T3_ATTR_DIM | T3_ATTR_BLINK))
		_t3_ansi_attrs &= ~(T3_ATTR_REVERSE | T3_ATTR_BOLD | T3_ATTR_DIM | T3_ATTR_BLINK);
}

/** Send a string for measuring it's on screen width.

    This function moves the cursor to the top left position, writes the test
    string, followed by a cursor position report request.
*/
static void send_test_string(const char *str) {
	/* Move cursor to the begining of the line. Use cup if hpa is not available.
	   Also, make sure we use line 1, iso line 0, because xterm uses \e[1;<digit>R for
	   some combinations of F3 with modifiers and high-numbered function keys. :-( */
	if (_t3_hpa != NULL)
		_t3_putp(_t3_tparm(_t3_hpa, 1, 0));
	else
		_t3_do_cup(1, 0);

	fputs(str, _t3_putp_file);
	/* Send ANSI cursor reporting string. */
	if (terminal_is_screen)
		_t3_putp("\033P\033[6n\033\\");
	else
		_t3_putp("\033[6n");
}

/** Check if a terminfo string equals another string.

    Terminfo strings may contain timing/padding information, so a simple string
    compare may not result in a correct result. This function ignores the
    timing/padding information.
*/
static int ti_streq(const char *str, const char *reset_string) {
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

/** Check if a terminfo string resets all attributes.

    This function checks @p str against @c sgr0 and the ANSI reset attribute
    strings.
*/
static int isreset(const char *str) {
	return (_t3_sgr0 != NULL && streq(str, _t3_sgr0)) || ti_streq(str, "\033[m") || ti_streq(str, "\033[0m");
}

/** Initialize the different control sequences that are used by libt3window. */
static int init_sequences(const char *term) {
	int error, ncv_int;
	char *acsc;
	char *enacs;

	if ((error = _t3_setupterm(term, _t3_terminal_out_fd)) != 0) {
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
	if ((_t3_clear = get_ti_string("clear")) == NULL)
		return T3_ERR_TERMINAL_TOO_LIMITED;

	if ((_t3_cup = get_ti_string("cup")) == NULL) {
		if ((_t3_hpa = get_ti_string("hpa")) == NULL || ((_t3_vpa = get_ti_string("vpa")) == NULL))
			return T3_ERR_TERMINAL_TOO_LIMITED;
	}
	if (_t3_hpa == NULL)
		_t3_hpa = get_ti_string("hpa");

	_t3_sgr = get_ti_string("sgr");
	_t3_sgr0 = get_ti_string("sgr0");

	if ((_t3_smul = get_ti_string("smul")) != NULL) {
		if ((_t3_rmul = get_ti_string("rmul")) == NULL || isreset(_t3_rmul))
			_t3_reset_required_mask |= T3_ATTR_UNDERLINE;
	}
	_t3_bold = get_ti_string("bold");
	_t3_rev = get_ti_string("rev");
	_t3_blink = get_ti_string("blink");
	_t3_dim = get_ti_string("dim");
	_t3_smacs = get_ti_string("smacs");

	if (_t3_smacs != NULL && ((_t3_rmacs = get_ti_string("rmacs")) == NULL || isreset(_t3_rmacs)))
		_t3_reset_required_mask |= T3_ATTR_ACS;

	/* If rmul and rmacs are the same, there is a good chance it simply
	   resets everything. */
	if (_t3_rmul != NULL && _t3_rmacs != NULL && streq(_t3_rmul, _t3_rmacs))
		_t3_reset_required_mask |= T3_ATTR_UNDERLINE | T3_ATTR_ACS;

	if ((_t3_setaf = get_ti_string("setaf")) == NULL)
		_t3_setf = get_ti_string("setf");

	if ((_t3_setab = get_ti_string("setab")) == NULL)
		_t3_setb = get_ti_string("setb");

	if (_t3_setaf == NULL && _t3_setf == NULL && _t3_setab == NULL && _t3_setb == NULL) {
		if ((_t3_scp = get_ti_string("scp")) != NULL) {
			_t3_colors = _t3_tigetnum("colors");
			_t3_pairs = _t3_tigetnum("pairs");
		}
	} else {
		_t3_colors = _t3_tigetnum("colors");
		_t3_pairs = _t3_tigetnum("pairs");
	}

	if (_t3_colors < 0) _t3_colors = 0;
	if (_t3_pairs < 0) _t3_pairs = 0;

	_t3_op = get_ti_string("op");

	detect_ansi();

	/* If sgr0 and sgr are not defined, don't go into modes in _t3_reset_required_mask. */
	if (_t3_sgr0 == NULL && _t3_sgr == NULL) {
		_t3_reset_required_mask = 0;
		_t3_rev = NULL;
		_t3_bold = NULL;
		_t3_blink = NULL;
		_t3_dim = NULL;
		if (_t3_rmul == NULL) CLEAR(_t3_smul, free);
		if (_t3_rmacs == NULL) CLEAR(_t3_smacs, free);
	}

	_t3_bce = _t3_tigetflag("bce");
	if ((_t3_el = get_ti_string("el")) == NULL)
		_t3_bce = t3_true;

	if ((_t3_sc = get_ti_string("sc")) != NULL && (_t3_rc = get_ti_string("rc")) == NULL)
		CLEAR(_t3_sc, free);

	_t3_civis = get_ti_string("civis");
	_t3_cnorm = get_ti_string("cnorm");

	if ((acsc = get_ti_string("acsc")) != NULL) {
		if (_t3_sgr != NULL || _t3_smacs != NULL) {
			size_t i;
			for (i = 0; i < strlen(acsc); i += 2)
				_t3_alternate_chars[(unsigned int) acsc[i]] = acsc[i + 1];
		}
		free(acsc);
	}

	ncv_int = _t3_tigetnum("ncv");
	if (ncv_int >= 0) {
		if (ncv_int & (1<<1)) _t3_ncv |= T3_ATTR_UNDERLINE;
		if (ncv_int & (1<<2)) _t3_ncv |= T3_ATTR_REVERSE;
		if (ncv_int & (1<<3)) _t3_ncv |= T3_ATTR_BLINK;
		if (ncv_int & (1<<4)) _t3_ncv |= T3_ATTR_DIM;
		if (ncv_int & (1<<5)) _t3_ncv |= T3_ATTR_BOLD;
		if (ncv_int & (1<<8)) _t3_ncv |= T3_ATTR_ACS;
	}

	/* Enable alternate character set if required by terminal. */
	if ((enacs = get_ti_string("enacs")) != NULL) {
		_t3_putp(enacs);
		free(enacs);
	}

	return T3_ERR_SUCCESS;
}

/** @internal
    @brief Detect which terminal hacks should be applied. */
static void detect_terminal_hacks(const char *term) {
	(void) term;
#ifdef HAS_TIOCLINUX
{
	char cmd = TIOCL_GETSHIFTSTATE;
	if (ioctl(_t3_terminal_in_fd, TIOCLINUX, &cmd) == 0) {
#ifdef HAS_KDGKBENT
		struct kbentry arg = { (1 << KG_SHIFT), 106, 0 };
		int success = 0;
		int modified_key_action;

		success |= ioctl(_t3_terminal_in_fd, KDGKBENT, &arg);
		modified_key_action = arg.kb_value;
		arg.kb_table = 0;
		arg.kb_index = 106;
		success |= ioctl(_t3_terminal_in_fd, KDGKBENT, &arg);
		if (success == 0 && arg.kb_value != modified_key_action)
			return;
#endif
		_t3_modifier_hack = _T3_MODHACK_LINUX;
	}
}
#endif
}

/** @internal Check whether a string starts with a specific option.
    @param str The string to check.
    @param opt The option to look for.
    @return A boolean indicating whether the option was encountered.
*/
static t3_bool check_opt(const char *str, const char *opt) {
	size_t len = strlen(opt);
	return strncmp(str, opt, len) == 0 && (str[len] == ' ' || str[len] == 0);
}

/** @internal Check whether a string starts with a specific numerical option.
    @param str The string to check.
    @param opt The option to look for (including the '=' character.
    @param result A pointer to where to store the result.
    @return A boolean indicating whether the option was encountered and has a
        valid value.
*/
static t3_bool check_num_opt(const char *str, const char *opt, int *result) {
	size_t len = strlen(opt);
	long value;
	char *endptr;

	if (!(strncmp(str, opt, len) == 0))
		return t3_false;

	errno = 0;
	value = strtol(str + len, &endptr, 0);
	if (*endptr != 0 && *endptr != ' ')
		return t3_false;

	if (value > INT_MAX || value < INT_MIN || ((value == LONG_MAX || value == LONG_MIN) && errno == ERANGE))
		return t3_false;
	*result = value;
	return t3_true;
}

/** @internal Override the number of colors reported by terminfo.
    @param colors The number of colors to use, or @c 0 to use the terminfo setting.
    @param pairs The number of pairs to use, or @c 0 to use the terminfo setting.

    Many terminal emulators these days support XTerm 256 color mode. However,
    they use their old TERM setting, rather than the xxx-256color TERM setting
    that the terminfo database expects. Therefore, we provide an interface to
    override the values retrieved from the terminfo database.
*/
static void override_colors(int colors, int pairs) {
	if (colors <= 0)
		_t3_colors = _t3_tigetnum("colors");
	else if (colors <= 256)
		_t3_colors = colors;

	if (pairs <= 0)
		_t3_pairs = _t3_tigetnum("pairs");
	else
		_t3_pairs = pairs;
}

/** Read the T3WINDOW_OPTS environment variable and parse its contents. */
static void integrate_environment(void) {
	char *opts = getenv("T3WINDOW_OPTS");
	int value;

	if (opts == NULL)
		return;

	while (*opts != 0) {
		while (*opts == ' ') opts++;

		if (check_opt(opts, "acs=ascii")) {
			_t3_acs_override = _T3_ACS_ASCII;
		} else if (check_opt(opts, "acs=utf8")) {
			_t3_acs_override = _T3_ACS_UTF8;
		} else if (check_opt(opts, "acs=auto")) {
			_t3_acs_override = _T3_ACS_AUTO;
		} else if (check_num_opt(opts, "colors=", &value)) {
			override_colors(value, _t3_pairs);
		} else if (check_num_opt(opts, "pairs=", &value)) {
			override_colors(_t3_colors, value);
		} else if (check_opt(opts, "ansi=off")) {
			_t3_ansi_attrs = 0;
		}
		while (*opts != 0 && *opts != ' ') opts++;
	}
}

/** Initialize the terminal.
    @param fd The file descriptor of the terminal or -1 for default.
    @param term The name of the terminal, or @c NULL to use the @c TERM environment variable.
    @return One of: ::T3_ERR_SUCCESS, ::T3_ERR_NOT_A_TTY, ::T3_ERR_ERRNO, ::T3_ERR_HARDCOPY_TERMINAL,
        ::T3_ERR_TERMINFODB_NOT_FOUND, ::T3_ERR_UNKNOWN, ::T3_ERR_TERMINAL_TOO_LIMITED,
        ::T3_ERR_NO_SIZE_INFO.

    This function depends on the correct setting of the @c LC_CTYPE property
    by the @c setlocale function. Therefore, the @c setlocale function should
    be called before this function.

    If standard input/output should be used as the terminal, -1 should be
    passed. When calling t3_term_init multiple times (necessary when
    t3_term_restore was called), only the first successful call will inspect
    the @p fd and @p term parameters.

    The terminal is initialized to raw mode such that echo is disabled
    (characters typed are not shown), control characters are passed to the
    program (i.e. ctrl-c will result in a character rather than a TERM signal)
    and generally all characters typed are passed to the program immediately
    and with a minimum of pre-processing.
*/
int t3_term_init(int fd, const char *term) {
	static t3_bool detection_done, only_once;
#if defined(HAS_WINSIZE_IOCTL)
	struct winsize wsz;
#elif defined(HAS_SIZE_IOCTL)
	struct ttysize wsz;
#endif
	struct termios new_params;

	init_log();

	if (initialised)
		return T3_ERR_SUCCESS;

	if (_t3_putp_file == NULL) {
		/* We dup the fd, because when we use fclose on the FILE that we fdopen
		   it will close the underlying fd. This should not however close the
		   fd we have been passed or STDOUT. */
		if (fd >= 0) {
			if (!isatty(fd))
				return T3_ERR_NOT_A_TTY;
			if ((_t3_terminal_in_fd = _t3_terminal_out_fd = dup(fd)) == -1)
				return T3_ERR_ERRNO;
		} else if (_t3_putp_file == NULL) {
			if (!isatty(STDOUT_FILENO) || !isatty(STDIN_FILENO))
				return T3_ERR_NOT_A_TTY;
			if ((_t3_terminal_out_fd = dup(STDOUT_FILENO)) == -1)
				return T3_ERR_ERRNO;
			_t3_terminal_in_fd = STDIN_FILENO;
		}

		if ((_t3_putp_file = fdopen(_t3_terminal_out_fd, "w")) == NULL) {
			close(_t3_terminal_out_fd);
			return T3_ERR_ERRNO;
		}

		detect_terminal_hacks(term);

		FD_ZERO(&_t3_inset);
		FD_SET(_t3_terminal_in_fd, &_t3_inset);
	}

	if (!seqs_initialised) {
		int result;
		if ((result = init_sequences(term)) != T3_ERR_SUCCESS)
			return result;

		integrate_environment();
		seqs_initialised = t3_true;
	}

	/* Get terminal size. First try ioctl, then environment, then terminfo. */
#if defined(HAS_WINSIZE_IOCTL)
	if (ioctl(_t3_terminal_out_fd, TIOCGWINSZ, &wsz) == 0) {
		_t3_lines = wsz.ws_row;
		_t3_columns = wsz.ws_col;
	} else
#elif defined(HAS_SIZE_IOCTL)
	if (ioctl(_t3_terminal_out_fd, TIOCGSIZE, &wsz) == 0) {
		_t3_lines = wsz.ts_lines;
		_t3_columns = wsz.ts_cols;
	} else
#endif
	{
		char *lines_env = getenv("LINES");
		char *columns_env = getenv("COLUMNS");
		if (lines_env == NULL || columns_env == NULL || (_t3_lines = atoi(lines_env)) == 0 || (_t3_columns = atoi(columns_env)) == 0) {
			if ((_t3_lines = _t3_tigetnum("lines")) < 0 || (_t3_columns = _t3_tigetnum("columns")) < 0)
				return T3_ERR_NO_SIZE_INFO;
		}
	}

	if (!transcript_init_done) {
		transcript_init();
		transcript_init_done = t3_true;
	}
	if (!detection_done) {
		const char *charset = transcript_get_codeset();
		strncpy(_t3_current_charset, charset, sizeof(_t3_current_charset) - 1);
		_t3_current_charset[sizeof(_t3_current_charset) - 1] = '\0';
		if (!_t3_init_output_converter(_t3_current_charset))
			return T3_ERR_CHARSET_ERROR;

		_t3_set_alternate_chars_defaults();
	}

	/* Create or resize terminal window */
	if (_t3_terminal_window == NULL) {
		if ((_t3_terminal_window = t3_win_new(NULL, _t3_lines, _t3_columns, 0, 0, 0)) == NULL)
			return T3_ERR_ERRNO;
		if ((_t3_scratch_terminal_window = t3_win_new(NULL, _t3_lines, _t3_columns, 0, 0, 0)) == NULL)
			return T3_ERR_ERRNO;
		/* Remove terminal window from the window stack. */
		_t3_remove_window(_t3_terminal_window);
		_t3_remove_window(_t3_scratch_terminal_window);
	} else {
		if (!t3_win_resize(_t3_terminal_window, _t3_lines, _t3_columns))
			return T3_ERR_ERRNO;
		if (!t3_win_resize(_t3_scratch_terminal_window, _t3_lines, _t3_columns))
			return T3_ERR_ERRNO;
	}

	if (tcgetattr(_t3_terminal_in_fd, &saved) < 0)
		return T3_ERR_ERRNO;

	new_params = saved;
	new_params.c_iflag &= ~(IXON | IXOFF | IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
	new_params.c_lflag &= ~(ISIG | ICANON | ECHO);
	new_params.c_oflag &= ~OPOST;
	new_params.c_cflag &= ~(CSIZE | PARENB);
	new_params.c_cflag |= CS8;
	new_params.c_cc[VMIN] = 1;

	if (tcsetattr(_t3_terminal_in_fd, TCSADRAIN, &new_params) < 0)
		return T3_ERR_ERRNO;

	/* Start cursor positioning mode. */
	do_smcup();

	if (!detection_done) {
		detection_done = t3_true;
		/* Make sure we use line 1, iso line 0, because xterm uses \e[1;<digit>R for
		   some combinations of F3 with modifiers and high-numbered function keys. :-( */
		if (_t3_hpa != NULL) {
			if (_t3_vpa != NULL)
				_t3_putp(_t3_tparm(_t3_vpa, 1, 1));
			else
				_t3_do_cup(1, 0);
		}

		if (term != NULL || (term = getenv("TERM")) != NULL)
			if (strcmp(term , "screen") == 0)
				terminal_is_screen = t3_true;

		#define GENERATE_STRINGS
		#include "terminal_detection.h"
		#undef GENERATE_STRINGS
		_t3_putp(_t3_clear);
		fflush(_t3_putp_file);
	}

	/* Make sure the cursor is visible */
	if (_t3_show_cursor)
		_t3_putp(_t3_cnorm);
	else
		_t3_putp(_t3_civis);
	_t3_do_cup(_t3_cursor_y, _t3_cursor_x);

	/* Set the attributes of the terminal to a known value. */
	_t3_set_attrs(0);

	_t3_init_output_buffer();

	if (!only_once) {
		_t3_init_attr_map();
		only_once = t3_true;
	}

	initialised = t3_true;
	return T3_ERR_SUCCESS;
}

/** Restore terminal state (de-initialize). */
void t3_term_restore(void) {
	if (initialised) {
		/* Ensure complete repaint of the terminal on re-init (if required) */
		t3_win_set_paint(_t3_terminal_window, 0, 0);
		t3_win_clrtobot(_t3_terminal_window);
		if (seqs_initialised) {
			/* Restore cursor to visible state. */
			if (!_t3_show_cursor)
				_t3_putp(_t3_cnorm);
			/* Make sure attributes are reset */
			_t3_set_attrs(0);
			_t3_putp(_t3_clear);
			_t3_attrs = 0;
			do_rmcup();
			fflush(_t3_putp_file);
		}
		tcsetattr(_t3_terminal_in_fd, TCSADRAIN, &saved);
		initialised = t3_false;
	}
}

/** Free all memory allocated by libt3window.

    This function releases all memory allocated by libt3window, and allows
    libt3window to be initialized for a new terminal.
*/
void t3_term_deinit(void) {
	t3_term_restore();
	CLEAR(_t3_putp_file, fclose);

	seqs_initialised = t3_false;
	CLEAR(smcup, free);
	CLEAR(rmcup, free);
	CLEAR(_t3_clear, free);
	CLEAR(_t3_cup, free);
	CLEAR(_t3_hpa, free);
	CLEAR(_t3_vpa, free);
	CLEAR(_t3_sgr, free);
	CLEAR(_t3_sgr0, free);
	CLEAR(_t3_smul, free);
	CLEAR(_t3_rmul, free);
	CLEAR(_t3_bold, free);
	CLEAR(_t3_rev, free);
	CLEAR(_t3_blink, free);
	CLEAR(_t3_dim, free);
	CLEAR(_t3_smacs, free);
	CLEAR(_t3_rmacs, free);
	CLEAR(_t3_setaf, free);
	CLEAR(_t3_setf, free);
	CLEAR(_t3_setab, free);
	CLEAR(_t3_setb, free);
	CLEAR(_t3_scp, free);
	CLEAR(_t3_op, free);
	CLEAR(_t3_el, free);
	CLEAR(_t3_sc, free);
	CLEAR(_t3_rc, free);
	CLEAR(_t3_civis, free);
	CLEAR(_t3_cnorm, free);

	CLEAR(_t3_terminal_window, t3_win_del);
	CLEAR(_t3_scratch_terminal_window, t3_win_del);
	_t3_free_output_buffer();
	_t3_free_attr_map();
	if (transcript_init_done) {
		transcript_finalize();
		transcript_init_done = t3_false;
	}
}

/** Disable the ANSI terminal control sequence optimization.
    @deprecated This function does nothing anymore. To disable the ANSI
    optimization, set the environment variable @c T3WINDOW_OPTS=ansi=off.
*/
void t3_term_disable_ansi_optimization(void) {}
