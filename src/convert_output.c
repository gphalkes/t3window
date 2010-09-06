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
#include <iconv.h>

#include "window.h"
#include "internal.h"
#include "unicode.h"

#define CONV_BUFFER_LEN (160)

typedef void *t3_export_t;

static char *output_buffer;
static size_t output_buffer_size, output_buffer_idx;
static t3_export_t output_iconv = (t3_export_t) -1;

static char *nfc_output;
static size_t nfc_output_size;
static char replacement_char = '?';

static int unicode_export_fast(t3_export_t, char **, size_t *, char **, size_t *);
static int unicode_export_conservative(t3_export_t, char **, size_t *, char **, size_t *);
static int (*unicode_export)(t3_export_t, char **, size_t *, char **, size_t *) = unicode_export_conservative;

#define CONVBUF_SIZE 32
typedef struct {
	iconv_t handle;
	char convbuf[CONVBUF_SIZE];
	size_t convbuf_size;
	t3_bool saved;
} export_data;

static t3_bool check_utf8_validity(const char *buf) {
	size_t discard;
	uint32_t c = t3_getuc(buf, &discard);
	const unsigned char *_buf = (const unsigned char *) buf;

	/* t3_getuc returns a replacement character if the sequence is not valid. However,
	   the buffer may actually contain the replacement character so only return false
	   if that is not the case. */
	if (c == 0xFFFD && !(_buf[0] == 0xEF && _buf[1] == 0xBF && _buf[2] == 0xDB))
		return t3_false;
	return t3_true;
}


static int unicode_export_fast(t3_export_t handle, char **inbuf, size_t *inbytesleft, char **outbuf, size_t *outbytesleft) {
	/* libiconv and glibc stop with EILSEQ on an unavailable character. Therefore
	   we can simply try to do the conversion in one go, and if it fails with
	   EILSEQ, determine whether the next character is valid UTF-8 to see which
	   of the two possible cases we are in.
	*/
	size_t retval;

	retval = iconv(((export_data *) handle)->handle, inbuf, inbytesleft, outbuf, outbytesleft);
	if (retval == (size_t) -1) {
		if (errno == EILSEQ && check_utf8_validity(*inbuf))
			return ENOTSUP;
		return errno;
	}
	/* The conversion should not return anything other than 0 or an error, so
	   we don't check here anymore. */
	return 0;
}

static int unicode_export_conservative(t3_export_t handle, char **inbuf, size_t *inbytesleft, char **outbuf, size_t *outbytesleft) {
	/* Some iconv libraries replace unavailable output characters with a fixed,
	   but unknown character (usually a question mark or NUL byte). We won't be
	   able to find out about this until it is too late. Furthermore, if the
	   conversion fails, we won't even be told about the fact that this has
	   happened. Therefore we just convert one UTF-8 codepoint at a time, such
	   that we at least have the full picture.
	*/
	export_data *_handle = (export_data *) handle;
	size_t charsize, saved_charsize, outbytes;
	char *outbuf_ptr;
	char *saved_inbuf, *saved_outbuf;
	size_t saved_inbytesleft, saved_outbytesleft;

	if (_handle->saved) {
		if (outbuf != NULL) {
			outbytes = CONVBUF_SIZE - _handle->convbuf_size;
			if (outbytes > *outbytesleft)
				return E2BIG;

			memcpy(*outbuf, _handle->convbuf, outbytes);
			*outbuf += outbytes;
			*outbytesleft -= outbytes;
		}

		_handle->saved = t3_false;
	}

	/* Reset the state of the iconv conversion. We do this so we have a known state
	   to which we can return if a non-reversible conversion is performed and we
	   have to roll-back the conversion. */
	if (iconv(_handle->handle, NULL, NULL, outbuf, outbytesleft) == (size_t) -1)
		return errno;

	if (inbuf == NULL)
		return 0;

	saved_inbuf = *inbuf;
	saved_inbytesleft = *inbytesleft;
	saved_outbuf = *outbuf;
	saved_outbytesleft = *outbytesleft;

	while (*inbytesleft > 0 && (outbytesleft == NULL || *outbytesleft > 0)) {
		switch ((**inbuf) & 0xF0) {
			case 0xF0:
				charsize = 4;
				break;
			case 0xE0:
				charsize = 3;
				break;
			case 0xC0:
			case 0xD0:
				charsize = 2;
				break;
			default:
				charsize = 1;
				break;
		}
		if (*inbytesleft < charsize)
			return EINVAL;
		saved_charsize = charsize;

		outbuf_ptr = _handle->convbuf;
		_handle->convbuf_size = CONVBUF_SIZE;
		switch (iconv(_handle->handle, inbuf, &charsize, &outbuf_ptr, &_handle->convbuf_size)) {
			case -1:
				if (errno == EILSEQ && check_utf8_validity(*inbuf))
					return ENOTSUP;
				return errno;
			case 0:
				outbytes = CONVBUF_SIZE - _handle->convbuf_size;
				*inbytesleft -= saved_charsize;
				if (outbytes > *outbytesleft) {
					_handle->saved = t3_true;
					return E2BIG;
				}
				memcpy(*outbuf, _handle->convbuf, outbytes);
				*outbuf += outbytes;
				*outbytesleft -= outbytes;
				break;
			default: {
				/* Now we are in a pickle. Iconv did something we do not want, but which we
				   can not avoid: it performed an non-reversible conversion. This means that
				   it probably inserted some character that we do not want there. So we roll-back
				   the state to what it was before the conversion, and start the conversion
				   again. Only this time, we know exactly which part of the input is convertible,
				   so we can do the conversion in one go.
				*/
				size_t to_convert = (*inbuf - saved_inbuf) - saved_charsize;
				if (iconv(_handle->handle, NULL, NULL, NULL, NULL) == (size_t) -1)
					return errno;

				*inbuf = saved_inbuf;
				*outbuf = saved_outbuf;
				*inbytesleft = saved_inbytesleft - to_convert;
				*outbytesleft = saved_outbytesleft;

				if (to_convert == 0)
					return ENOTSUP;

				if (iconv(_handle->handle, inbuf, &to_convert, outbuf, outbytesleft) == (size_t) -1)
					return errno;

				return ENOTSUP;
			}
		}
	}
	return 0;
}

/** Open an export character set for ::unicode_export.
    @param charset The name of the character set to convert to.

    This function wraps ::iconv_open and returns either a ::t3_export_t or
    @c (t3_export_t) @c -1.
*/
static t3_export_t unicode_open_export(const char *charset) {
	static t3_bool export_initalized = t3_false;
	export_data *retval;

	if (!export_initalized) {
		iconv_t handle;
		char _in[2] = "\xc3\xa1", *in = _in;
		char _out[10], *out = _out;
		size_t inlen = 2, outlen = 10;

		/* FIXME: add more names for ASCII that are in common use. */
		if ((handle = iconv_open("ASCII", "UTF-8")) != (iconv_t) -1 ||
			(handle = iconv_open("IBM367", "UTF-8")) != (iconv_t) -1) {
			if (iconv(handle, &in, &inlen, &out, &outlen) == (size_t) -1 && errno == EILSEQ)
				unicode_export = unicode_export_fast;
			iconv_close(handle);
		}
		export_initalized = t3_true;
	}

	if ((retval = malloc(sizeof(export_data))) == NULL) {
		errno = ENOMEM;
		return (t3_export_t) -1;
	}

	retval->saved = t3_false;

	if ((retval->handle = iconv_open(charset, "UTF-8")) == NULL) {
		int saved_errno = errno;
		free(retval);
		errno = saved_errno;
		return (t3_export_t) -1;
	}

	return (t3_export_t) retval;
}

/** Close an export character set. */
static void unicode_close_export(t3_export_t handle) {
	export_data *_handle = (export_data *) handle;
	iconv_close(_handle->handle);
	free(handle);
}


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
t3_bool _t3_init_output_iconv(const char *encoding) {
	if (output_iconv != (t3_export_t) -1)
		unicode_close_export(output_iconv);

	/* FIXME: use case-insensitive compare and check for other "spellings" of UTF-8 */
	if (strcmp(encoding, "UTF-8") == 0) {
		output_iconv = (t3_export_t) -1;
		return t3_true;
	}

	return (output_iconv = unicode_open_export(encoding)) != (t3_export_t) -1;
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
/* FIXME: The following was taken from gnulib striconv.c:

	 Irix iconv() inserts a NUL byte if it cannot convert.
     NetBSD iconv() inserts a question mark if it cannot convert.
     Only GNU libiconv and GNU libc are known to prefer to fail rather
     than doing a lossy conversion.  For other iconv() implementations,
     we have to look at the number of irreversible conversions returned;
     but this information is lost when iconv() returns for an E2BIG reason.
     Therefore we cannot use the second, faster algorithm.

	This means that we won't have any control over how we print such characters, unless we
	actually convert character by character, checking for irreversible conversions in
	the process. Of course the E2BIG "error" will prevent us from checking this if the
	output buffer is smaller than needed for the full conversion (nice API design guys...).

	Maybe we can implement some kind of binary like searching, by first trying to convert
	the whole string and if that contains some irreversible converion try the first half etc.

	Associated ifdef from same file:
	# if !defined _LIBICONV_VERSION && !defined __GLIBC__
*/

	size_t nfc_output_len;
	if (output_buffer_idx == 0)
		return;
	//FIXME: check return value!
	nfc_output_len = t3_to_nfc(output_buffer, output_buffer_idx, &nfc_output, &nfc_output_size);

	if (output_iconv == (t3_export_t) -1) {
		//FIXME: filter out combining characters if the terminal is known not to support them! (gnome-terminal)
		fwrite(nfc_output, 1, nfc_output_len, stdout);
	} else {
		char conversion_output[CONV_BUFFER_LEN], *conversion_output_ptr = conversion_output,
			*conversion_input_ptr = nfc_output;
		size_t input_len = nfc_output_len, output_len = CONV_BUFFER_LEN;
		int retval;

		/* Convert UTF-8 sequence into current output encoding using iconv. */
		while (input_len > 0) {
			retval = unicode_export(output_iconv, &conversion_input_ptr, &input_len, &conversion_output_ptr, &output_len);
			switch (retval) {
				case EILSEQ:
					/* Handle illegal sequences (which shouldn't occur anyway) the same was
					   unsupported characters. This way we at least make progress, and hope
					   for the best. */
					/* FALLTHROUGH */
				case ENOTSUP: {
					/* Conversion did not succeed on this character; print chars with length equal to char. */
					size_t char_len = input_len;
					int width;
					uint32_t c;

					/* First write all output that has been converted. */
					if (output_len < CONV_BUFFER_LEN)
						fwrite(conversion_output, 1, CONV_BUFFER_LEN - output_len, stdout);

					c = t3_getuc(conversion_input_ptr, &char_len);
					conversion_input_ptr += char_len;
					input_len -= char_len;

					/* Ensure that the conversion ends in the 'initial state', because after this we will
					   be outputing replacement characters. */
					conversion_output_ptr = conversion_output;
					output_len = CONV_BUFFER_LEN;
					unicode_export(output_iconv, NULL, NULL, &conversion_output_ptr, &output_len);
					if (output_len < CONV_BUFFER_LEN)
						fwrite(conversion_output, 1, CONV_BUFFER_LEN - output_len, stdout);

					for (width = T3_INFO_TO_WIDTH(t3_get_codepoint_info(c)); width > 0; width--)
						putchar(replacement_char);

					break;
				}
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
					fwrite(conversion_output, 1, CONV_BUFFER_LEN - output_len, stdout);
					break;
			}
		}
		/* Ensure that the conversion ends in the 'initial state', because after this we will
		   be outputing escape sequences. */
		conversion_output_ptr = conversion_output;
		output_len = CONV_BUFFER_LEN;
		unicode_export(output_iconv, NULL, NULL, &conversion_output_ptr, &output_len);
		if (output_len < CONV_BUFFER_LEN)
			fwrite(conversion_output, 1, CONV_BUFFER_LEN - output_len, stdout);
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

	if (output_iconv == (t3_export_t) -1) {
		//FIXME: make this dependent on the detected terminal capabilities
		for (idx = 0; idx < nfc_output_len; idx += codepoint_len) {
			codepoint_len = nfc_output_len - idx;
			c = t3_getuc(nfc_output + idx, &codepoint_len);
			if (t3_get_codepoint_info(c) & T3_COMBINING_BIT)
				return t3_false;
		}
		return t3_true;
	} else {
		char conversion_output[CONV_BUFFER_LEN], *conversion_output_ptr = conversion_output,
			*conversion_input_ptr = nfc_output;
		size_t input_len = nfc_output_len, output_len = CONV_BUFFER_LEN;
		int retval;

		/* NOTE: although the iconv manual page and glibc iconv description mention that
		   outbuf may be a NULL pointer, in fact passing one results in a segmentation fault.
		   Therefore we just convert to a temporary buffer which is then discarded. */

		/* Convert UTF-8 sequence into current output encoding using iconv. */
		while (input_len > 0) {
			retval = unicode_export(output_iconv, &conversion_input_ptr, &input_len, &conversion_output_ptr, &output_len);
			switch (retval) {
				case EILSEQ:
				case EINVAL:
					/* Reset conversion to initial state. */
					conversion_output_ptr = conversion_output;
					output_len = CONV_BUFFER_LEN;
					unicode_export(output_iconv, NULL, NULL, &conversion_output_ptr, &output_len);
					return t3_false;
				case E2BIG:
					/* Not enough space in output buffer. Restart conversion with remaining chars. */
					conversion_output_ptr = conversion_output;
					output_len = CONV_BUFFER_LEN;
					break;
				case ENOTSUP:
					return t3_false;
				default:
					break;
			}
		}
		/* Reset conversion to initial state. */
		conversion_output_ptr = conversion_output;
		output_len = CONV_BUFFER_LEN;
		unicode_export(output_iconv, NULL, NULL, &conversion_output_ptr, &output_len);
		return t3_true;
	}
}

/** Set the replacement character used for undrawable characters.
    @ingroup t3window_term
    @param c The character to draw when an undrawable characters is encountered.

    The default character is the question mark ('?'). The character must be an
    ASCI character. For terminals capable of Unicode output the Replacement
    Character is used (codepoint FFFD).
*/
void t3_term_set_replacement_char(char c) {
	replacement_char = c;
}

