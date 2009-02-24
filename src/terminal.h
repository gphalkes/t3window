#ifndef TERMINAL_H
#define TERMINAL_H

#include <limits.h>

typedef enum { false, true } Bool;

Bool term_init(void);
void term_restore(void);
int term_get_keychar(int msec);

#define ATTR_UNDERLINE (1<<(CHAR_BIT + 2))
#define ATTR_BOLD (1<<(CHAR_BIT + 3))
#define ATTR_STANDOUT (1<<(CHAR_BIT + 4))
#define ATTR_REVERSE (1<<(CHAR_BIT + 5))
#define ATTR_BLINK (1<<(CHAR_BIT + 6))
#define ATTR_DIM (1<<(CHAR_BIT + 7))
#define ATTR_USER1(_x) (1<<(CHAR_BIT + 8))
#define _ATTR_COLOR_SHIFT 9
#define ATTR_FOREGROUND(_x) (((_x) & 0xf) << (CHAR_BIT + _ATTR_COLOR_SHIFT))
#define ATTR_BACKGROUND(_x) (((_x) & 0xf) << (CHAR_BIT + _ATTR_COLOR_SHIFT + 4))

#define ATTR_FG_DEFAULT 0
#define ATTR_FG_BLACK (1 << (CHAR_BIT + _ATTR_COLOR_SHIFT))
#define ATTR_FG_RED (2 << (CHAR_BIT + _ATTR_COLOR_SHIFT))
#define ATTR_FG_GREEN (3 << (CHAR_BIT + _ATTR_COLOR_SHIFT))
#define ATTR_FG_YELLOW (4 << (CHAR_BIT + _ATTR_COLOR_SHIFT))
#define ATTR_FG_BLUE (5 << (CHAR_BIT + _ATTR_COLOR_SHIFT))
#define ATTR_FG_MAGENTA (6 << (CHAR_BIT + _ATTR_COLOR_SHIFT))
#define ATTR_FG_CYAN (7 << (CHAR_BIT + _ATTR_COLOR_SHIFT))
#define ATTR_FG_WHITE (8 << (CHAR_BIT + _ATTR_COLOR_SHIFT))

#define ATTR_BG_DEFAULT 0
#define ATTR_BG_BLACK (1 << (CHAR_BIT + _ATTR_COLOR_SHIFT + 4))
#define ATTR_BG_RED (2 << (CHAR_BIT + _ATTR_COLOR_SHIFT + 4))
#define ATTR_BG_GREEN (3 << (CHAR_BIT + _ATTR_COLOR_SHIFT + 4))
#define ATTR_BG_YELLOW (4 << (CHAR_BIT + _ATTR_COLOR_SHIFT + 4))
#define ATTR_BG_BLUE (5 << (CHAR_BIT + _ATTR_COLOR_SHIFT + 4))
#define ATTR_BG_MAGENTA (6 << (CHAR_BIT + _ATTR_COLOR_SHIFT + 4))
#define ATTR_BG_CYAN (7 << (CHAR_BIT + _ATTR_COLOR_SHIFT + 4))
#define ATTR_BG_WHITE (8 << (CHAR_BIT + _ATTR_COLOR_SHIFT + 4))

void term_set_cursor(int y, int x);
void term_hide_cursor(void);
void term_show_cursor(void);
void term_get_size(int *height, int *width);
Bool term_resize(void);
void term_refresh(void);
enum {
	KEY_ERROR = -1,
	KEY_TIMEOUT = -2
};
/* FIXME:
- line drawing
- add one char
*/
#endif
