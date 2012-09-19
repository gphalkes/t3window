static int test(void) {
	t3_window_t *win;

	ASSERT(win = t3_win_new(NULL, 10, 10, 3, 5, 10));
	t3_win_show(win);
	t3_term_hide_cursor();
	next();

	t3_win_set_paint(win, 0, 0);
	t3_win_addstr(win, "0123456789-", 0);
	t3_win_set_paint(win, 6, 0);
	t3_win_addstr(win, "abＱc̃defghijk", 0);
	next();

	t3_term_show_cursor();
	t3_win_set_cursor(win, 0, 0);
	next();

	return 0;
}
