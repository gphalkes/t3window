#ifndef INTERNAL_STRUCTS_H
#define INTERNAL_STRUCTS_H

#define INITIAL_ALLOC 80

//FIXME: make sure that the base type is the correct size to store all the attributes
typedef int CharData;

typedef struct {
	CharData *data;
	int start;
	int width;
	int length;
	int allocated;
} LineData;

struct Window {
	int x, y;
	int paint_x, paint_y;
	int width, height;
	int depth;
	int attr;
	Bool shown;
	LineData *lines;

	/* Pointers for linking into depth sorted list. */
	Window *next;
	Window *prev;
};

Bool _win_refresh_term_line(struct Window *terminal, LineData *store, int line);

#endif
