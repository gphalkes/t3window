/* Copyright (C) 2011-2012,2018 G.P. Halkes
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

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <transcript/transcript.h>
#include <unictype.h>
#include <uninorm.h>

#include "convert_output.h"
#include "curses_interface.h"
#include "generated/chardata.h"
#include "internal.h"
#include "utf8.h"
#include "window.h"

#define CONV_BUFFER_LEN (160)

static char *output_buffer;
static size_t output_buffer_size, output_buffer_idx;
static transcript_t *output_converter = NULL;

static char *nfc_output;
static size_t nfc_output_size;
static uint32_t replacement_char = '?';
static char replacement_char_str[16] = "?";
static size_t replacement_char_length = 1;

static void convert_replacement_char(uint32_t c);
static void print_replacement_character(void);

/** @internal
    @brief Initialize the output buffer used for accumulating output characters.
*/
t3_bool _t3_init_output_buffer(void) {
  if (output_buffer != NULL) return t3_true;
  output_buffer_size = 160;
  return (output_buffer = malloc(output_buffer_size)) != NULL;
}

/** @internal
    @brief Free all memory associated with the output conversion.
*/
void _t3_free_output_buffer(void) {
  if (output_buffer != NULL) {
    free(output_buffer);
    output_buffer = NULL;
  }
  if (output_converter != NULL) {
    transcript_close_converter(output_converter);
    output_converter = NULL;
  }
  if (nfc_output != NULL) {
    free(nfc_output);
    nfc_output_size = 0;
    nfc_output = NULL;
  }
}

/** @internal
    @brief Initialize the characterset conversion used for output.
    @param encodig The encoding to convert to.
*/
t3_bool _t3_init_output_converter(const char *encoding) {
  char squashed_name[10];

  if (output_converter != NULL) transcript_close_converter(output_converter);

  transcript_normalize_name(encoding, squashed_name, sizeof(squashed_name));
  if (strcmp(squashed_name, "utf8") == 0) {
    output_converter = NULL;
    return t3_true;
  }

  if ((output_converter = transcript_open_converter(encoding, TRANSCRIPT_UTF8, 0, NULL)) == NULL)
    return t3_false;

  convert_replacement_char(replacement_char);

  return t3_true;
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

    if ((SIZE_MAX >> 1) > output_buffer_size) output_buffer_size <<= 1;
    retval = realloc(output_buffer, output_buffer_size);
    if (retval == NULL) return t3_false;
    output_buffer = retval;
  }
  output_buffer[output_buffer_idx++] = c;
  return t3_true;
}

/** Add a string to the output buffer.
    @ingroup t3window_term
    @param s The string to add.

    This function should not be used outside the callback set with ::t3_term_set_user_callback.
    @internal
    Contrary to the previous comment for users of the library, this function is also
    used from term_update.
*/
t3_bool t3_term_puts(const char *s) {
  t3_bool retval = t3_true;
  while (*s != 0) retval &= t3_term_putc(*s++);
  return retval;
}

/** Add a string to the output buffer.
    @ingroup t3window_term
    @param s The string to add.
    @param n The length of @p s.

    This function should not be used outside the callback set with ::t3_term_set_user_callback.
    @internal
    Contrary to the previous comment for users of the library, this function is also
    used from term_update.
*/
t3_bool t3_term_putn(const char *s, size_t n) {
  t3_bool retval = t3_true;
  for (; n != 0; n--) retval &= t3_term_putc(*s++);
  return retval;
}

/** @internal
    @brief Print the characters in the output buffer.
*/
void _t3_output_buffer_print(void) {
  char *tmp_nfc_output;
  size_t nfc_output_len;
  if (output_buffer_idx == 0) return;

  nfc_output_len = nfc_output_size;
  tmp_nfc_output = (char *)u8_normalize(UNINORM_NFC, (const uint8_t *)output_buffer,
                                        output_buffer_idx, (uint8_t *)nfc_output, &nfc_output_len);
  if (tmp_nfc_output != nfc_output) {
    free(nfc_output);
    nfc_output = tmp_nfc_output;
    nfc_output_size = nfc_output_len;
  }

  // FIXME: for GB18030 we should also take the first option. However, it does need conversion...
  if (output_converter == NULL) {
#if 1
    size_t idx, codepoint_len, output_start;
    uint32_t c;
    uint_fast8_t available_since;
    /* Filter out combining characters if the terminal is known not to support them (e.g.
       gnome-terminal).
       If the character is accepted, then don't do anything. If the character has to be filtered
       out, print
       the acceptable characters up to now and then print replacement characters if needed, moving
       the
       acceptable pointer up to after the character to skip. */
    /* FIXME: should we filter out other zero-width characters? */
    for (idx = 0, output_start = 0; idx < nfc_output_len; idx += codepoint_len) {
      codepoint_len = nfc_output_len - idx;
      c = t3_utf8_get(nfc_output + idx, &codepoint_len);
      available_since = get_chardata(c) & 0x3f;

      if (_t3_term_combining < available_since &&
          uc_is_general_category_withtable(c, UC_CATEGORY_MASK_M)) {
        fwrite(nfc_output + output_start, 1, idx - output_start, _t3_putp_file);
        /* For non-zero width combining characters, print a replacement character. */
        if (t3_utf8_wcwidth(c) == 1) print_replacement_character();
        output_start = idx + codepoint_len;
      }
      if (_t3_term_double_width < available_since && t3_utf8_wcwidth(c) == 2) {
        if (_t3_term_double_width < 0) {
          fwrite(nfc_output + output_start, 1, idx - output_start, _t3_putp_file);
          print_replacement_character();
          print_replacement_character();
        } else {
          fwrite(nfc_output + output_start, 1, idx - output_start + codepoint_len, _t3_putp_file);
          /* Add a space to compensate for the lack of double width characters. */
          fputc(' ', _t3_putp_file);
        }
        output_start = idx + codepoint_len;
      }
    }
    fwrite(nfc_output + output_start, 1, idx - output_start, _t3_putp_file);
#else
    fwrite(nfc_output, 1, nfc_output_len, _t3_putp_file);
#endif
  } else {
    char conversion_output[CONV_BUFFER_LEN], *conversion_output_ptr;
    const char *conversion_input_ptr = nfc_output,
               *conversion_input_end = nfc_output + nfc_output_len;

    /* Convert UTF-8 sequence into current output encoding using transcript_from_unicode. */
    while (conversion_input_ptr < conversion_input_end) {
      conversion_output_ptr = conversion_output;
      switch (transcript_from_unicode(output_converter, &conversion_input_ptr, conversion_input_end,
                                      &conversion_output_ptr, conversion_output + CONV_BUFFER_LEN,
                                      TRANSCRIPT_END_OF_TEXT)) {
        default: {
          /* Conversion did not succeed on this character; print chars with length equal to char.
             Possible reasons include unassigned characters, fallbacks. Others should not happen. */
          size_t char_len = conversion_input_end - conversion_input_ptr;
          int width;
          uint32_t c;

          /* First write all output that has been converted. */
          if (conversion_output_ptr != conversion_output)
            fwrite(conversion_output, 1, conversion_output_ptr - conversion_output, _t3_putp_file);

          c = t3_utf8_get(conversion_input_ptr, &char_len);
          conversion_input_ptr += char_len;

          /* Ensure that the conversion ends in the 'initial state', because after this we will
             be outputing replacement characters. */
          conversion_output_ptr = conversion_output;
          transcript_from_unicode_flush(output_converter, &conversion_output_ptr,
                                        conversion_output + CONV_BUFFER_LEN);
          if (conversion_output_ptr != conversion_output)
            fwrite(conversion_output, 1, conversion_output_ptr - conversion_output, _t3_putp_file);

          for (width = t3_utf8_wcwidth(c); width > 0; width--) print_replacement_character();

          break;
        }
        case TRANSCRIPT_ILLEGAL_END:
          /* This should only happen if there is an incomplete UTF-8 character at the end of
             the buffer. Not much we can do about that... */
          break;
        case TRANSCRIPT_NO_SPACE:
          /* Not enough space in output buffer. Flush current contents and continue. */
          fwrite(conversion_output, 1, conversion_output_ptr - conversion_output, _t3_putp_file);
          break;
        case TRANSCRIPT_SUCCESS:
          fwrite(conversion_output, 1, conversion_output_ptr - conversion_output, _t3_putp_file);
          break;
      }
    }
    /* Ensure that the conversion ends in the 'initial state', because after this we will
       be outputing escape sequences. */
    conversion_output_ptr = conversion_output;
    transcript_from_unicode_flush(output_converter, &conversion_output_ptr,
                                  conversion_output + CONV_BUFFER_LEN);
    if (conversion_output_ptr != conversion_output)
      fwrite(conversion_output, 1, conversion_output_ptr - conversion_output, _t3_putp_file);
  }
  output_buffer_idx = 0;
}

/** Determine if the terminal can draw a character.
    @ingroup t3window_term
    @param str The UTF-8 string representing the character to be displayed.
    @param str_len The length of @a str.
    @return A @a boolean indicating to whether the terminal is able to correctly
        draw the character.

    Note that @a str may contain combining characters, which will only be drawn
    correctly if a precomposed character is available or the terminal supports
    them. And even if the terminal supports combining characters they _may_ not
    be correctly rendered, depending on the combination of combining marks.

    Moreover, the font the terminal is using may not have a glyph available
    for the requested character. Therefore, if this function determines that
    the character can be drawn, it may still not be correctly represented on
    screen.
*/
t3_bool t3_term_can_draw(const char *str, size_t str_len) {
  size_t nfc_output_len;

  if (str_len > 1 || nfc_output == NULL) {
    char *tmp_nfc_output;
    nfc_output_len = nfc_output_size;
    tmp_nfc_output = (char *)u8_normalize(UNINORM_NFC, (const uint8_t *)str, str_len,
                                          (uint8_t *)nfc_output, &nfc_output_len);
    if (tmp_nfc_output != nfc_output) {
      free(nfc_output);
      nfc_output = tmp_nfc_output;
      nfc_output_size = nfc_output_len;
    }
  } else {
    nfc_output[0] = *str;
    nfc_output_len = 1;
  }

  if (output_converter == NULL) {
    size_t idx, codepoint_len;
    uint_fast8_t available_since;
    uint32_t c;

    /* FIXME: should we filter out control characters, like in t3_win_addnstr? */
    if (nfc_output_len == 1) return t3_true;

    for (idx = 0; idx < nfc_output_len; idx += codepoint_len) {
      codepoint_len = nfc_output_len - idx;
      c = t3_utf8_get(nfc_output + idx, &codepoint_len);
      available_since = get_chardata(c) & 0x3f;

      if (_t3_term_combining < available_since &&
          uc_is_general_category_withtable(c, UC_CATEGORY_MASK_M))
        return t3_false;
      if (_t3_term_double_width < available_since && t3_utf8_wcwidth(c) == 2) return t3_false;
    }
    return t3_true;
  } else {
    char conversion_output[CONV_BUFFER_LEN], *conversion_output_ptr;
    const char *conversion_input_ptr = nfc_output,
               *conversion_input_end = nfc_output + nfc_output_len;

    /* Convert UTF-8 sequence into current output encoding using transcript_from_unicode. */
    while (conversion_input_ptr < conversion_input_end) {
      conversion_output_ptr = conversion_output;
      switch (transcript_from_unicode(output_converter, &conversion_input_ptr, conversion_input_end,
                                      &conversion_output_ptr, conversion_output + CONV_BUFFER_LEN,
                                      TRANSCRIPT_END_OF_TEXT)) {
        default:
          /* Reset conversion to initial state. */
          transcript_from_unicode_reset(output_converter);
          return t3_false;
        case TRANSCRIPT_SUCCESS:
        case TRANSCRIPT_NO_SPACE: /* Not enough space in output buffer. Restart conversion with
                                     remaining chars. */
          break;
      }
    }
    /* Reset conversion to initial state. */
    transcript_from_unicode_reset(output_converter);
    return t3_true;
  }
}

/** Convert the given replacment character. */
static void convert_replacement_char(uint32_t c) {
  char utf8_buffer[5];
  char *conversion_output_ptr;
  const char *conversion_input_ptr;

  if (output_converter == NULL) return;

  memset(utf8_buffer, 0, sizeof(replacement_char));
  t3_utf8_put(c, utf8_buffer);

  conversion_input_ptr = (const char *)&utf8_buffer;
  conversion_output_ptr = replacement_char_str;

  if (transcript_from_unicode(output_converter, &conversion_input_ptr,
                              utf8_buffer + strlen(utf8_buffer), &conversion_output_ptr,
                              replacement_char_str + sizeof(replacement_char_str),
                              TRANSCRIPT_END_OF_TEXT) == TRANSCRIPT_SUCCESS &&
      transcript_from_unicode_flush(output_converter, &conversion_output_ptr,
                                    replacement_char_str + sizeof(replacement_char_str)) ==
          TRANSCRIPT_SUCCESS) {
    replacement_char_length = conversion_output_ptr - replacement_char_str;
    return;
  }

  if (c != '?') {
    /* Fallback to question mark if for some reason it doesn't work. */
    convert_replacement_char('?');
  } else {
    /* If all else fails, hope that the bare question mark prints. */
    replacement_char_str[0] = '?';
    replacement_char_length = 1;
  }
}

/** Set the replacement character used for undrawable characters.
    @ingroup t3window_term
    @param c The character to draw when an undrawable characters is encountered.

    The default character is the question mark ('?'). The character must be a
    Unicode character representable in the current character set. For terminals
    capable of Unicode output the Replacement Character is used (codepoint FFFD).
*/
void t3_term_set_replacement_char(int c) {
  replacement_char = c;
  convert_replacement_char(replacement_char);
}

/** Print the replacement character. */
static void print_replacement_character(void) {
  if (output_converter == NULL) {
    fwrite("\xef\xbf\xbd", 1, 3, _t3_putp_file);
  } else {
    fwrite(replacement_char_str, 1, replacement_char_length, _t3_putp_file);
  }
}
