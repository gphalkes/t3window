#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <iconv.h>
#include <errno.h>

#include "terminal.h"
#include "window.h"
#include "internal.h"
#include "unicode/tdunicode.h"

#define CONV_BUFFER_LEN (160)

static char *output_buffer;
static size_t output_buffer_size, output_buffer_idx;
static iconv_t output_iconv = (iconv_t) -1;

static char *nfc_output;
static size_t nfc_output_size;


Bool init_output_buffer(void) {
	output_buffer_size = 160;
	return (output_buffer = malloc(output_buffer_size)) != NULL;
}

Bool init_output_iconv(const char *encoding) {
	if (output_iconv != (iconv_t) -1)
		iconv_close(output_iconv);

	if (strcmp(encoding, "UTF-8") == 0) {
		output_iconv = (iconv_t) -1;
		return True;
	}

	return (output_iconv = iconv_open(encoding, "UTF-8")) != (iconv_t) -1;
}

Bool output_buffer_add(char c) {
	if (output_buffer_idx >= output_buffer_size) {
		char *retval;

		if (SIZE_MAX >> 1 > output_buffer_size)
			output_buffer_size <<= 1;
		retval = realloc(output_buffer, output_buffer_size);
		if (retval == NULL)
			return False;
		output_buffer = retval;
	}
	output_buffer[output_buffer_idx++] = c;
	return True;
}

void output_buffer_print(void) {
	size_t nfc_output_len;
	if (output_buffer_idx == 0)
		return;
	//FIXME: check return value!
	nfc_output_len = tdu_to_nfc(output_buffer, output_buffer_idx, &nfc_output, &nfc_output_size);

	if (output_iconv == (iconv_t) -1) {
		//FIXME: filter out combining characters if the terminal is known not to support them! (gnome-terminal)
		fwrite(nfc_output, 1, nfc_output_len, stdout);
	} else {
		char conversion_output[CONV_BUFFER_LEN], *conversion_output_ptr = conversion_output,
			*conversion_input = nfc_output;
		size_t input_len = nfc_output_len, output_len = CONV_BUFFER_LEN, retval;

		/* Convert UTF-8 sequence into current output encoding using iconv. */
		while (input_len > 0) {
			retval = iconv(output_iconv, &conversion_input, &input_len, &conversion_output_ptr, &output_len);
			if (retval == (size_t) -1) {
				switch (errno) {
					case EILSEQ:
						//FIXME: ensure that the output_buffer actually contains the number of bytes being skipped!
						/* Conversion did not succeed on this character; skip. */
						switch ((*conversion_input) & 0xF0) {
							default:
							/* case 0x80: case 0x90: case 0xA0: case 0xB0: */
								conversion_input++;
								input_len--;
								break;
							case 0xC0: case 0xD0:
								conversion_input += 2;
								input_len -= 2;
								break;
							case 0xE0:
								conversion_input += 3;
								input_len -= 3;
								break;
							case 0xF0:
								conversion_input += 4;
								input_len -= 4;
								break;
						}
						break;
					case EINVAL:
						/* This should only happen if there is an incomplete UTF-8 character at the end of
						   the buffer. Not much we can do about that... */
						input_len = 0;
						break;
					case E2BIG:
						/* Not enough space in output buffer. Flush current contents and continue. */
						fwrite(conversion_output, 1, CONV_BUFFER_LEN - output_len, stdout);
						conversion_output_ptr = conversion_output;
						output_len = CONV_BUFFER_LEN;
						break;
					default:
						//FIXME: what to do here?
						break;
				}
			} else {
				fwrite(conversion_output, 1, CONV_BUFFER_LEN - output_len, stdout);
			}
		}
		/* Ensure that the conversion ends in the 'initial state', because after this we will
		   be outputing escape sequences. */
		conversion_output_ptr = conversion_output;
		output_len = CONV_BUFFER_LEN;
		iconv(output_iconv, NULL, NULL, &conversion_output_ptr, &output_len);
		if (output_len < CONV_BUFFER_LEN)
			fwrite(conversion_output, 1, CONV_BUFFER_LEN - output_len, stdout);
	}
	output_buffer_idx = 0;
}

/** Determine if the terminal can draw a character.
    @param str The UTF-8 string representing the character to be displayed.
    @param str_len The length of @a str.
    @return A @a DrawType indicating to what extent the terminal is able to draw
        the character.

    Note that @a str may contain combining characters, which will only be drawn
    correctly if a precomposed character is available or the terminal supports
    them. And even if the terminal supports combining characters they _may_ not
    be correctly rendered, depending on the combination of combining marks.
*/
DrawType term_can_draw(const char *str, size_t str_len) {
	size_t nfc_output_len = tdu_to_nfc(str, str_len, &nfc_output, &nfc_output_size);

	if (output_iconv == (iconv_t) -1) {
		for (idx = 0; idx < nfc_output_len; idx += codepoint_len) {
			codepoint_len = nfc_output_len - idx;
			c = tdu_getuc(nfc_output + idx, &codepoint_len);
			if (tdu_get_info(c) & TDU_COMBINING_BIT)
				return DRAW_PARTIAL;
		}
		return DRAW_FULL;
	} else {
		//FIXME: do conversion and return result. May not be possible to draw at all!


	}
}
