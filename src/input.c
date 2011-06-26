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
#include <string.h>
#include <errno.h>
#include <unistd.h>
#ifdef HAS_SELECT_H
#include <sys/select.h>
#else
#include <sys/time.h>
#include <sys/types.h>
#endif
#include <transcript/transcript.h>
#include <t3unicode/unicode.h>
#include "window.h"
#include "internal.h"

/** @addtogroup t3window_term */
/** @{ */

/** @internal States for parsing cursor position reports. */
typedef enum {
	STATE_INITIAL,
	STATE_ESC_SEEN,
	STATE_ROW,
	STATE_COLUMN
} detection_state_t;


/** Boolean indicating whether the library is currently detecting the terminal capabilities. */
static t3_bool detecting_terminal_capabilities = t3_true;

static int last_key = -1, /**< Last keychar returned from ::t3_term_get_keychar. Used in ::t3_term_unget_keychar. */
	stored_key = -1; /**< Location for storing "ungot" keys in ::t3_term_unget_keychar. */

fd_set _t3_inset; /**< File-descriptor set used for select in ::t3_term_get_keychar. */

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
				strcpy(_t3_current_charset, "UTF-8");
				_t3_detection_needs_finishing = t3_true;
			} else if (_t3_term_double_width != -1 || _t3_term_combining != -1) {
				_t3_detection_needs_finishing = t3_true;
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
				strcpy(_t3_current_charset, "Shift_JIS");
				_t3_detection_needs_finishing = t3_true;
			}
			break;
		case _T3_TERM_GB18030:
			if (!transcript_equal(current_encoding, "gb18030")) {
				strcpy(_t3_current_charset, "GB18030");
				_t3_detection_needs_finishing = t3_true;
			} else if (_t3_term_double_width != -1 || _t3_term_combining != -1) {
				_t3_detection_needs_finishing = t3_true;
			}
			break;
		default:
			break;
	}

	if (set_ascii) {
		strcpy(_t3_current_charset, "ASCII");
		_t3_detection_needs_finishing = t3_true;
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
		ssize_t retval = read(_t3_terminal_fd, &c, 1);
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
		_inset = _t3_inset;
		if (msec > 0) {
			timeout.tv_sec = msec / 1000;
			timeout.tv_usec = (msec % 1000) * 1000;
		}

		retval = select(_t3_terminal_fd + 1, &_inset, NULL, NULL, msec > 0 ? &timeout : NULL);

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

/** @} */
