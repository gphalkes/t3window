static int test(void) {
	t3_window_t *low, *high, *sub;

	t3_term_hide_cursor();
	ASSERT(low = t3_win_new(NULL, 10, 10, 0, 5, 10));
	ASSERT(high = t3_win_new(NULL, 10, 10, 5, 10, 0));
	t3_win_show(low);
	next();

	t3_win_set_paint(low, 0, 0);
	t3_win_addstr(low, "0123456789-", 0);
	t3_win_set_paint(low, 6, 0);
	t3_win_addstr(low, "abＱc̃defghijk", 0);
	next();

	t3_term_show_cursor();
	t3_win_set_cursor(low, 0, 0);
	t3_win_show(high);
	next();

	t3_win_set_paint(high, 0, 0);
	t3_win_addstr(high, "ABCDEFGHIJK", 0);
	next();

	t3_win_set_paint(high, 1, 0);
	t3_win_addstr(high, "9876543210+", T3_ATTR_REVERSE | T3_ATTR_FG_RED);
	t3_win_set_paint(high, 2, 0);
	t3_win_addstr(high, "wutvlkmjqx", T3_ATTR_ACS);

	t3_term_set_user_callback(callback);
	t3_win_set_paint(high, 3, 0);
	t3_win_addstr(high, "f", T3_ATTR_USER);
	next();

	t3_win_hide(high);
	next();

	t3_win_move(high, 5, 0);
	t3_win_resize(high, 10, 8);
	t3_win_show(high);
	next();

	t3_win_hide(high);
	next();

	t3_win_box(low, 0, 0, 10, 10, T3_ATTR_REVERSE);
	next();

	t3_win_hide(low);
	next();

	//~ ASSERT(sub = t3_win_new(low, 1, 20, 1, -6, -3));
	ASSERT(sub = t3_win_new(low, 1, 20, 1, -6, -3));

	t3_win_set_paint(sub, 0, 2);
	t3_win_set_default_attrs(sub, T3_ATTR_REVERSE);
	t3_win_addstr(sub, "abcＱabcＱabcＱ", 0);
	t3_win_show(sub);
	next();

	t3_win_show(low);
	next();

	return 0;
}

