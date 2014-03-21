/* Copyright (C) 2011-2014 G.P. Halkes
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
#include <uniwidth.h>
#include "utf8.h"

/** Get the first codepoint represented by a UTF-8 string.
    @param src The UTF-8 string to parse.
	@param size The location to store the number of bytes in the first
	    codepoint, which should contain the number of bytes in src on entry
		(may be @c NULL).
    @return The codepoint at the start of @p src or @c FFFD if an invalid
        codepoint is encountered.
*/
uint32_t t3_utf8_get(const char *src, size_t *size) {
	size_t max_size, _size;
	int bytes_left;
	uint32_t retval, least;

	/* Just assume the buffer is large enough if size is not passed. */
	max_size = size == NULL ? 4 : *size;
	_size = 1;

	switch ((uint8_t) src[0]) {
		case  0: case  1: case  2: case  3: case  4: case  5: case  6: case  7:
		case  8: case  9: case 10: case 11: case 12: case 13: case 14: case 15:
		case 16: case 17: case 18: case 19: case 20: case 21: case 22: case 23:
		case 24: case 25: case 26: case 27: case 28: case 29: case 30: case 31:
		case 32: case 33: case 34: case 35: case 36: case 37: case 38: case 39:
		case 40: case 41: case 42: case 43: case 44: case 45: case 46: case 47:
		case 48: case 49: case 50: case 51: case 52: case 53: case 54: case 55:
		case 56: case 57: case 58: case 59: case 60: case 61: case 62: case 63:
		case 64: case 65: case 66: case 67: case 68: case 69: case 70: case 71:
		case 72: case 73: case 74: case 75: case 76: case 77: case 78: case 79:
		case 80: case 81: case 82: case 83: case 84: case 85: case 86: case 87:
		case 88: case 89: case 90: case 91: case 92: case 93: case 94: case 95:
		case  96: case  97: case  98: case  99: case 100: case 101: case 102: case 103:
		case 104: case 105: case 106: case 107: case 108: case 109: case 110: case 111:
		case 112: case 113: case 114: case 115: case 116: case 117: case 118: case 119:
		case 120: case 121: case 122: case 123: case 124: case 125: case 126: case 127:
			if (size != NULL)
				*size = _size;
			return src[0];
		case 128: case 129: case 130: case 131: case 132: case 133: case 134: case 135:
		case 136: case 137: case 138: case 139: case 140: case 141: case 142: case 143:
		case 144: case 145: case 146: case 147: case 148: case 149: case 150: case 151:
		case 152: case 153: case 154: case 155: case 156: case 157: case 158: case 159:
		case 160: case 161: case 162: case 163: case 164: case 165: case 166: case 167:
		case 168: case 169: case 170: case 171: case 172: case 173: case 174: case 175:
		case 176: case 177: case 178: case 179: case 180: case 181: case 182: case 183:
		case 184: case 185: case 186: case 187: case 188: case 189: case 190: case 191:
		case 192: case 193:
			if (size != NULL)
				*size = _size;
			return 0xFFFD;
		case 194: case 195: case 196: case 197: case 198: case 199: case 200: case 201:
		case 202: case 203: case 204: case 205: case 206: case 207: case 208: case 209:
		case 210: case 211: case 212: case 213: case 214: case 215: case 216: case 217:
		case 218: case 219: case 220: case 221: case 222: case 223:
			least = 0x80;
			bytes_left = 1;
			retval = src[0] & 0x1F;
			break;
		case 224: case 225: case 226: case 227: case 228: case 229: case 230: case 231:
		case 232: case 233: case 234: case 235: case 236: case 237: case 238: case 239:
			least = 0x800;
			bytes_left = 2;
			retval = src[0] & 0x0F;
			break;
		case 240: case 241: case 242: case 243: case 244:
			least = 0x10000L;
			bytes_left = 3;
			retval = src[0] & 0x07;
			break;
		case 245: case 246: case 247: case 248: case 249: case 250: case 251: case 252:
		case 253: case 254: case 255:
			if (size != NULL)
				*size = _size;
			return 0xFFFD;
		default:
			if (size != NULL)
				*size = _size;
			return 0xFFFD;
	}

	src++;
	for (; bytes_left > 0 && _size < max_size; bytes_left--, _size++) {
		if ((src[0] & 0xC0) != 0x80) {
			if (size != NULL)
				*size = _size;
			return 0xFFFD;
		}
		retval = (retval << 6) | (src[0] & 0x3F);
		src++;
	}
	if (size != NULL)
		*size = _size;

	if (retval < least)
		return 0xFFFD;
	if (retval > 0x10FFFFL)
		return 0xFFFD;
	if (bytes_left > 0)
		return 0xFFFD;
	return retval;
}

/** Convert a codepoint to a UTF-8 string.
    @param c The codepoint to convert.
    @param dst The location to store the result.
    @return The number of bytes stored in @p dst.

    If an invalid codepoint is passed in @p c, the replacement character
    (@c FFFD) is stored instead
*/
size_t t3_utf8_put(uint32_t c, char *dst) {
	if (c < 0x80) {
		dst[0] = c;
		return 1;
	} else if (c < 0x800) {
		dst[0] = 0xC0 | (c >> 6);
		dst[1] = 0x80 | (c & 0x3F);
		return 2;
	} else if (c < 0x10000) {
		dst[0] = 0xe0 | (c >> 12);
		dst[1] = 0x80 | ((c >> 6) & 0x3f);
		dst[2] = 0x80 | (c & 0x3F);
		return 3;
	} else if (c < 0x110000) {
		dst[0] = 0xf0 | (c >> 18);
		dst[1] = 0x80 | ((c >> 12) & 0x3f);
		dst[2] = 0x80 | ((c >> 6) & 0x3f);
		dst[3] = 0x80 | (c & 0x3F);
		return 4;
	} else {
		/* Store the replacement character. */
		dst[0] = 0xEF;
		dst[1] = 0xBF;
		dst[2] = 0xBD;
		return 3;
	}
}

/** Get the width of a Unicode codepoint.

    This function is a wrapper around uc_width, which takes into account that
    for some characters uc_width returns a value that is different from what
    terminals actually use.
*/
int t3_utf8_wcwidth(uint32_t c) {
	static const char nul;
	if (c >= 0x1160 && c < 0x11fa)
		return 0;
	else if (c == 0x00ad)
		return 1;
	else if (c == 0)
		return -1;
	return uc_width(c, &nul);
}
