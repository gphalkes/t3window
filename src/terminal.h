#ifndef TERMINAL_H
#define TERMINAL_H

/** @file */

#ifdef __cplusplus
extern "C" {
#endif

#include <limits.h>

/** Boolean type that does not clash with C++ or C99 bool. */
typedef enum {False, True} Bool;

/** @typedef CharData
    @brief Type to hold data about a single @c char, with attributes used for terminal display.
*/
#if INT_MAX < 2147483647L
typedef long CharData;
#else
typedef int CharData;
#endif

/** User callback type.
    The user callback is passed a pointer to the CharData that is marked with ATTR_USER1,
    and the length of the string (in CharData units, not display cells!).
*/
typedef void (*TermUserCallback)(const CharData *c, int length);

#define _ATTR_SHIFT (CHAR_BIT + 2)

#define ATTR_UNDERLINE (1 << _ATTR_SHIFT)
#define ATTR_BOLD (1 << (_ATTR_SHIFT + 1))
#define ATTR_REVERSE (1 << (_ATTR_SHIFT + 2))
#define ATTR_BLINK (1 << (_ATTR_SHIFT + 3))
#define ATTR_DIM (1 << (_ATTR_SHIFT + 4))
#define ATTR_ACS (1<< (_ATTR_SHIFT + 5))
#define ATTR_USER1 (1 << (_ATTR_SHIFT + 6))

#define _ATTR_COLOR_SHIFT (_ATTR_SHIFT + 8)

#define ATTR_MASK (~((1 << _ATTR_SHIFT) - 1))
#define ATTR_USER_MASK (ATTR_USER1)
#define CHAR_MASK ((1 << CHAR_BIT) - 1)

#define ATTR_FG_UNSPEC 0
#define ATTR_FG_BLACK (1 << _ATTR_COLOR_SHIFT)
#define ATTR_FG_RED (2 << _ATTR_COLOR_SHIFT)
#define ATTR_FG_GREEN (3 << _ATTR_COLOR_SHIFT)
#define ATTR_FG_YELLOW (4 << _ATTR_COLOR_SHIFT)
#define ATTR_FG_BLUE (5 << _ATTR_COLOR_SHIFT)
#define ATTR_FG_MAGENTA (6 << _ATTR_COLOR_SHIFT)
#define ATTR_FG_CYAN (7 << _ATTR_COLOR_SHIFT)
#define ATTR_FG_WHITE (8 << _ATTR_COLOR_SHIFT)
#define ATTR_FG_DEFAULT (9 << _ATTR_COLOR_SHIFT)

#define ATTR_BG_UNSPEC 0
#define ATTR_BG_BLACK (1 << (_ATTR_COLOR_SHIFT + 4))
#define ATTR_BG_RED (2 << (_ATTR_COLOR_SHIFT + 4))
#define ATTR_BG_GREEN (3 << (_ATTR_COLOR_SHIFT + 4))
#define ATTR_BG_YELLOW (4 << (_ATTR_COLOR_SHIFT + 4))
#define ATTR_BG_BLUE (5 << (_ATTR_COLOR_SHIFT + 4))
#define ATTR_BG_MAGENTA (6 << (_ATTR_COLOR_SHIFT + 4))
#define ATTR_BG_CYAN (7 << (_ATTR_COLOR_SHIFT + 4))
#define ATTR_BG_WHITE (8 << (_ATTR_COLOR_SHIFT + 4))
#define ATTR_BG_DEFAULT (9 << (_ATTR_COLOR_SHIFT + 4))

/** Alternate character set symbolic constants. */
enum TermAcsConstants {
	TERM_TTEE = 'w', /**< Tee pointing down. */
	TERM_RTEE = 'u', /**< Tee pointing left. */
	TERM_LTEE = 't', /**< Tee pointing right. */
	TERM_BTEE = 'v', /**< Tee pointing up. */
	TERM_ULCORNER = 'l', /**< Upper left corner. */
	TERM_URCORNER = 'k', /**< Upper right corner. */
	TERM_LLCORNER = 'm', /**< Lower left corner. */
	TERM_LRCORNER = 'j', /**< Lower right corner. */
	TERM_HLINE = 'q', /**< Horizontal line. */
	TERM_VLINE = 'x', /**< Vertical line. */
	TERM_UARROW = '-', /**< Arrow pointing up. */
	TERM_DARROW = '.', /**< Arrow pointing down. */
	TERM_LARROW = ',', /**< Arrow pointing left. */
	TERM_RARROW = '+', /**< Arrow pointing right. */
	TERM_BOARD = 'h', /**< Board of squares. */
	TERM_CKBOARD = 'a' /**< Checker board pattern (stipple). */
};

/** Values returned by ::term_get_keychar. */
enum {
	KEY_ERROR = -1, /**< An error as reported through @c errno has been encountered. */
	KEY_TIMEOUT = -2, /**< A timeout has occured before a @c char could be read. */
	KEY_EOF = -3 /**< End of file condition occured on stdin. */
};

Bool term_init(void);
void term_restore(void);
int term_get_keychar(int msec);
void term_set_cursor(int y, int x);
void term_hide_cursor(void);
void term_show_cursor(void);
void term_get_size(int *height, int *width);
Bool term_resize(void);
void term_update(void);
void term_redraw(void);
void term_set_attrs(CharData new_attrs);
void term_set_user_callback(TermUserCallback callback);
int term_get_keychar(int msec);
int term_unget_keychar(int c);
void term_putp(const char *str);
Bool term_acs_available(int idx);

CharData term_combine_attrs(CharData a, CharData b);

int term_strwidth(const char *str);

Bool term_can_draw(const char *str, size_t str_len);
void term_set_replacement_char(char c);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
