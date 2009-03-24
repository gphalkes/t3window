#ifndef TERMINAL_H
#define TERMINAL_H

#include <limits.h>

typedef enum { false, true } bool;
//FIXME: make sure that the base type is the correct size to store all the attributes
typedef int CharData;
typedef void (*TermUserCallback)(CharData *c, int length);

bool term_init(void);
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

#define TERM_TTEE (ATTR_ACS | 'w')
#define TERM_RTEE (ATTR_ACS | 'u')
#define TERM_LTEE (ATTR_ACS | 't')
#define TERM_BTEE (ATTR_ACS | 'v')
#define TERM_ULCORNER (ATTR_ACS | 'l')
#define TERM_URCORNER (ATTR_ACS | 'k')
#define TERM_LLCORNER (ATTR_ACS | 'm')
#define TERM_LRCORNER (ATTR_ACS | 'j')
#define TERM_HLINE (ATTR_ACS | 'q')
#define TERM_VLINE (ATTR_ACS | 'x')

void term_set_cursor(int y, int x);
void term_hide_cursor(void);
void term_show_cursor(void);
void term_get_size(int *height, int *width);
bool term_resize(void);
void term_refresh(void);
void term_set_attrs(CharData new_attrs);
void term_set_user_callback(TermUserCallback callback);

enum {
	KEY_ERROR = -1,
	KEY_TIMEOUT = -2
};
/* FIXME:
- line drawing
- add one char
*/
#endif
