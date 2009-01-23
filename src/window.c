#include "window.h"

//FIXME: we need to figure out the proper sematics for the repaint handler, such that we
// don't unecessarily end up with half charachters:
// - Make x and width pointers such that we get back what they really painted. That way
// we know what other windows we need to draw.
//FIXME: do repainting per line
//FIXME: hide cursor when repainting
//FIXME: add routine to position the visible cursor
//FIXME: do we want to make sure we don't end up outside the window when painting? That
// would require nasty stuff like string widths, which require conversion :-(
//FIXME: implement "hardware" scrolling for optimization

struct Window {
	int x, y;
	int width, height;
	int depth;
	int attr;
	Bool shown;
	WindowRepaintHandler handler;
	Window *next;
	Window *prev;
};

Window *head;

Window *win_new(int height, int width, int y, int x, int depth, WindowRepaintHandler handler) {
	Window *retval, *ptr, *last;

	if ((retval = malloc(sizeof(Window))) == NULL)
		return NULL;

	retval->x = x;
	retval->y = y;
	retval->width = width;
	retval->height = height;
	retval->depth = depth;
	retval->handler = handler;
	retval->shown = false;

	if (head == NULL) {
		tail = head = retval;
		retval->next = retval->prev = NULL;
		return retval;
	}

	ptr = head;
	while (ptr != NULL && ptr->depth < depth)
		ptr = ptr->next;

	if (ptr == NULL) {
		retval->prev = tail;
		retval->next = NULL;
		tail->next = retval;
	} else if (ptr->prev == NULL) {
		retval->prev = NULL;
		retval->next = ptr;
		head = retval;
	} else {
		retval->prev = ptr->prev;
		retval->next = ptr;
		ptr->prev->next = retval;
		ptr->prev = retval;
	}
	return retval;
}

void win_del(Window *win) {
	if (win->next == NULL)
		tail = win->prev;
	else
		win->next->prev = win->prev;

	if (win->prev == NULL)
		head = win->next;
	else
		win->prev->next = win->next;
	free(win);
}

void win_resize(Window *win, int height, int width) {
	win->height = height
	win->width = width;
	//FIXME redraw stuff
}

void win_move(Window *win, int y, int x) {
	win->y = y;
	win->x = x;
	//FIXME redraw stuff
}

int win_get_width(Window *win) {
	return win->width;
}

void win_get_height(Window *win) {
	return win->height;
}

void win_set_cursor(Window *win, int y, int x) {
	set_cursor(win->y + y, win->x + x);
}

void win_set_attr(Window *win, int attr) {
	win->attr = attr;
}

void win_add_str(Window *win, const char *str) {
	//FIXME: not when not shown and need to repaint overlapping windows??
	set_attr(win->attr);
	add_str(str);
}

void win_show(Window *win) {
	win->shown = true;
	do {
		if (win->shown)
			win->handler(win, 0, 0, win->height, win->width);
		win = win->prev;
	} while (win);
}

void win_hide(Window *win) {
	win->shown = false;
	/* FIXME:
		- find deepest window that covers this window and all others in between
		- redraw all windows from that window to top window

		- it is probably easiest to do this one line at a time.

		For now we just repaint the whole screen.
	*/
	win = tail;
	do {
		if (win->shown)
			win->handler(win, 0, 0, win->height, win->width);
		win = win->prev;
	} while (win);
}
