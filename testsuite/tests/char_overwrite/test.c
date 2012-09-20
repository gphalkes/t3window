static int test(void) {
	t3_window_t *win;

	ASSERT(win = t3_win_new(NULL, 10, 10, 3, 5, 10));
	t3_win_set_paint(win, 0, 0);
	t3_win_addstr(win, "abcdefghijklmn", 0);
	t3_win_show(win);
	t3_term_hide_cursor();

	t3_win_set_paint(win, 0, 5);
	t3_win_addstr(win, "\xCC\x81", 0);
	next();

	t3_win_set_paint(win, 0, 5);
	t3_win_addstr(win, "Z", 0);
	next();

	t3_win_set_paint(win, 0, 4);
	t3_win_addstr(win, "X", 0);
	t3_win_set_paint(win, 0, 7);
	t3_win_addstr(win, "Y", 0);
	next();

	return 0;
}
