#ifndef TERMINAL_H
#define TERMINAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <limits.h>

typedef enum {False, True} Bool;

#if INT_MAX < 2147483647L
typedef long CharData;
#else
typedef int CharData;
#endif

typedef void (*TermUserCallback)(CharData *c, int length);

Bool term_init(void);
void term_restore(void);
int term_get_keychar(int msec);

#define _ATTR_SHIFT (CHAR_BIT + 2)

#define ATTR_UNDERLINE (1 << _ATTR_SHIFT)
#define ATTR_BOLD (1 << (_ATTR_SHIFT + 1))
#define ATTR_STANDOUT (1 << (_ATTR_SHIFT + 2))
#define ATTR_REVERSE (1 << (_ATTR_SHIFT + 3))
#define ATTR_BLINK (1 << (_ATTR_SHIFT + 4))
#define ATTR_DIM (1 << (_ATTR_SHIFT + 5))
#define ATTR_ACS (1<< (_ATTR_SHIFT + 6))
#define ATTR_USER1 (1 << (_ATTR_SHIFT + 7))
#define _ATTR_COLOR_SHIFT (_ATTR_SHIFT + 8)
#define ATTR_FOREGROUND(_x) (((_x) & 0xf) << _ATTR_COLOR_SHIFT)
#define ATTR_BACKGROUND(_x) (((_x) & 0xf) << (_ATTR_COLOR_SHIFT + 4))

#define ATTR_MASK (~((1 << _ATTR_SHIFT) - 1))
#define ATTR_USER_MASK (ATTR_USER1)
#define CHAR_MASK ((1 << CHAR_BIT) - 1)

#define ATTR_FG_DEFAULT 0
#define ATTR_FG_BLACK (1 << _ATTR_COLOR_SHIFT)
#define ATTR_FG_RED (2 << _ATTR_COLOR_SHIFT)
#define ATTR_FG_GREEN (3 << _ATTR_COLOR_SHIFT)
#define ATTR_FG_YELLOW (4 << _ATTR_COLOR_SHIFT)
#define ATTR_FG_BLUE (5 << _ATTR_COLOR_SHIFT)
#define ATTR_FG_MAGENTA (6 << _ATTR_COLOR_SHIFT)
#define ATTR_FG_CYAN (7 << _ATTR_COLOR_SHIFT)
#define ATTR_FG_WHITE (8 << _ATTR_COLOR_SHIFT)

#define ATTR_BG_DEFAULT 0
#define ATTR_BG_BLACK (1 << (_ATTR_COLOR_SHIFT + 4))
#define ATTR_BG_RED (2 << (_ATTR_COLOR_SHIFT + 4))
#define ATTR_BG_GREEN (3 << (_ATTR_COLOR_SHIFT + 4))
#define ATTR_BG_YELLOW (4 << (_ATTR_COLOR_SHIFT + 4))
#define ATTR_BG_BLUE (5 << (_ATTR_COLOR_SHIFT + 4))
#define ATTR_BG_MAGENTA (6 << (_ATTR_COLOR_SHIFT + 4))
#define ATTR_BG_CYAN (7 << (_ATTR_COLOR_SHIFT + 4))
#define ATTR_BG_WHITE (8 << (_ATTR_COLOR_SHIFT + 4))

#define TERM_TTEE 'w'
#define TERM_RTEE 'u'
#define TERM_LTEE 't'
#define TERM_BTEE 'v'
#define TERM_ULCORNER 'l'
#define TERM_URCORNER 'k'
#define TERM_LLCORNER 'm'
#define TERM_LRCORNER 'j'
#define TERM_HLINE 'q'
#define TERM_VLINE 'x'

void term_set_cursor(int y, int x);
void term_hide_cursor(void);
void term_show_cursor(void);
void term_get_size(int *height, int *width);
Bool term_resize(void);
void term_refresh(void);
void term_set_attrs(CharData new_attrs);
void term_set_user_callback(TermUserCallback callback);
int term_get_keychar(int msec);
int term_unget_keychar(int c);
void term_putp(const char *str);

enum {
	KEY_ERROR = -1,
	KEY_TIMEOUT = -2
};

int term_strwidth(const char *str);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
