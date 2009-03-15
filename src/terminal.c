#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
//FIXME: we need to do some checking which header files we need
#include <sys/select.h>
#include <sys/ioctl.h>

#include "terminal.h"
#include "window.h"
#include "internal.h"
/* The curses header file defines to many symbols that get in the way of our
   own, so we have a separate C file which exports only those functions that
   we actually use. */
#include "curses_interface.h"

/*FIXME: line drawing for UTF-8 may require that we use the UTF-8 line drawing
characters as apparently the linux console does not do alternate character set
drawing. On the other hand if it does do proper UTF-8 line drawing there is not
really a problem anyway. Simply the question of which default to use may be
of interest. */
/* FIXME: do proper cleanup on failure, especially on term_init */

static struct termios saved;
static Bool initialised, seqs_initialised;
static fd_set inset;

static char *smcup, *rmcup, *cup;
static char *sgr, *setaf, *setab, *op, *smacs, *rmacs;

static Window *terminal_window;
static LineData new_data;

static int lines, columns;
static int cursor_y, cursor_x;

static char alternate_chars[256];

static void set_alternate_chars_defaults(void) {
	alternate_chars['}'] = 'f';
	alternate_chars['.'] = 'v';
	alternate_chars[','] = '<';
	alternate_chars['+'] = '>';
	alternate_chars['-'] = '^';
	alternate_chars['h'] = '#';
	alternate_chars['~'] = 'o';
	alternate_chars['a'] = ':';
	alternate_chars['f'] = '\\';
	alternate_chars['\''] = '+';
	alternate_chars['z'] = '>';
	alternate_chars['{'] = '*';
	alternate_chars['q'] = '-';
	alternate_chars['i'] = '#';
	alternate_chars['n'] = '+';
	alternate_chars['y'] = '<';
	alternate_chars['m'] = '+';
	alternate_chars['j'] = '+';
	alternate_chars['|'] = '!';
	alternate_chars['g'] = '#';
	alternate_chars['o'] = '~';
	alternate_chars['p'] = '-';
	alternate_chars['r'] = '-';
	alternate_chars['s'] = '_';
	alternate_chars['0'] = '#';
	alternate_chars['w'] = '+';
	alternate_chars['u'] = '+';
	alternate_chars['t'] = '+';
	alternate_chars['v'] = '+';
	alternate_chars['l'] = '+';
	alternate_chars['k'] = '+';
	alternate_chars['x'] = '|';
}


#if 0
static const char *ti_strings[] = {
/* Line drawing characters */
	"acs_chars",
	"smacs",
	"rmacs",
/* Clear screen (and home cursor) */
	"clear",
/* Cursor positioning, with alternatives */
	"smcup",
	"rmcup",
	"cup",
	"vpa", /* goto line */
	"hpa", /* goto column */

	"home",
	"cud1",
	"cud",
	"cuf1",
	"cuf",
/* Cursor appearence */
	"cnorm",
	"civis",
/* Attribute setting */
	"sgr", /* set all modes at the same time */
		/* Alternatively */
	"blink",
	"bold",
	"dim",
	"smso", /* standout */
	"rmso",
	"smul", /* standout */
	"rmul",
	"rev",
	"sgr0", /* turn off all attributes */
};
/*FIXME: todo color */

static const char *ti_nums[] = {
	"xmc", /* number of spaces left on the screen when entering standout mode */
};

static const char *ti_bools[] = {
};
#endif


static char *get_ti_string(const char *name) {
	char *result = call_tigetstr(name);
	if (result == (char *) 0 || result == (char *) -1)
		return NULL;

	return strdup(result);
}

Bool term_init(void) {
	struct winsize wsz;
	if (!initialised) {
		struct termios new_params;
		if (!isatty(STDOUT_FILENO))
			return false;

		if (tcgetattr(STDOUT_FILENO, &saved) < 0)
			return false;

		new_params = saved;
		new_params.c_iflag &= ~(IXON | IXOFF);
		new_params.c_iflag |= INLCR;
		new_params.c_lflag &= ~(ISIG | ICANON | ECHO);
		new_params.c_cc[VMIN] = 1;

		if (tcsetattr(STDOUT_FILENO, TCSADRAIN, &new_params) < 0)
			return -1;
		initialised = true;
		FD_ZERO(&inset);
		FD_SET(STDIN_FILENO, &inset);

		if (!seqs_initialised) {
			int error;
			char *acsc;

			if ((error = call_setupterm()) != 0)
				return false;
			/*FIXME: we should probably have some way to return what was the problem. */

			if ((smcup = get_ti_string("smcup")) == NULL)
				return false;
			if ((rmcup = get_ti_string("rmcup")) == NULL)
				return false;
			if ((cup = get_ti_string("cup")) == NULL)
				return false;

			if ((sgr = get_ti_string("sgr")) == NULL) {
				/* FIXME: get alternatives. */
			}
			if ((setaf = get_ti_string("setaf")) == NULL) {
				/* FIXME: get alternatives. */
			}
			if ((setab = get_ti_string("setab")) == NULL) {
				/* FIXME: get alternatives. */
			}
			if ((op = get_ti_string("op")) == NULL) {
				/* FIXME: get alternatives. */
			}
			if ((acsc = get_ti_string("acsc")) == NULL) {
				set_alternate_chars_defaults();
			} else {
				/* FIXME: only required when sgr not found! */
				if ((smacs = get_ti_string("smacs")) == NULL || (rmacs = get_ti_string("rmacs")) == NULL) {
					set_alternate_chars_defaults();
				} else {
					size_t i;
					for (i = 0; i < strlen(acsc); i += 2)
						alternate_chars[(unsigned int) acsc[i]] = acsc[i + 1];
				}
				free(acsc);
			}

			seqs_initialised = true;
		}

		/* Get terminal size. First try ioctl, then environment, then terminfo. */
		if (ioctl(STDIN_FILENO, TIOCGWINSZ, &wsz) == 0) {
			lines = wsz.ws_row;
			columns = wsz.ws_col;
		} else {
			char *lines_env = getenv("LINES");
			char *columns_env = getenv("COLUMNS");
			if (lines_env == NULL || columns_env == NULL || (lines = atoi(lines_env)) == 0 || (columns = atoi(columns_env)) == 0) {
				if ((lines = call_tigetnum("lines")) < 0 || (columns = call_tigetnum("columns")) < 0)
					return false;
			}
		}

		/* Create or resize terminal window */
		if (terminal_window == NULL) {
			/* FIXME: maybe someday we can make the window outside of the window stack. */
			if ((terminal_window = win_new(lines, columns, 0, 0, 0)) == NULL)
				return false;
			if ((new_data.data = malloc(sizeof(CharData) * INITIAL_ALLOC)) == NULL)
				return false;
			new_data.allocated = INITIAL_ALLOC;
		} else {
			if (!win_resize(terminal_window, lines, columns))
				return false;
		}

		/* Start cursor positioning mode. */
		call_putp(smcup);
	}
	return true;
}


void term_restore(void) {
	/*FIXME: restore different attributes:
		- color to original pair
		- saved modes for xterm
	*/
	if (initialised) {
		call_putp(rmcup);
		tcsetattr(STDOUT_FILENO, TCSADRAIN, &saved);
		initialised = false;
	}
}

static int safe_read_char(void) {
	char c;
	while (1) {
		ssize_t retval = read(STDIN_FILENO, &c, 1);
		if (retval < 0 && errno == EINTR)
			continue;
		else if (retval >= 1)
			return c;

		return KEY_ERROR;
	}
}

int term_get_keychar(int msec) {
	int retval;
	fd_set _inset;
	struct timeval timeout;


	while (1) {
		_inset = inset;
		if (msec > 0) {
			timeout.tv_sec = msec / 1000;
			timeout.tv_usec = (msec % 1000) * 1000;
		}

		retval = select(STDIN_FILENO + 1, &_inset, NULL, NULL, msec > 0 ? &timeout : NULL);

		if (retval < 0) {
			if (errno == EINTR)
				continue;
			return KEY_ERROR;
		} else if (retval == 0) {
			return KEY_TIMEOUT;
		} else {
			return safe_read_char();
		}
	}
}

void term_set_cursor(int y, int x) {
	cursor_y = y;
	cursor_x = x;
	/* FIXCOMPAT: use cup only when supported */
	call_putp(call_tparm(cup, 2, y, x));
}

void term_hide_cursor(void);
void term_show_cursor(void);
void term_get_size(int *height, int *width);

/** Handle resizing of the terminal.

    Retrieves the size of the terminal and resizes the backing structures.
	After calling @a term_resize, @a term_get_size can be called to retrieve
    the new terminal size. Should be called after a SIGWINCH.
*/
Bool term_resize(void) {
	struct winsize wsz;

	if (ioctl(STDIN_FILENO, TIOCGWINSZ, &wsz) < 0)
		return true;

	lines = wsz.ws_row;
	columns = wsz.ws_col;
	return win_resize(terminal_window, lines, columns);
}

static int attr_to_color[10] = { 9, 0, 1, 2, 3, 4, 5, 6, 7 };
static CharData attrs = 0;
/* FIXME: make this available to user for drawing user attributes */
static void set_attrs(CharData new_attrs) {
	/* Just in case the caller forgot */
	new_attrs &= ATTR_MASK;
	if ((attrs & BASIC_ATTRS) != (new_attrs & BASIC_ATTRS)) {
		/* FIXME: implement sgr alternatives */
		if (sgr != NULL) {
			call_putp(call_tparm(sgr, 9,
				new_attrs & ATTR_STANDOUT,
				new_attrs & ATTR_UNDERLINE,
				new_attrs & ATTR_REVERSE,
				new_attrs & ATTR_BLINK,
				new_attrs & ATTR_DIM,
				new_attrs & ATTR_BOLD,
				0,
				0,
				/* FIXME: UTF-8 terminals may need different handling */
				new_attrs & ATTR_ACS));
		}
	}

	/* FIXME: for alternatives this may not work! specifically this won't
	   work if only color pairs are supported, rather than random combinations. */

	/* Set default color through op string */
	if (((attrs & FG_COLOR_ATTRS) != (new_attrs & FG_COLOR_ATTRS) && (new_attrs & FG_COLOR_ATTRS) == 0) ||
			((attrs & BG_COLOR_ATTRS) != (new_attrs & BG_COLOR_ATTRS) && (new_attrs & BG_COLOR_ATTRS) == 0)) {
		if (op != NULL) {
			call_putp(op);
			attrs = new_attrs & ~(FG_COLOR_ATTRS | BG_COLOR_ATTRS);
		}
	}

	if ((attrs & FG_COLOR_ATTRS) != (new_attrs & FG_COLOR_ATTRS)) {
		/* FIXME: implement setaf alternatives */
		if (setaf != NULL)
			call_putp(call_tparm(setaf, 1, attr_to_color[(new_attrs >> _ATTR_COLOR_SHIFT) & 0xf]));
	}

	if ((attrs & BG_COLOR_ATTRS) != (new_attrs & BG_COLOR_ATTRS)) {
		/* FIXME: implement setab alternatives */
		if (setab != NULL)
			call_putp(call_tparm(setab, 1, attr_to_color[(new_attrs >> (_ATTR_COLOR_SHIFT + 4)) & 0xf]));
	}

	attrs = new_attrs;
}

/* FIXME: assume clear background, such that we erase characters that are "invisible" */
void term_refresh(void) {
	int i, j;
	CharData new_attrs;

	for (i = 0; i < lines; i++) {
		_win_refresh_term_line(terminal_window, &new_data, i);
		/* FIXME: do diff, redraw differences, save the data in the terminal_window struct */

		/* FIXME: for now we simply paint the line (ie no optimizations) */
		call_putp(call_tparm(cup, 2, i, new_data.start));
		for (j = 0; j < new_data.length; j++) {
			if (GET_WIDTH(new_data.data[j]) > 0) {
				new_attrs = new_data.data[j] & ATTR_MASK;
				if (new_attrs != attrs)
					set_attrs(new_attrs);
			}
			putchar(new_data.data[j] & CHAR_MASK);
		}

		/* Reset new_data for the next line. */
		new_data.width = 0;
		new_data.length = 0;
		new_data.start = 0;
	}
	fflush(stdout);
}
