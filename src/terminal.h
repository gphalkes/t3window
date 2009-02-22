#ifndef TERMINAL_H
#define TERMINAL_H

typedef enum { false, true } Bool;

Bool term_init(void);
void term_restore(void);
int term_get_keychar(int msec);

#define ATTR_UNDERLINE (1<<0)
#define ATTR_BOLD (1<<1)
#define ATTR_STANDOUT (1<<2)
#define ATTR_REVERSE (1<<3)
#define ATTR_BLINK (1<<4)
#define ATTR_DIM (1<<5)
#define ATTR_FOREGROUND(_x) (((_x) & 0x7) << 6)
#define ATTR_BACKGROUND(_x) (((_x) & 0x7) << 9)

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
