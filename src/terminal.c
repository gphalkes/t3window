#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
//FIXME: we need to do some checking which header files we need
#include <sys/select.h>
#include <sys/ioctl.h>
#include <assert.h>
#include <limits.h>
//FIXME: allow alternative
#include <langinfo.h>

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
/* FIXME: km property indicates that 8th bit *may* be alt. Then smm and rmm may help */

static struct termios saved;
static Bool initialised, seqs_initialised;
static fd_set inset;

static char *smcup, *rmcup, *cup, *sc, *rc, *clear, *home, *vpa, *hpa, *cud, *cud1, *cuf, *cuf1;
static char *civis, *cnorm;
static char *sgr, *setaf, *setab, *op, *smacs, *rmacs, *sgr0, *smul, *rmul,
	*rev, *bold, *blink, *dim, *setf, *setb;
static char *el;
static CharData ncv;
static Bool bce;

static Window *terminal_window;
static LineData old_data;

static int lines, columns;
static int cursor_y, cursor_x;
static Bool show_cursor = True;

static int attr_to_color[10] = { 9, 0, 1, 2, 3, 4, 5, 6, 7, 9 };
static int attr_to_alt_color[10] = { 0, 0, 4, 2, 6, 1, 5, 3, 7, 0 };
static CharData attrs = 0;
static CharData ansi_attrs = 0;
static CharData reset_required_mask = ATTR_BOLD | ATTR_REVERSE | ATTR_BLINK | ATTR_DIM;
static TermUserCallback user_callback = NULL;

static char alternate_chars[256];
static char default_alternate_chars[256];

static void set_alternate_chars_defaults(char *table) {
	table['}'] = 'f';
	table['.'] = 'v';
	table[','] = '<';
	table['+'] = '>';
	table['-'] = '^';
	table['h'] = '#';
	table['~'] = 'o';
	table['a'] = ':';
	table['f'] = '\\';
	table['\''] = '+';
	table['z'] = '>';
	table['{'] = '*';
	table['q'] = '-';
	table['i'] = '#';
	table['n'] = '+';
	table['y'] = '<';
	table['m'] = '+';
	table['j'] = '+';
	table['|'] = '!';
	table['g'] = '#';
	table['o'] = '~';
	table['p'] = '-';
	table['r'] = '-';
	table['s'] = '_';
	table['0'] = '#';
	table['w'] = '+';
	table['u'] = '+';
	table['t'] = '+';
	table['v'] = '+';
	table['l'] = '+';
	table['k'] = '+';
	table['x'] = '|';
}

static char *get_ti_string(const char *name) {
	char *result = call_tigetstr(name);
	if (result == (char *) 0 || result == (char *) -1)
		return NULL;

	return strdup(result);
}

static void do_cup(int line, int col) {
	if (cup != NULL) {
		call_putp(call_tparm(cup, 2, line, col));
		return;
	}
	if (vpa != NULL) {
		call_putp(call_tparm(vpa, 1, line));
		call_putp(call_tparm(hpa, 1, col));
		return;
	}
	if (home != NULL) {
		int i;

		call_putp(home);
		if (line > 0) {
			if (cud != NULL) {
				call_putp(call_tparm(cud, 1, line));
			} else {
				for (i = 0; i < line; i++)
					call_putp(cud1);
			}
		}
		if (col > 0) {
			if (cuf != NULL) {
				call_putp(call_tparm(cuf, 1, col));
			} else {
				for (i = 0; i < col; i++)
					call_putp(cuf1);
			}
		}
	}
}

static void do_smcup(void) {
	if (smcup != NULL) {
		call_putp(smcup);
		return;
	}
	if (clear != NULL) {
		call_putp(clear);
		return;
	}
}

static void do_rmcup(void) {
	if (rmcup != NULL) {
		call_putp(rmcup);
		return;
	}
	if (clear != NULL) {
		call_putp(clear);
		do_cup(lines - 1, 0);
		return;
	}
}

#define streq(a,b) (strcmp((a), (b)) == 0)
static void detect_ansi(void) {
	CharData non_existant = 0;

	if (op != NULL && (streq(op, "\033[39;49m") || streq(op, "\033[49;39m"))) {
		if (setaf != NULL && streq(setaf, "\033[3%p1%dm") &&
				setab != NULL && streq(setab, "\033[4%p1%dm"))
			ansi_attrs |= FG_COLOR_ATTRS |BG_COLOR_ATTRS;
	}
	if (smul != NULL && rmul != NULL && streq(smul, "\033[4m") && streq(rmul, "\033[24m"))
		ansi_attrs |= ATTR_UNDERLINE;
	if (smacs != NULL && rmacs != NULL && streq(smacs, "\033[11m") && streq(rmacs, "\03310m"))
		ansi_attrs |= ATTR_ACS;

	/* So far, we have been able to check that the "exit mode" operation was ANSI compatible as well.
	   However, for bold, dim, reverse and blink we can't check this, so we will only accept them
	   as attributes if the terminal uses ANSI colors, and they all match in as far as they exist.
	*/
	if ((ansi_attrs & (FG_COLOR_ATTRS | BG_COLOR_ATTRS)) == 0)
		return;

	if (rev != NULL) {
		if (streq(rev, "\033[7m"))
			ansi_attrs |= ATTR_REVERSE;
	} else {
		non_existant |= ATTR_REVERSE;
	}

	if (bold != NULL) {
		if (streq(bold, "\033[1m"))
			ansi_attrs |= ATTR_BOLD;
	} else {
		non_existant |= ATTR_BOLD;
	}

	if (dim != NULL) {
		if (streq(dim, "\033[2m"))
			ansi_attrs |= ATTR_DIM;
	} else {
		non_existant |= ATTR_DIM;
	}

	if (blink != NULL) {
		if (streq(blink, "\033[5m"))
			ansi_attrs |= ATTR_BLINK;
	} else {
		non_existant |= ATTR_BLINK;
	}

	if (((non_existant | ansi_attrs) & (ATTR_REVERSE | ATTR_BOLD | ATTR_DIM | ATTR_BLINK)) !=
			(ATTR_REVERSE | ATTR_BOLD | ATTR_DIM | ATTR_BLINK))
		ansi_attrs &= ~(ATTR_REVERSE | ATTR_BOLD | ATTR_DIM | ATTR_BLINK);
}


Bool term_init(void) {
	struct winsize wsz;
	char *enacs;

	if (!initialised) {
		struct termios new_params;
		int ncv_int;

		if (!isatty(STDOUT_FILENO))
			return False;

		if (tcgetattr(STDOUT_FILENO, &saved) < 0)
			return False;

		new_params = saved;
		new_params.c_iflag &= ~(IXON | IXOFF | IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
		new_params.c_lflag &= ~(ISIG | ICANON | ECHO);
		new_params.c_oflag &= ~OPOST;
		new_params.c_cflag &= ~(CSIZE | PARENB);
		new_params.c_cflag |= CS8;
		new_params.c_cc[VMIN] = 1;

		if (tcsetattr(STDOUT_FILENO, TCSADRAIN, &new_params) < 0)
			return False;
		initialised = True;
		FD_ZERO(&inset);
		FD_SET(STDIN_FILENO, &inset);

		if (!seqs_initialised) {
			int error;
			char *acsc;

			if ((error = call_setupterm()) != 0)
				return False;
			/*FIXME: we should probably have some way to return what was the problem. */

			if ((smcup = get_ti_string("smcup")) == NULL || (rmcup = get_ti_string("rmcup")) == NULL) {
				if (smcup != NULL) {
					free(smcup);
					smcup = NULL;
				}
			}
			if ((clear = get_ti_string("clear")) == NULL)
				return False;
			if ((cup = get_ti_string("cup")) == NULL)
				return False;

			sgr = get_ti_string("sgr");
			smul = get_ti_string("smul");
			if (smul != NULL && (rmul = get_ti_string("rmul")) == NULL)
				reset_required_mask |= ATTR_UNDERLINE;
			bold = get_ti_string("bold");
			/*FIXME: we could get smso and rmso for the purpose of ANSI detection. On many
			  terminals smso == rev and rmso = exit rev */
/* 			smso = get_ti_string("smso");
			if (smso != NULL && (rmso = get_ti_string("rmso")) == NULL)
				reset_required_mask |= ATTR_STANDOUT; */
			rev = get_ti_string("rev");
			blink = get_ti_string("blink");
			dim = get_ti_string("dim");
			smacs = get_ti_string("smacs");
			if (smacs != NULL && (rmacs = get_ti_string("rmacs")) == NULL)
				reset_required_mask |= ATTR_ACS;

			//FIXME: use scp if neither setaf/setf is available
			if ((setaf = get_ti_string("setaf")) == NULL)
				setf = get_ti_string("setf");
			if ((setab = get_ti_string("setab")) == NULL)
				setb = get_ti_string("setb");

			op = get_ti_string("op");

			detect_ansi();

			sgr0 = get_ti_string("sgr0");
			/* If sgr0 is not defined, don't go into modes in reset_required_mask. */
			if (sgr0 == NULL) {
				reset_required_mask = 0;
				rev = NULL;
				bold = NULL;
				blink = NULL;
				dim = NULL;
				if (rmul == NULL) smul = NULL;
				if (rmacs == NULL) smacs = NULL;
			}

			bce = call_tigetflag("bce");
			if ((el = get_ti_string("el")) == NULL)
				bce = True;

			if ((sc = get_ti_string("sc")) != NULL && (rc = get_ti_string("rc")) == NULL)
				sc = NULL;

			civis = get_ti_string("civis");
			cnorm = get_ti_string("cnorm");

			if ((acsc = get_ti_string("acsc")) == NULL) {
				set_alternate_chars_defaults(alternate_chars);
			} else {
				if (sgr == NULL && ((smacs = get_ti_string("smacs")) == NULL || (rmacs = get_ti_string("rmacs")) == NULL)) {
					set_alternate_chars_defaults(alternate_chars);
				} else {
					size_t i;
					for (i = 0; i < strlen(acsc); i += 2)
						alternate_chars[(unsigned int) acsc[i]] = acsc[i + 1];
				}
				free(acsc);
			}

			seqs_initialised = True;
		}
		set_alternate_chars_defaults(default_alternate_chars);

		ncv_int = call_tigetnum("ncv");
/* 		if (ncv_int & (1<<0)) ncv |= ATTR_STANDOUT; */
		if (ncv_int & (1<<1)) ncv |= ATTR_UNDERLINE;
		if (ncv_int & (1<<2)) ncv |= ATTR_REVERSE;
		if (ncv_int & (1<<3)) ncv |= ATTR_BLINK;
		if (ncv_int & (1<<4)) ncv |= ATTR_DIM;
		if (ncv_int & (1<<5)) ncv |= ATTR_BOLD;
		if (ncv_int & (1<<8)) ncv |= ATTR_ACS;

		/* Get terminal size. First try ioctl, then environment, then terminfo. */
		if (ioctl(STDIN_FILENO, TIOCGWINSZ, &wsz) == 0) {
			lines = wsz.ws_row;
			columns = wsz.ws_col;
		} else {
			char *lines_env = getenv("LINES");
			char *columns_env = getenv("COLUMNS");
			if (lines_env == NULL || columns_env == NULL || (lines = atoi(lines_env)) == 0 || (columns = atoi(columns_env)) == 0) {
				if ((lines = call_tigetnum("lines")) < 0 || (columns = call_tigetnum("columns")) < 0)
					return False;
			}
		}

		/* Create or resize terminal window */
		if (terminal_window == NULL) {
			/* FIXME: maybe someday we can make the window outside of the window stack. */
			if ((terminal_window = win_new(lines, columns, 0, 0, 0)) == NULL)
				return False;
			if ((old_data.data = malloc(sizeof(CharData) * INITIAL_ALLOC)) == NULL)
				return False;
			old_data.allocated = INITIAL_ALLOC;
		} else {
			if (!win_resize(terminal_window, lines, columns))
				return False;
		}

		//FIXME: can we find a way to save the current terminal settings (attrs etc)?
		/* Start cursor positioning mode. */
		do_smcup();
		/* Make sure the cursor is visible */
		call_putp(cnorm);

		/* Enable alternate character set if required by terminal. */
		if ((enacs = get_ti_string("enacs")) != NULL) {
			call_putp(enacs);
			free(enacs);
		}


		//FIXME: set the attributes of the terminal to a known value

		//FIXME: only works when setlocale has been called first. This should
		// therefore be a requirement for calling this function
		//FIXME: only for UTF-8, or can we do any multibyte encoding??
		if (strcmp(nl_langinfo(CODESET), "UTF-8") == 0) {
			//FIXME: no iconv needed (for now)
			_win_set_multibyte();
		}
	}
	return True;
}


void term_restore(void) {
	if (initialised) {
		/* Ensure complete repaint of the terminal on re-init (if required) */
		win_set_paint(terminal_window, 0, 0);
		win_clrtobot(terminal_window);
		if (seqs_initialised) {
			do_rmcup();
			/* Restore cursor to visible state. */
			if (!show_cursor)
				call_putp(cnorm);
			/* Make sure attributes are reset */
			term_set_attrs(0);
			fflush(stdout);
		}
		tcsetattr(STDOUT_FILENO, TCSADRAIN, &saved);
		initialised = False;
	}
}

static int safe_read_char(void) {
	char c;
	while (1) {
		ssize_t retval = read(STDIN_FILENO, &c, 1);
		if (retval < 0 && errno == EINTR)
			continue;
		else if (retval >= 1)
			return (int) (unsigned char) c;

		return KEY_ERROR;
	}
}

static int last_key = -1, stored_key = -1;

int term_get_keychar(int msec) {
	int retval;
	fd_set _inset;
	struct timeval timeout;

	if (stored_key >= 0) {
		last_key = stored_key;
		stored_key = -1;
		return last_key;
	}

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
			return last_key = safe_read_char();
		}
	}
}

int term_unget_keychar(int c) {
	if (c == last_key) {
		stored_key = c;
		return c;
	}
	return KEY_ERROR;
}

void term_set_cursor(int y, int x) {
	cursor_y = y;
	cursor_x = x;
	if (show_cursor) {
		do_cup(y, x);
		fflush(stdout);
	}
}

void term_hide_cursor(void) {
	if (show_cursor) {
		show_cursor = False;
		call_putp(civis);
		fflush(stdout);
	}
}

void term_show_cursor(void) {
	if (!show_cursor) {
		show_cursor = True;
		do_cup(cursor_y, cursor_x);
		call_putp(cnorm);
		fflush(stdout);
	}
}

void term_get_size(int *height, int *width) {
	*height = terminal_window->height;
	*width = terminal_window->width;
}

/** Handle resizing of the terminal.

    Retrieves the size of the terminal and resizes the backing structures.
	After calling @a term_resize, @a term_get_size can be called to retrieve
    the new terminal size. Should be called after a SIGWINCH.
*/
Bool term_resize(void) {
	struct winsize wsz;

	if (ioctl(STDIN_FILENO, TIOCGWINSZ, &wsz) < 0)
		return True;

	lines = wsz.ws_row;
	columns = wsz.ws_col;
	if (columns > terminal_window->width || lines != terminal_window->height) {
		int i;
		for (i = 0; i < terminal_window->height; i++) {
			win_set_paint(terminal_window, i, 0);
			win_clrtoeol(terminal_window);
		}
		call_putp(clear);
	}
	return win_resize(terminal_window, lines, columns);
}

static void set_attrs_non_ansi(CharData new_attrs) {
	CharData attrs_basic_non_ansi = attrs & BASIC_ATTRS & ~ansi_attrs;
	CharData new_attrs_basic_non_ansi = new_attrs & BASIC_ATTRS & ~ansi_attrs;

	if (attrs_basic_non_ansi != new_attrs_basic_non_ansi) {
		CharData changed;
		if (attrs_basic_non_ansi & ~new_attrs & reset_required_mask) {
			if (sgr != NULL) {
				call_putp(call_tparm(sgr, 9,
					/* new_attrs & ATTR_STANDOUT */ 0,
					new_attrs & ATTR_UNDERLINE,
					new_attrs & ATTR_REVERSE,
					new_attrs & ATTR_BLINK,
					new_attrs & ATTR_DIM,
					new_attrs & ATTR_BOLD,
					0,
					0,
					/* FIXME: UTF-8 terminals may need different handling */
					new_attrs & ATTR_ACS));
				attrs = new_attrs & ~(FG_COLOR_ATTRS | BG_COLOR_ATTRS);
				attrs_basic_non_ansi = attrs & ~ansi_attrs;
			} else {
				/* Note that this will not be NULL if it is required because of
				   tests in the initialization. */
				call_putp(sgr0);
				attrs_basic_non_ansi = attrs = 0;
			}
		}

		changed = attrs_basic_non_ansi ^ new_attrs_basic_non_ansi;
		if (changed) {
			if (changed & ATTR_UNDERLINE)
				call_putp(new_attrs & ATTR_UNDERLINE ? smul : rmul);
			if (changed & ATTR_REVERSE)
				call_putp(rev);
			if (changed & ATTR_BLINK)
				call_putp(blink);
			if (changed & ATTR_DIM)
				call_putp(dim);
			if (changed & ATTR_BOLD)
				call_putp(bold);
			if (changed & ATTR_ACS)
				call_putp(new_attrs & ATTR_ACS ? smacs : rmacs);
		}
	}


	if ((~ansi_attrs & (FG_COLOR_ATTRS | BG_COLOR_ATTRS)) == 0)
		return;

	if ((new_attrs & FG_COLOR_ATTRS) == ATTR_FG_DEFAULT)
		new_attrs &= ~(FG_COLOR_ATTRS);
	if ((new_attrs & BG_COLOR_ATTRS) == ATTR_BG_DEFAULT)
		new_attrs &= ~(BG_COLOR_ATTRS);

	/* Set default color through op string */
	if (((attrs & FG_COLOR_ATTRS) != (new_attrs & FG_COLOR_ATTRS) && (new_attrs & FG_COLOR_ATTRS) == 0) ||
			((attrs & BG_COLOR_ATTRS) != (new_attrs & BG_COLOR_ATTRS) && (new_attrs & BG_COLOR_ATTRS) == 0)) {
		if (op != NULL) {
			call_putp(op);
			attrs = new_attrs & ~(FG_COLOR_ATTRS | BG_COLOR_ATTRS);
		}
	}

	/* FIXME: for alternatives this may not work! specifically this won't
	   work if only color pairs are supported, rather than random combinations. */
	if ((attrs & FG_COLOR_ATTRS) != (new_attrs & FG_COLOR_ATTRS)) {
		if (setaf != NULL)
			call_putp(call_tparm(setaf, 1, attr_to_color[(new_attrs >> _ATTR_COLOR_SHIFT) & 0xf]));
		else if (setf != NULL)
			call_putp(call_tparm(setf, 1, attr_to_alt_color[(new_attrs >> _ATTR_COLOR_SHIFT) & 0xf]));
	}

	if ((attrs & BG_COLOR_ATTRS) != (new_attrs & BG_COLOR_ATTRS)) {
		if (setab != NULL)
			call_putp(call_tparm(setab, 1, attr_to_color[(new_attrs >> (_ATTR_COLOR_SHIFT + 4)) & 0xf]));
		else if (setb != NULL)
			call_putp(call_tparm(setb, 1, attr_to_alt_color[(new_attrs >> (_ATTR_COLOR_SHIFT + 4)) & 0xf]));
	}

	attrs = new_attrs;
}

#define ADD_SEP() do { strcat(mode_string, sep); sep = ";"; } while(0)

void term_set_attrs(CharData new_attrs) {
	char mode_string[30]; /* Max is (if I counted correctly) 24. Use 30 for if I miscounted. */
	CharData changed_attrs;
	const char *sep = "[";

	/* Just in case the caller forgot */
	new_attrs &= ATTR_MASK;

	if (new_attrs == 0 && sgr0 != NULL) {
		call_putp(sgr0);
		attrs = 0;
		return;
	}

	changed_attrs = (new_attrs ^ attrs) & ~ansi_attrs;
	if (changed_attrs != 0)
		/* Add ansi attributes from current attributes, because set_attrs_non_ansi
		   will change attrs to the value passed here, or to some value without
	       the ansi attributes if an sgr was required. */
		set_attrs_non_ansi(new_attrs);

	changed_attrs = (new_attrs ^ attrs) & ansi_attrs;
	if (changed_attrs == 0) {
		attrs = new_attrs;
		return;
	}

	mode_string[0] = '\033';
	mode_string[1] = 0;

	if (changed_attrs & ATTR_UNDERLINE) {
		ADD_SEP();
		strcat(mode_string, new_attrs & ATTR_UNDERLINE ? "4" : "24");
	}

	if (changed_attrs & (ATTR_BOLD | ATTR_DIM)) {
		ADD_SEP();
		strcat(mode_string, new_attrs & ATTR_BOLD ? "1" : (new_attrs & ATTR_DIM ? "2" : "22"));
	}

	if (changed_attrs & ATTR_REVERSE) {
		ADD_SEP();
		strcat(mode_string, new_attrs & ATTR_REVERSE ? "7" : "27");
	}

	if (changed_attrs & ATTR_BLINK) {
		ADD_SEP();
		strcat(mode_string, new_attrs & ATTR_BLINK ? "5" : "25");
	}

	if (changed_attrs & ATTR_ACS) {
		ADD_SEP();
		strcat(mode_string, new_attrs & ATTR_ACS ? "11" : "10");
	}

	if (changed_attrs & FG_COLOR_ATTRS) {
		char color[3];
		color[0] = '3';
		color[1] = '0' + attr_to_color[(new_attrs >> _ATTR_COLOR_SHIFT) & 0xf];
		color[2] = 0;
		ADD_SEP();
		strcat(mode_string, color);
	}

	if (changed_attrs & BG_COLOR_ATTRS) {
		char color[3];
		color[0] = '4';
		color[1] = '0' + attr_to_color[(new_attrs >> (_ATTR_COLOR_SHIFT + 4)) & 0xf];
		color[2] = 0;
		ADD_SEP();
		strcat(mode_string, color);
	}
	strcat(mode_string, "m");
	call_putp(mode_string);
	attrs = new_attrs;
#undef ADD_SEP
}

void term_set_user_callback(TermUserCallback callback) {
	user_callback = callback;
}

#define SWAP_LINES(a, b) do { LineData save; save = (a); (a) = (b); (b) = save; } while (0)
void term_refresh(void) {
	int i, j;
	CharData new_attrs;

	if (show_cursor) {
		call_putp(sc);
		call_putp(civis);
	}

	/* There may be another optimization possibility here: if the new line is
	   shorter than the old line, we could detect that the end of the new line
	   matches the old line. In that case we could skip printing the end of the
	   new line. Of course the question is how often this will actually happen.
	   It also brings with it several issues with the clearing of the end of
	   the line. */
	for (i = 0; i < lines; i++) {
		int new_idx, old_idx = terminal_window->lines[i].length, width = 0;
		SWAP_LINES(old_data, terminal_window->lines[i]);
		_win_refresh_term_line(terminal_window, i);

		new_idx = terminal_window->lines[i].length;

		/* Find the last character that is different. */
		if (old_data.width == terminal_window->lines[i].width) {
			for (new_idx--, old_idx--; new_idx >= 0 &&
					old_idx >= 0 && terminal_window->lines[i].data[new_idx] == old_data.data[old_idx];
					new_idx--, old_idx--)
			{}
			if (new_idx == -1) {
				assert(old_idx == -1);
				goto done;
			}
			assert(old_idx >= 0);
			for (new_idx++; new_idx < terminal_window->lines[i].length && GET_WIDTH(terminal_window->lines[i].data[new_idx]) == 0; new_idx++) {}
			for (old_idx++; old_idx < old_data.length &&
				GET_WIDTH(old_data.data[old_idx]) == 0; old_idx++) {}
		}

		/* Find the first character that is different */
		for (j = 0; j < new_idx && j < old_idx && terminal_window->lines[i].data[j] == old_data.data[j]; j++)
			width += GET_WIDTH(terminal_window->lines[i].data[j]);

		/* Go back to the last non-zero-width character, because that is the one we want to print first. */
		if ((j < new_idx && GET_WIDTH(terminal_window->lines[i].data[j]) == 0) || (j < old_idx && GET_WIDTH(old_data.data[j]) == 0)) {
			for (; j > 0 && (GET_WIDTH(terminal_window->lines[i].data[j]) == 0 || GET_WIDTH(old_data.data[j]) == 0); j--) {}
			width -= GET_WIDTH(terminal_window->lines[i].data[j]);
		}

		/* Position the cursor */
		do_cup(i, width);
		for (; j < new_idx; j++) {
			if (GET_WIDTH(terminal_window->lines[i].data[j]) > 0) {
				if (width + GET_WIDTH(terminal_window->lines[i].data[j]) > terminal_window->width)
					break;

				new_attrs = terminal_window->lines[i].data[j] & ATTR_MASK;

				width += GET_WIDTH(terminal_window->lines[i].data[j]);
				if (user_callback != NULL && new_attrs & ATTR_USER_MASK) {
					/* Let the user draw this character because they want funky attributes */
					int start = j;
					for (j++; j < new_idx && GET_WIDTH(terminal_window->lines[i].data[j]) == 0; j++) {}
					user_callback(terminal_window->lines[i].data + start, j - start);
					if (j < new_idx)
						j--;
					continue;
				} else if (new_attrs != attrs) {
					term_set_attrs(new_attrs);
				}
			}
			if (attrs & ATTR_ACS)
				putchar(alternate_chars[terminal_window->lines[i].data[j] & CHAR_MASK]);
			else
				putchar(terminal_window->lines[i].data[j] & CHAR_MASK);
		}

		/* Clear the terminal line if the new line is shorter than the old one. */
		if ((terminal_window->lines[i].width < old_data.width || j < new_idx) && width < terminal_window->width) {
			if (bce && (attrs & ~FG_COLOR_ATTRS) != 0)
				term_set_attrs(0);

			if (el != NULL) {
				call_putp(el);
			} else {
				int max = old_data.width < terminal_window->width ? old_data.width : terminal_window->width;
				for (; width < max; width++)
					putchar(' ');
			}
		}

done: /* Add empty statement to shut up compilers */ ;
	}

	term_set_attrs(0);

	if (show_cursor) {
		if (rc != NULL)
			call_putp(rc);
		else
			do_cup(cursor_y, cursor_x);
		call_putp(cnorm);
	}

	fflush(stdout);
}

void term_renew(void) {
	term_set_attrs(0);
	call_putp(clear);
	win_set_paint(terminal_window, 0, 0);
	win_clrtobot(terminal_window);
}

void term_putp(const char *str) {
	call_putp(str);
}

/* FIXME: this isn't exactly the cleanest way to do this, but for now it works and
   avoids code duplication */
int term_strwidth(const char *str) {
	Window *win = win_new(1, INT_MAX, 0, 0, 0);
	int result;

	win_set_paint(win, 0, 0);
	win_addstr(win, str, 0);
	result = win->paint_x;
	win_del(win);

	return result;
}

Bool term_acs_available(int idx) {
	if (idx < 0 || idx > 255)
		return False;
	return alternate_chars[idx] != 0;
}

char term_get_default_acs(int idx) {
	if (idx < 0 || idx > 255)
		return ' ';
	return default_alternate_chars[idx] != 0 ? default_alternate_chars[idx] : ' ';
}

CharData term_combine_attrs(CharData a, CharData b) {
	#warning FIXME: take ncv into account
	CharData result = b | (a & ~(FG_COLOR_ATTRS | BG_COLOR_ATTRS));
	if ((a & FG_COLOR_ATTRS) != 0)
		result = (result & ~(FG_COLOR_ATTRS)) | (a & FG_COLOR_ATTRS);
	if ((a & BG_COLOR_ATTRS) != 0)
		result = (result & ~(BG_COLOR_ATTRS)) | (a & BG_COLOR_ATTRS);
	return result;
}

CharData term_get_ncv(void) {
	return ncv;
}
