static int test(void) {
	t3_window_t *win;

	ASSERT(win = t3_win_new(NULL, 10, 10, 3, 5, 10));
	t3_win_set_paint(win, 0, 0);
	t3_win_addstr(win, "abcde\xCC\x81""fghijklmn", 0);
	t3_win_show(win);
	t3_term_hide_cursor();

	t3_win_set_paint(win, 0, 1);
	t3_win_addstr(win, "\xCC\x82", 0);
	t3_win_set_paint(win, 0, 10);
	t3_win_addstr(win, "\xCC\x81", 0);
	next();


	return 0;
}
