/* Copyright (C) 2016 G.P. Halkes
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 3, as
   published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file */
#include <string.h>

#include <divsufsort.h>

#include "window.h"
#include "log.h"
#include "internal.h"

#if 1
#define CHUNK_SIZE 4

typedef struct {
	uint32_t bytes[32];
} hamming_hash_t;

static void set_bit(hamming_hash_t *hash, unsigned long bit) {
	bit %= 1021;
	hash->bytes[bit / 32] |= 1 << (bit & 31);
}

static int popcount(uint32_t x) {
	x = x - ((x >> 1) & 0x55555555);
	x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
	return (((x + (x >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}

static int count_intersection(const hamming_hash_t *a, const hamming_hash_t *b) {
	int result = 0;
	for (int i = 0; i < (int)(sizeof(a->bytes) / sizeof(a->bytes[0])); i++) {
		result += popcount(a->bytes[i] & b->bytes[i]);
	}
	return result;
}

static int count_union(const hamming_hash_t *a, const hamming_hash_t *b) {
	int result = 0;
	for (int i = 0; i < (int)(sizeof(a->bytes) / sizeof(a->bytes[0])); i++) {
		result += popcount(a->bytes[i] | b->bytes[i]);
	}
	return result;
}
#endif
static int block_length(const char *data) {
	size_t bytes;
	int value = _t3_get_value(data, &bytes) >> 1;
	return value + bytes;
}
#if 1
// Fast enough but needs some more experimentation. Possibly a rolling hash implementation (buzhash) would be faster still.
// Also, possibly a bloom-filter like approach would be better.
static unsigned long hash_block(const char *str, int size) {
	unsigned long hash = 5381;
	for (int i = 0; i < size; i++) {
		int c = *str++;
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	}
	return hash;
}

void _t3_optimize_terminal(const t3_window_t *current_window, const t3_window_t *new_window) {
	hamming_hash_t *current_line_hashes = NULL;
	int *offsets = NULL;

	if ((current_line_hashes = malloc(current_window->height * sizeof(hamming_hash_t))) == NULL)
		goto clean_up;
	memset(current_line_hashes, 0, current_window->height * sizeof(hamming_hash_t));
	size_t offsets_size = (2 * current_window->height + 1) * sizeof(int);
	if ((offsets = malloc(offsets_size)) == NULL)
		goto clean_up;
	memset(offsets, 0, offsets_size);

	for (int i = 0; i < current_window->height; i++) {
		line_data_t *line = &current_window->lines[i];
		for (int j = 0; j < line->length; j += block_length(line->data + j)) {
			int k;
			int end = j;
			for (k = 0; end < line->length && k < CHUNK_SIZE; k++) {
				end += block_length(line->data + end);
			}
			if (k != CHUNK_SIZE)
				break;
			unsigned long hash_index = hash_block(line->data + j, end -j);
			set_bit(&current_line_hashes[i], hash_index);
		}
	}

	for (int i = 0; i < new_window->height; i++) {
		line_data_t *line = &new_window->lines[i];
		hamming_hash_t new_line_hash;
		memset(&new_line_hash, 0, sizeof(new_line_hash));
		for (int j = 0; j < line->length; j += block_length(line->data + j)) {
			int k;
			int end = j;
			for (k = 0; end < line->length && k < CHUNK_SIZE; k++) {
				end += block_length(line->data + end);
			}
			if (k != CHUNK_SIZE)
				break;
			unsigned long hash_index = hash_block(line->data + j, end -j);
			set_bit(&new_line_hash, hash_index);
		}
		for (int j = 0; j < current_window->height; j++) {
			float jaccard_similarity = (float)(count_intersection(&new_line_hash, &current_line_hashes[j])) / count_union(&new_line_hash, &current_line_hashes[j]);
			if (jaccard_similarity > 0.6) {
				//FIXME: check that this will not go out of bounds
				offsets[current_window->height + j - i]++;
			}
		}
	}

	int best_count = 0;
	int best_offset = 0;
	int second_best_count = 0;
	int second_best_offset = 0;
	for (int i = 0; i < 2 * current_window->height + 1; i++) {
		if (offsets[i] > best_count) {
			second_best_count = best_count;
			second_best_offset = best_offset;
			best_count = offsets[i];
			best_offset = i - current_window->height;
		}
	}
	lprintf("Best results: %d %d, %d %d\n", best_offset, best_count, second_best_offset, second_best_count);

clean_up:
	free(current_line_hashes);
	free(offsets);
}
#endif
#if 0
// Too slow!
int map_d(int d) {
	return abs(d) * 2 - (d < 0);
}

static t3_bool __attribute__((noinline)) equal_chars(const char *a, int a_size, const char *b, int b_size) {
	return a_size == b_size && memcmp(a, b, a_size) == 0;
}

static int line_diff(const char *current_line_data, int *current_line_offsets, int current_line_length,
		const char *new_line_data, int *new_line_offsets, int new_line_length, int *v) {
	int max = _t3_columns;
/*	if (max > 10)
		max = 10;
*/
	v[map_d(1)] = 0;
	for (int d = 0; d <= max; d++) {
		for (int k = -d; k <= d; k += 2) {
			int x = (k == -d || (k != d && v[map_d(k - 1)] < v[map_d(k + 1)])) ? v[map_d(k + 1)] : v[map_d(k - 1)] + 1;
			int y = x - k;
			while (x < current_line_length && y < new_line_length &&
					equal_chars(current_line_data + current_line_offsets[x], current_line_offsets[x + 1] - current_line_offsets[x],
							new_line_data + new_line_offsets[y], new_line_offsets[y + 1] - new_line_offsets[y])) {
				x++; y++;
			}
			v[map_d(k)] = x;
			if (x >= current_line_length && y >= new_line_length)
				return d;
		}
	}
	return _t3_columns;
}


void _t3_optimize_terminal(const t3_window_t *current_window, const t3_window_t *new_window) {
	int *v = NULL, *current_line_offsets = NULL, *new_line_offsets = NULL;

	if ((v = malloc(sizeof(int) * (_t3_columns * 2 + 1))) == NULL ||
			(current_line_offsets = malloc(sizeof(int) * (_t3_columns + 1))) == NULL ||
			(new_line_offsets = malloc(sizeof(int) * (_t3_columns + 1))) == NULL) {
		lprintf("Malloc problem; skipping optimization (%d)\n", _t3_columns);
		goto clean_up;
	}

	if (current_window->height != _t3_lines || new_window->height != _t3_lines) {
		lprintf("Error in window height; skipping optimization\n");
		goto clean_up;
	}

	for (int i = 0; i < _t3_lines; i++) {
		int k;
		int current_line_length = 0;
		for (k = 0; k < current_window->lines[i].length; current_line_length++) {
			current_line_offsets[current_line_length] = k;
			k += block_length(current_window->lines[i].data + k);
		}
		current_line_offsets[current_line_length] = k;
		for (int j = 0; j < _t3_lines; j++) {
			int new_line_length = 0;
			for (k = 0; k < new_window->lines[j].length; new_line_length++) {
				new_line_offsets[new_line_length] = k;
				k += block_length(new_window->lines[j].data + k);
			}
			new_line_offsets[new_line_length] = k;
			int diff = line_diff(current_window->lines[i].data, current_line_offsets, current_line_length,
					new_window->lines[j].data, new_line_offsets, new_line_length, v);
			lprintf("Difference for line %d - %d: %d\n", i, j, diff);
		}
	}

clean_up:
	free(v);
	free(current_line_offsets);
	free(new_line_offsets);

	/* FIXME: check the list of copy hints. Depending on the capabilities of the terminal, we can do the following:
	   - scroll a region. This only makes sense if doing so does not cause more updates. Depending on the
	     capabilities of the terminal, we can scroll specific lines, or only the whole screen.
	   - shift a region sideways. If the terminal supports ichn and dchn, this is always an option.
	     Here we also have to be careful to consider the possible optimizations in the correct order. although
	     we could for now simply use the optimization with the largest effect, which should alreayd provide
	     most of the gains.
	*/
	/* Free the copy hints. */
	copy_hint_t *ptr = _t3_copy_hint_head;
	while (ptr != NULL) {
		copy_hint_t *next_ptr = ptr->next;
		free(ptr);
		ptr = next_ptr;
	}
	_t3_copy_hint_head = NULL;
}
#endif
#if 0
// Too slow
void _t3_optimize_terminal(const t3_window_t *current_window, const t3_window_t *new_window) {
	uint8_t *combined_line = NULL;
	saidx_t *suffix_array = NULL;
	uint32_t *char_start_map;
	int max_current_length = 0;
	int max_new_length = 0;
	for (int i = 0; i < current_window->height; ++i) {
		if (current_window->lines[i].length > max_current_length) {
			max_current_length = current_window->lines[i].length;
		}
	}
	for (int i = 0; i < new_window->height; ++i) {
		if (new_window->lines[i].length > max_new_length) {
			max_new_length = new_window->lines[i].length;
		}
	}

	if ((combined_line = malloc(max_current_length + max_new_length + 1)) == NULL ||
			(suffix_array = malloc(sizeof(saidx_t) * (max_current_length + max_new_length + 1))) == NULL ||
			(char_start_map = malloc(sizeof(uint32_t) * (((max_current_length + max_new_length) / 32) + 1))) == NULL) {
		return;
	}

	for (int i = 0; i < current_window->height; i++) {
		memcpy(combined_line, current_window->lines[i].data, current_window->lines[i].length);
		combined_line[current_window->lines[i].length] = 0;
		for (int j = 0; j < new_window->height; j++) {
			int length = current_window->lines[i].length + 1;
			memcpy(combined_line + length, new_window->lines[j].data, new_window->lines[j].length);
			length += new_window->lines[j].length;
			memset(char_start_map, 0, 4 * ((max_current_length + max_new_length) / 32) + 1);
			for (int k = 0; k < length;) {
				char_start_map[k / 32] |= 1u << (k & 31);
				k += block_length(combined_line + k);
			}
			divsufsort(combined_line, suffix_array, length);
		}
	}

clean_up:
	free(combined_line);
	free(suffix_array);
}
#endif
