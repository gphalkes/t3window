/* Copyright (C) 2010 G.P. Halkes
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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include "convert_output.h"
#include "window.h"
#include "internal.h"
#include "unicode.h"
#include "charconv.h"

#define CONV_BUFFER_LEN (160)

static char *output_buffer;
static size_t output_buffer_size, output_buffer_idx;
static charconv_t *output_convertor = NULL;

static char *nfc_output;
static size_t nfc_output_size;
static char replacement_char = '?';

/** @internal
    @brief Initialize the output buffer used for accumulating output characters.
*/
t3_bool _t3_init_output_buffer(void) {
	output_buffer_size = 160;
	return (output_buffer = malloc(output_buffer_size)) != NULL;
}

/** @internal
    @brief Initialize the characterset conversion used for output.
    @param encodig The encoding to convert to.
*/
t3_bool _t3_init_output_convertor(const char *encoding) {
	if (output_convertor != NULL)
		charconv_close_convertor(output_convertor);

	/* FIXME: use case-insensitive compare and check for other "spellings" of UTF-8 */
	if (strcmp(encoding, "UTF-8") == 0) {
		output_convertor = NULL;
		return t3_true;
	}

	return (output_convertor = charconv_open_convertor(encoding, CHARCONV_UTF8, 0, NULL)) != NULL;
}

/** Add a charater to the output buffer.
    @ingroup t3window_term
    @param c The character to add.

    The character passed as @p c is a single @c char, not a unicode codepoint. This function
    should not be used outside the callback set with ::t3_term_set_user_callback.
    @internal
    Contrary to the previous comment for users of the library, this function is also
    used from term_update.
*/
t3_bool t3_term_putc(char c) {
	if (output_buffer_idx >= output_buffer_size) {
		char *retval;

		if ((SIZE_MAX >> 1) > output_buffer_size)
			output_buffer_size <<= 1;
		retval = realloc(output_buffer, output_buffer_size);
		if (retval == NULL)
			return t3_false;
		output_buffer = retval;
	}
	output_buffer[output_buffer_idx++] = c;
	return t3_true;
}

/** @internal
    @brief Print the characters in the output buffer.
*/
void _t3_output_buffer_print(void) {
	size_t nfc_output_len;
	if (output_buffer_idx == 0)
		return;
	//FIXME: check return value!
	nfc_output_len = t3_to_nfc(output_buffer, output_buffer_idx, &nfc_output, &nfc_output_size);

	if (output_convertor == NULL) {
		//FIXME: filter out combining characters if the terminal is known not to support them! (gnome-terminal)
		fwrite(nfc_output, 1, nfc_output_len, stdout);
	} else {
		char conversion_output[CONV_BUFFER_LEN], *conversion_output_ptr;
		const char *conversion_input_ptr = nfc_output, *conversion_input_end = nfc_output + nfc_output_len;

		/* Convert UTF-8 sequence into current output encoding using t3_unicode_export. */
		while (conversion_input_ptr < conversion_input_end) {
			conversion_output_ptr = conversion_output;
			switch (charconv_from_unicode(output_convertor, &conversion_input_ptr, conversion_input_end,
					&conversion_output_ptr, conversion_output + CONV_BUFFER_LEN, CHARCONV_END_OF_TEXT))
			{
				default: {
					/* Conversion did not succeed on this character; print chars with length equal to char.
					   Possible reasons include unassigned characters, fallbacks. Others should not happen. */
					size_t char_len = conversion_input_end - conversion_input_ptr;
					int width;
					uint32_t c;

					/* First write all output that has been converted. */
					if (conversion_output_ptr != conversion_output)
						fwrite(conversion_output, 1, conversion_output_ptr - conversion_output, stdout);

					c = t3_getuc(conversion_input_ptr, &char_len);
					conversion_input_ptr += char_len;

					/* Ensure that the conversion ends in the 'initial state', because after this we will
					   be outputing replacement characters. */
					conversion_output_ptr = conversion_output;
					charconv_from_unicode_flush(output_convertor, &conversion_output_ptr, conversion_output + CONV_BUFFER_LEN);
					if (conversion_output_ptr != conversion_output)
						fwrite(conversion_output, 1, conversion_output_ptr - conversion_output, stdout);

					/* FIXME: this assumes that the given character is actually valid in the output
					   encoding, which is of course not necessarily the case. It should be converted
					   first!. */
					for (width = T3_INFO_TO_WIDTH(t3_get_codepoint_info(c)); width > 0; width--)
						putchar(replacement_char);

					break;
				}
				case CHARCONV_ILLEGAL_END:
					/* This should only happen if there is an incomplete UTF-8 character at the end of
					   the buffer. Not much we can do about that... */
					break;
				case CHARCONV_NO_SPACE:
					/* Not enough space in output buffer. Flush current contents and continue. */
					fwrite(conversion_output, 1, conversion_output_ptr - conversion_output, stdout);
					break;
				case CHARCONV_SUCCESS:
					fwrite(conversion_output, 1, conversion_output_ptr - conversion_output, stdout);
					break;
			}
		}
		/* Ensure that the conversion ends in the 'initial state', because after this we will
		   be outputing escape sequences. */
		conversion_output_ptr = conversion_output;
		charconv_from_unicode_flush(output_convertor, &conversion_output_ptr, conversion_output + CONV_BUFFER_LEN);
		if (conversion_output_ptr != conversion_output)
			fwrite(conversion_output, 1, conversion_output_ptr - conversion_output, stdout);
	}
	output_buffer_idx = 0;
}

/** Determine if the terminal can draw a character.
    @ingroup t3window_term
    @param str The UTF-8 string representing the character to be displayed.
    @param str_len The length of @a str.
    @return A @a DrawType indicating to what extent the terminal is able to draw
        the character.

    Note that @a str may contain combining characters, which will only be drawn
    correctly if a precomposed character is available or the terminal supports
    them. And even if the terminal supports combining characters they _may_ not
    be correctly rendered, depending on the combination of combining marks.
*/
t3_bool t3_term_can_draw(const char *str, size_t str_len) {
	size_t idx, codepoint_len;
	size_t nfc_output_len = t3_to_nfc(str, str_len, &nfc_output, &nfc_output_size);
	uint32_t c;

	if (output_convertor == NULL) {
		//FIXME: make this dependent on the detected terminal capabilities
		for (idx = 0; idx < nfc_output_len; idx += codepoint_len) {
			codepoint_len = nfc_output_len - idx;
			c = t3_getuc(nfc_output + idx, &codepoint_len);
			if (t3_get_codepoint_info(c) & T3_COMBINING_BIT)
				return t3_false;
		}
		return t3_true;
	} else {
		char conversion_output[CONV_BUFFER_LEN], *conversion_output_ptr;
		const char *conversion_input_ptr = nfc_output, *conversion_input_end = nfc_output + nfc_output_len;

		/* Convert UTF-8 sequence into current output encoding using t3_unicode_export. */
		while (conversion_input_ptr < conversion_input_end) {
			conversion_output_ptr = conversion_output;
			switch (charconv_from_unicode(output_convertor, &conversion_input_ptr, conversion_input_end,
					&conversion_output_ptr, conversion_output + CONV_BUFFER_LEN, CHARCONV_END_OF_TEXT))
			{
				default:
					/* Reset conversion to initial state. */
					charconv_from_unicode_reset(output_convertor);
					return t3_false;
				case CHARCONV_SUCCESS:
				case CHARCONV_NO_SPACE:	/* Not enough space in output buffer. Restart conversion with remaining chars. */
					break;
			}
		}
		/* Reset conversion to initial state. */
		charconv_from_unicode_reset(output_convertor);
		return t3_true;
	}
}

/** Set the replacement character used for undrawable characters.
    @ingroup t3window_term
    @param c The character to draw when an undrawable characters is encountered.

    The default character is the question mark ('?'). The character must be an
    ASCII character. For terminals capable of Unicode output the Replacement
    Character is used (codepoint FFFD).
*/
void t3_term_set_replacement_char(char c) {
	replacement_char = c;
}

