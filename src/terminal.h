#ifndef T3_TERMINAL_H
#define T3_TERMINAL_H

/** @file */

/** @defgroup t3window_term Terminal manipulation functions. */
/** @defgroup t3window_other Contants, data types and miscellaneous functions. */

#ifdef __cplusplus
extern "C" {
#endif

#include <limits.h>

/** @addtogroup t3window_other */
/** @{ */
/** @name Version information */
/*@{*/
/** The version of libt3window encoded as a single integer.

    The least significant 8 bits represent the patch level.
    The second 8 bits represent the minor version.
    The third 8 bits represent the major version.

	At runtime, the value of T3_WINDOW_VERSION can be retrieved by calling
	::t3_window_get_version.

    @internal
    The value 0 is an invalid value which should be replaced by the script
    that builds the release package.
*/
#define T3_WINDOW_VERSION 0

/* Although it doesn't make a lot of sense to put this function in either this
   file or in window.h, there is a good reason to put it in here: because
   window.h includes terminal.h, this function (and the macro) will always
   be available, regardless of which files the user includes. */
long t3_window_get_version(void);
/*@}*/

/** Boolean type that does not clash with C++ or C99 bool. */
typedef enum {t3_false, t3_true} t3_bool;

/** @typedef t3_chardata_t
    @brief Type to hold data about a single @c char, with attributes used for terminal display.
*/
#if INT_MAX < 2147483647L
typedef long t3_chardata_t;
#else
typedef int t3_chardata_t;
#endif

/** User callback type.
    The user callback is passed a pointer to the t3_chardata_t that is marked with
    ::T3_ATTR_USER, and the length of the string (in t3_chardata_t units, not display cells!).
*/
typedef void (*t3_attr_user_callback_t)(const t3_chardata_t *c, int length);

/** Bit number of the least significant attribute bit.

    By shifting a ::t3_chardata_t value to the right by T3_ATTR_SHIFT, the attributes
    will be in the least significant bits. This will leave ::T3_ATTR_USER in the
    least significant bit. This allows using the attribute bits as a number instead
    of a bitmask.
*/
#define T3_ATTR_SHIFT (CHAR_BIT + 2)
/** Bit number of the least significant color attribute bit. */
#define T3_ATTR_COLOR_SHIFT (T3_ATTR_SHIFT + 8)
/** Get the width in character cells encoded in a ::t3_chardata_t value. */
#define T3_CHARDATA_TO_WIDTH(_c) (((_c) >> CHAR_BIT) & 3)

/** Bitmask to leave only the attributes in a ::t3_chardata_t value. */
#define T3_ATTR_MASK ((t3_chardata_t) (~((1L << T3_ATTR_SHIFT) - 1)))
/** Bitmask to leave only the character in a ::t3_chardata_t value. */
#define T3_CHAR_MASK ((t3_chardata_t) ((1L << CHAR_BIT) - 1))

/** @name Attributes */
/*@{*/
/** Use callback for drawing the characters.

    When T3_ATTR_USER is set all other attribute bits are ignored. These can be used by
    the callback to determine the drawing style. The callback is set with ::t3_term_set_callback.
	Note that the callback is responsible for outputing the characters as well (using ::t3_term_putc).
*/
#define T3_ATTR_USER ((t3_chardata_t) (1L << T3_ATTR_SHIFT))
/** Draw characters with underlining. */
#define T3_ATTR_UNDERLINE ((t3_chardata_t) (1L << (T3_ATTR_SHIFT + 1)))
/** Draw characters with bold face/bright appearance. */
#define T3_ATTR_BOLD ((t3_chardata_t) (1L << (T3_ATTR_SHIFT + 2)))
/** Draw characters with reverse video. */
#define T3_ATTR_REVERSE ((t3_chardata_t) (1L << (T3_ATTR_SHIFT + 3)))
/** Draw characters blinking. */
#define T3_ATTR_BLINK ((t3_chardata_t) (1L << (T3_ATTR_SHIFT + 4)))
/** Draw characters with dim appearance. */
#define T3_ATTR_DIM ((t3_chardata_t) (1L << (T3_ATTR_SHIFT + 5)))
/** Draw characters with alternate character set (for line drawing etc). */
#define T3_ATTR_ACS ((t3_chardata_t) (1L << (T3_ATTR_SHIFT + 6)))

/** Foreground color unspecified. */
#define T3_ATTR_FG_UNSPEC ((t3_chardata_t) 0L)
/** Foreground color black. */
#define T3_ATTR_FG_BLACK ((t3_chardata_t) (1L << T3_ATTR_COLOR_SHIFT))
/** Foreground color red. */
#define T3_ATTR_FG_RED ((t3_chardata_t) (2L << T3_ATTR_COLOR_SHIFT))
/** Foreground color green. */
#define T3_ATTR_FG_GREEN ((t3_chardata_t) (3L << T3_ATTR_COLOR_SHIFT))
/** Foreground color yellow. */
#define T3_ATTR_FG_YELLOW ((t3_chardata_t) (4L << T3_ATTR_COLOR_SHIFT))
/** Foreground color blue. */
#define T3_ATTR_FG_BLUE ((t3_chardata_t) (5L << T3_ATTR_COLOR_SHIFT))
/** Foreground color magenta. */
#define T3_ATTR_FG_MAGENTA ((t3_chardata_t) (6L << T3_ATTR_COLOR_SHIFT))
/** Foreground color cyan. */
#define T3_ATTR_FG_CYAN ((t3_chardata_t) (7L << T3_ATTR_COLOR_SHIFT))
/** Foreground color white. */
#define T3_ATTR_FG_WHITE ((t3_chardata_t) (8L << T3_ATTR_COLOR_SHIFT))
/** Foreground color default. */
#define T3_ATTR_FG_DEFAULT ((t3_chardata_t) (9L << T3_ATTR_COLOR_SHIFT))

/** Background color unspecified. */
#define T3_ATTR_BG_UNSPEC ((t3_chardata_t) 0L)
/** Background color black. */
#define T3_ATTR_BG_BLACK ((t3_chardata_t) (1L << (T3_ATTR_COLOR_SHIFT + 4)))
/** Background color red. */
#define T3_ATTR_BG_RED ((t3_chardata_t) (2L << (T3_ATTR_COLOR_SHIFT + 4)))
/** Background color green. */
#define T3_ATTR_BG_GREEN ((t3_chardata_t) (3L << (T3_ATTR_COLOR_SHIFT + 4)))
/** Background color yellow. */
#define T3_ATTR_BG_YELLOW ((t3_chardata_t) (4L << (T3_ATTR_COLOR_SHIFT + 4)))
/** Background color blue. */
#define T3_ATTR_BG_BLUE ((t3_chardata_t) (5L << (T3_ATTR_COLOR_SHIFT + 4)))
/** Background color magenta. */
#define T3_ATTR_BG_MAGENTA ((t3_chardata_t) (6L << (T3_ATTR_COLOR_SHIFT + 4)))
/** Background color cyan. */
#define T3_ATTR_BG_CYAN ((t3_chardata_t) (7L << (T3_ATTR_COLOR_SHIFT + 4)))
/** Background color white. */
#define T3_ATTR_BG_WHITE ((t3_chardata_t) (8L << (T3_ATTR_COLOR_SHIFT + 4)))
/** Background color default. */
#define T3_ATTR_BG_DEFAULT ((t3_chardata_t) (9L << (T3_ATTR_COLOR_SHIFT + 4)))
/*@}*/

/** Alternate character set symbolic constants. */
enum {
	T3_ACS_TTEE = 'w', /**< Tee pointing down. */
	T3_ACS_RTEE = 'u', /**< Tee pointing left. */
	T3_ACS_LTEE = 't', /**< Tee pointing right. */
	T3_ACS_BTEE = 'v', /**< Tee pointing up. */
	T3_ACS_ULCORNER = 'l', /**< Upper left corner. */
	T3_ACS_URCORNER = 'k', /**< Upper right corner. */
	T3_ACS_LLCORNER = 'm', /**< Lower left corner. */
	T3_ACS_LRCORNER = 'j', /**< Lower right corner. */
	T3_ACS_HLINE = 'q', /**< Horizontal line. */
	T3_ACS_VLINE = 'x', /**< Vertical line. */
	T3_ACS_UARROW = '-', /**< Arrow pointing up. */
	T3_ACS_DARROW = '.', /**< Arrow pointing down. */
	T3_ACS_LARROW = ',', /**< Arrow pointing left. */
	T3_ACS_RARROW = '+', /**< Arrow pointing right. */
	T3_ACS_BOARD = 'h', /**< Board of squares. */
	T3_ACS_CKBOARD = 'a' /**< Checker board pattern (stipple). */
};

/** @name Error codes (T3 generic) */
/*@{*/
#ifndef T3_ERR_SUCCESS
/** Error code: success */
#define T3_ERR_SUCCESS 0
/** Error code: see @c errno. */
/* Use large negative value, such that we don't have to number each and
   every value. */
#define T3_ERR_ERRNO (-128)
/** Error code: end of file reached. */
#define T3_ERR_EOF (-127)
/** Error code: unkown error. */
#define T3_ERR_UNKNOWN (-126)
/** Error code: bad argument. */
#define T3_ERR_BAD_ARG (-125)
#endif
/*@}*/

/** @name Error codes (libt3window specific) */
/*@{*/
/** Error code: no information found for the terminal in the terminfo database. */
#define T3_ERR_TERMINFODB_NOT_FOUND (-64)
/** Error code: the file descriptor is a hard-copy terminal. */
#define T3_ERR_HARDCOPY_TERMINAL (-63)
/** Error code: the file descriptor is not a terminal. */
#define T3_ERR_NOT_A_TTY (-62)
/** Error code: a timeout occured. */
#define T3_ERR_TIMEOUT (-61)
/** Error code: terminal provides too limited possibilities for the library to function. */
#define T3_ERR_TERMINAL_TOO_LIMITED (-60)
/** Error code: could not retrieve information about the size of the terminal window. */
#define T3_ERR_NO_SIZE_INFO (-59)
/** Error code: input contains non-printable characters. */
#define T3_ERR_NONPRINT (-58)

const char *t3_window_strerror(int error);
/*@}*/

/** @} */

int t3_term_init(int fd);
void t3_term_restore(void);
int t3_term_get_keychar(int msec);
void t3_term_set_cursor(int y, int x);
void t3_term_hide_cursor(void);
void t3_term_show_cursor(void);
void t3_term_get_size(int *height, int *width);
t3_bool t3_term_resize(void);
void t3_term_update(void);
void t3_term_redraw(void);
void t3_term_set_attrs(t3_chardata_t new_attrs);
void t3_term_set_user_callback(t3_attr_user_callback_t callback);
int t3_term_get_keychar(int msec);
int t3_term_unget_keychar(int c);
void t3_term_putp(const char *str);
t3_bool t3_term_acs_available(int idx);

t3_chardata_t t3_term_combine_attrs(t3_chardata_t a, t3_chardata_t b);

int t3_term_strwidth(const char *str);

/** These are implemented in convert_output.c */
t3_bool t3_term_can_draw(const char *str, size_t str_len);
void t3_term_set_replacement_char(char c);
t3_bool t3_term_putc(char c);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
