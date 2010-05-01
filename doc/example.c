#include <stdlib.h>
#include <stdio.h>
#include <t3/terminal.h>
#include <t3/window.h>

int main(int argc, char *argv[]) {
	int result;
	t3_window_t *hidden, *exposed;

	/* Initialize libt3window for standard input/output. */
	result = t3_term_init(-1);

	if (result != T3_ERR_SUCCESS) {
		fprintf(stderr, "Error initializing terminal: %s\n", t3_window_strerror(result));
		exit(EXIT_FAILURE);
	}

	/* Create a new 10x10 window on line 0, column 5, depth 10. */
	hidden = t3_win_new(10, 10, 0, 5, 10);
	/* Create a new 10x10 window on line 5, column 11, depth 0. */
	exposed = t3_win_new(10, 10, 5, 11, 0);
	if (hidden == NULL || exposed == NULL) {
		/* Restore the terminal to normal state. */
		t3_term_restore();
		fprintf(stderr, "Not enough memory available for creating windows\n");
		exit(EXIT_FAILURE);
	}

	/* Draw a box on the hidden window with reverse video. */
	t3_win_box(hidden, 0, 0, 10, 10, T3_ATTR_REVERSE);
	/* Draw a box on the exposed window without special attributes. */
	t3_win_box(exposed, 0, 0, 10, 10, 0);

	/* Set the paint cursor for the hidden window at row 7, column 1 of the window. */
	t3_win_set_paint(hidden, 7, 1);
	/* Draw the string "Hello" on the hidden window. */
	t3_win_addstr(hidden, "Hello", 0);

	/* Set the paint cursor for the exposed window at row 2, column 1 of the window. */
	t3_win_set_paint(exposed, 2, 1);
	/* Draw the string "World" on the exposed window. */
	t3_win_addstr(exposed, "World", 0);

	/* Show windows. */
	t3_win_show(hidden);
	t3_win_show(exposed);

	/* Hide cursor */
	t3_term_hide_cursor();

	/* Now update the terminal to reflect our drawing. */
	t3_term_update();

	/* Wait for the user to press the a key. */
	t3_term_get_keychar(-1);
	/* If the key the user pressed resulted in character sequence rather than
	   a single character, we want to read those here because otherwise we won't
	   wait before the end of the program. Any characters available within
	   10 msec are deemed to be part of this character sequence. */
	while (t3_term_get_keychar(10) >= 0) {}

	/* Hide the exposed window. */
	t3_win_hide(exposed);

	/* Now update the terminal to reflect our changes. */
	t3_term_update();

	/* Wait for the user to press the a key. */
	t3_term_get_keychar(-1);
	while (t3_term_get_keychar(10) >= 0) {}

	/* Restore the terminal to normal state. */
	t3_term_restore();
	exit(EXIT_SUCCESS);
}
