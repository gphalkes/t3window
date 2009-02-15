#include <termios.h>
#include <unistd.h>
#include <errno.h>
//FIXME: we need to do some checking which header files we need
#include <sys/select.h>
#include "terminal.h"
#include "window.h"

static struct termios saved;
static Bool initialised, seqs_initialised;
static fd_set inset;

/*FIXME: line drawing for UTF-8 may require that we use the UTF-8 line drawing
characters as apparently the linux console does not do alternate character set
drawing. On the other hand if it does do proper UTF-8 line drawing there is not
really a problem anyway. Simply the question of which default to use may be
of interest. */

Bool init_terminal(void) {
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
		/*FIXME: find control sequences for different supported actions*/
		}
		/*FIXME: send terminal control sequences:
			check restore_shell_mode in ncurses
			- smcup
			- smkx
		*/
		/*FIXME: get terminal size, ioctl, env, terminfo */
		/*FIXME: get 2 initialsed window structs, with the size of the terminal
			Implement refresh. */

	}
	return true;
}


void restore_terminal(void) {
	/*FIXME: restore different attributes:
		- color to original pair
		- saved modes for xterm
	*/
	if (initialised) {
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

int get_keychar(int msec) {
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

void set_cursor(int y, int x) {

}

void hide_cursor(void);
void show_cursor(void);
void get_terminal_size(int *height, int *width);
