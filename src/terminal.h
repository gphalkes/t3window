#ifndef TERMINAL_H
#define TERMINAL_H

Bool init_terminal(void);
void restore_terminal(void);
int get_keychar(int msec);

#define ATTR_UNDERLINE (1<<0)
#define ATTR_BOLD (1<<1)
#define ATTR_STANDOUT (1<<2)
#define ATTR_REVERSE (1<<3)
#define ATTR_BLINK (1<<4)
#define ATTR_DIM (1<<5)
#define ATTR_FOREGROUND(_x) (((_x) & 0x7) << 6)
#define ATTR_BACKGROUND(_x) (((_x) & 0x7) << 9)

void set_cursor(int y, int x);
void set_attr(int attr);
void add_str(const char *str);
void hide_cursor(void);
void show_cursor(void);
void get_terminal_size(int *height, int *width);

/* FIXME:
- line drawing
- add one char
*/
#endif
