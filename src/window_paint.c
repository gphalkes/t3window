/* Copyright (C) 2011-2012 G.P. Halkes
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
#include <string.h>
#include <unictype.h>

#include "internal.h"
#include "log.h"
#include "utf8.h"
#include "window.h"

/** @internal
    @brief The maximum size of a UTF-8 character in bytes. Used in ::t3_win_addnstr.
*/
#define UTF8_MAX_BYTES 4

/* Attribute to index mapping. To make the mapping quick, a simple hash table
   with hash chaining is used.
*/

typedef struct attr_map_t attr_map_t;
struct attr_map_t {
  t3_attr_t attr;
  int next;
};

/** @internal
    @brief The initial allocation for ::attr_map.
*/
#define ATTR_MAP_START_SIZE 32
/** @internal
    @brief The size of the hash map used for ::t3_attr_t mapping.
*/
#define ATTR_HASH_MAP_SIZE 337

static attr_map_t *attr_map; /**< @internal @brief The map of indices to attribute sets. */
static int attr_map_fill,    /**< @internal @brief The number of elements used in ::attr_map. */
    attr_map_allocated; /**< @internal @brief The number of elements allocated in ::attr_map. */
static int attr_hash_map[ATTR_HASH_MAP_SIZE]; /**< @internal @brief Hash map for quickly mapping
                                                 ::t3_attr_t's to indices. */

/** @addtogroup t3window_win */
/** @{ */

/** Ensure that a line_data_t struct has at least a specified number of
        bytes of unused space.
    @param line The line_data_t struct to check.
    @param n The required unused space in bytes.
    @return A boolean indicating whether, after possibly reallocating, the
        requested number of bytes is available.
*/
static t3_bool ensure_space(line_data_t *line, size_t n) {
  int newsize;
  char *resized;

  if (n > INT_MAX || INT_MAX - (int)n < line->length) return t3_false;

  if (line->allocated > line->length + (int)n) return t3_true;

  newsize = line->allocated;

  do {
    /* Sanity check for overflow of allocated variable. Prevents infinite loops. */
    if (newsize > INT_MAX / 2)
      newsize = INT_MAX;
    else
      newsize *= 2;
  } while (newsize - line->length < (int)n);

  if ((resized = realloc(line->data, sizeof(t3_attr_t) * newsize)) == NULL) return t3_false;
  line->data = resized;
  line->allocated = newsize;
  return t3_true;
}

/** @internal
    @brief Map a set of attributes to an integer.
    @param attr The attribute set to map.
*/
int _t3_map_attr(t3_attr_t attr) {
  int ptr;

  for (ptr = attr_hash_map[attr % ATTR_HASH_MAP_SIZE]; ptr != -1 && attr_map[ptr].attr != attr;
       ptr = attr_map[ptr].next) {
  }

  if (ptr != -1) return ptr;

  if (attr_map_fill >= attr_map_allocated) {
    int new_allocation = attr_map_allocated == 0 ? ATTR_MAP_START_SIZE : attr_map_allocated * 2;
    attr_map_t *new_map;

    if ((new_map = realloc(attr_map, new_allocation * sizeof(attr_map_t))) == NULL) return -1;
    attr_map = new_map;
    attr_map_allocated = new_allocation;
  }
  attr_map[attr_map_fill].attr = attr;
  attr_map[attr_map_fill].next = attr_hash_map[attr % ATTR_HASH_MAP_SIZE];
  attr_hash_map[attr % ATTR_HASH_MAP_SIZE] = attr_map_fill;

  return attr_map_fill++;
}

/** @internal
    @brief Get the set of attributes associated with a mapped integer.
    @param idx The mapped attribute index as returned by ::_t3_map_attr.
*/
t3_attr_t _t3_get_attr(int idx) {
  if (idx < 0 || idx > attr_map_fill) return 0;
  return attr_map[idx].attr;
}

/** @internal
    @brief Initialize data structures used for attribute set mappings.
*/
void _t3_init_attr_map(void) {
  int i;
  for (i = 0; i < ATTR_HASH_MAP_SIZE; i++) attr_hash_map[i] = -1;
}

/** @internal
    @brief Clean up the memory used for attribute set mappings.
*/
void _t3_free_attr_map(void) {
#ifdef _T3_WINDOW_DEBUG
  {
    int ptr, chain, avg = 0, max = 0, chains = 0;
    int chain_length = 0;
    for (chain = 0; chain < ATTR_HASH_MAP_SIZE; chain++) {
      for (ptr = attr_hash_map[chain], chain_length = 0; ptr != -1;
           ptr = attr_map[ptr].next, chain_length++) {
      }
      if (chain_length > max) max = chain_length;
      if (chain_length > 0) {
        chains++;
        avg += chain_length;
      }
    }
    lprintf("max: %d, chains: %d, avg: %.2f, attrs: %d\n", max, chains, (double)avg / chains,
            attr_map_fill);
  }
#endif
  free(attr_map);
  attr_map = NULL;
  attr_map_allocated = 0;
  attr_map_fill = 0;
  _t3_init_attr_map();
}

/** Get the first UTF-8 value encoded in a string.
    @param src The UTF-8 string to parse.
    @param size The location to store the size of the character.
    @return The value at the start of @p src.

    @note This function assumes that the input is a valid UTF-8 encoded value.
*/
uint32_t _t3_get_value_int(const char *src, size_t *size) {
  int bytes_left;
  uint32_t retval;

  switch ((uint8_t)src[0]) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
    case 17:
    case 18:
    case 19:
    case 20:
    case 21:
    case 22:
    case 23:
    case 24:
    case 25:
    case 26:
    case 27:
    case 28:
    case 29:
    case 30:
    case 31:
    case 32:
    case 33:
    case 34:
    case 35:
    case 36:
    case 37:
    case 38:
    case 39:
    case 40:
    case 41:
    case 42:
    case 43:
    case 44:
    case 45:
    case 46:
    case 47:
    case 48:
    case 49:
    case 50:
    case 51:
    case 52:
    case 53:
    case 54:
    case 55:
    case 56:
    case 57:
    case 58:
    case 59:
    case 60:
    case 61:
    case 62:
    case 63:
    case 64:
    case 65:
    case 66:
    case 67:
    case 68:
    case 69:
    case 70:
    case 71:
    case 72:
    case 73:
    case 74:
    case 75:
    case 76:
    case 77:
    case 78:
    case 79:
    case 80:
    case 81:
    case 82:
    case 83:
    case 84:
    case 85:
    case 86:
    case 87:
    case 88:
    case 89:
    case 90:
    case 91:
    case 92:
    case 93:
    case 94:
    case 95:
    case 96:
    case 97:
    case 98:
    case 99:
    case 100:
    case 101:
    case 102:
    case 103:
    case 104:
    case 105:
    case 106:
    case 107:
    case 108:
    case 109:
    case 110:
    case 111:
    case 112:
    case 113:
    case 114:
    case 115:
    case 116:
    case 117:
    case 118:
    case 119:
    case 120:
    case 121:
    case 122:
    case 123:
    case 124:
    case 125:
    case 126:
    case 127:
      *size = 1;
      return src[0];
    case 194:
    case 195:
    case 196:
    case 197:
    case 198:
    case 199:
    case 200:
    case 201:
    case 202:
    case 203:
    case 204:
    case 205:
    case 206:
    case 207:
    case 208:
    case 209:
    case 210:
    case 211:
    case 212:
    case 213:
    case 214:
    case 215:
    case 216:
    case 217:
    case 218:
    case 219:
    case 220:
    case 221:
    case 222:
    case 223:
      bytes_left = 1;
      retval = src[0] & 0x1F;
      break;
    case 224:
    case 225:
    case 226:
    case 227:
    case 228:
    case 229:
    case 230:
    case 231:
    case 232:
    case 233:
    case 234:
    case 235:
    case 236:
    case 237:
    case 238:
    case 239:
      bytes_left = 2;
      retval = src[0] & 0x0F;
      break;
    case 240:
    case 241:
    case 242:
    case 243:
    case 244:
    case 245:
    case 246:
    case 247:
      bytes_left = 3;
      retval = src[0] & 0x07;
      break;
    case 248:
    case 249:
    case 250:
    case 251:
      bytes_left = 4;
      retval = src[0] & 0x03;
      break;
    case 252:
    case 253:
      bytes_left = 5;
      retval = src[1] & 1;
      break;
    default:
      /* This should never occur, as we only use this to read values we generated
         ourselves. However, the compiler will start fussing if we don't add this. */
      *size = 0;
      return 0;
  }

  *size = bytes_left + 1;
  src++;
  for (; bytes_left > 0; bytes_left--) retval = (retval << 6) | (src++ [0] & 0x3f);
  return retval;
}

/** Write a UTF-8 encoded value.
    @param c The codepoint to convert.
    @param dst The location to store the result.
    @return The number of bytes stored in @p dst.

    The value must be lower than 0x80000000 (i.e. at most 31 bits may be used).
    No check is made for this however, so the calling code must ensure that this
    is the case.
*/
size_t _t3_put_value(uint32_t c, char *dst) {
  if (c < 0x80) {
    dst[0] = c;
    return 1;
  } else if (c < 0x800) {
    dst[0] = 0xc0 | (c >> 6);
    dst[1] = 0x80 | (c & 0x3f);
    return 2;
  } else if (c < 0x10000) {
    dst[0] = 0xe0 | (c >> 12);
    dst[1] = 0x80 | ((c >> 6) & 0x3f);
    dst[2] = 0x80 | (c & 0x3f);
    return 3;
  } else if (c < 0x200000) {
    dst[0] = 0xf0 | (c >> 18);
    dst[1] = 0x80 | ((c >> 12) & 0x3f);
    dst[2] = 0x80 | ((c >> 6) & 0x3f);
    dst[3] = 0x80 | (c & 0x3f);
    return 4;
  } else if (c < 0x4000000) {
    dst[0] = 0xf8 | (c >> 24);
    dst[1] = 0x80 | ((c >> 18) & 0x3f);
    dst[1] = 0x80 | ((c >> 12) & 0x3f);
    dst[2] = 0x80 | ((c >> 6) & 0x3f);
    dst[3] = 0x80 | (c & 0x3f);
    return 5;
  } else {
    dst[0] = 0xfc | (c >> 30);
    dst[1] = 0x80 | ((c >> 24) & 0x3f);
    dst[1] = 0x80 | ((c >> 18) & 0x3f);
    dst[1] = 0x80 | ((c >> 12) & 0x3f);
    dst[2] = 0x80 | ((c >> 6) & 0x3f);
    dst[3] = 0x80 | (c & 0x3f);
    return 6;
  }
}

/** Create memory block representing a space character with specific attributes.
    @param attr The attribute index to use.
    @param out An array of size at least 8 to write to.
    @return The number of bytes written to @p out.
*/
static size_t create_space_block(int attr, char *out) {
  size_t result_size;
  result_size = _t3_put_value(attr, out + 1);
  result_size++;
  out[result_size] = ' ';
  out[0] = result_size << 1;
  result_size++;
  return result_size;
}

/** Get the attribute index from a block. */
static uint32_t get_block_attr(const char *block) {
  size_t discard;
  for (block++; ((*block) & 0xc0) == 0x80; block++) {
  }

  return _t3_get_value(block, &discard);
}

/** Insert a zero-width character into an existing block.
    @param win The window to write to.
    @param str The string containing the UTF-8 encoded zero-width character.
    @param n The number of bytes in @p str.
    @return A boolean indicating success.
*/
static t3_bool _win_add_zerowidth(t3_window_t *win, const char *str, size_t n) {
  uint32_t block_size, new_block_size;
  size_t block_size_bytes, new_block_size_bytes;
  char new_block_size_str[6];
  int pos_width, i;

  if (win->lines == NULL) return t3_false;

  if (win->paint_y >= win->height) return t3_true;
  /* Combining characters may be added _at_ width. */
  if (win->paint_x > win->width) return t3_true;

  if (win->cached_pos_line != win->paint_y || win->cached_pos_width >= win->paint_x) {
    win->cached_pos_line = win->paint_y;
    win->cached_pos = 0;
    win->cached_pos_width = win->lines[win->paint_y].start;
  }

  /* Simply drop characters that don't belong to any other character. */
  if (win->lines[win->paint_y].length == 0 || win->paint_x <= win->lines[win->paint_y].start ||
      win->paint_x > win->lines[win->paint_y].start + win->lines[win->paint_y].width)
    return t3_true;

  /* Ensure we have space for n characters, and possibly extend the block size header by 1. */
  if (!ensure_space(win->lines + win->paint_y, n + 1)) return t3_false;

  pos_width = win->cached_pos_width;

  /* Locate the first character that at least partially overlaps the position
     where this string is supposed to go. Note that this loop will iterate at
     least once, because if win->cached_pos == win->lines[win->paint_y].length,
     then win->cached_pos_width will equal win->paint_x, and thereby get invalidated
     above. */
  block_size = 0; /* Shut up the compiler. */
  for (i = win->cached_pos; i < win->lines[win->paint_y].length;
       i += (block_size >> 1) + block_size_bytes) {
    block_size = _t3_get_value(win->lines[win->paint_y].data + i, &block_size_bytes);
    pos_width += _T3_BLOCK_SIZE_TO_WIDTH(block_size);

    /* Do the check for whether we found the insertion point here, so we don't update i. */
    if (pos_width >= win->paint_x) break;
  }

  /* Check whether we are being asked to add a zero-width character in the middle
     of a double-width character. If so, ignore. */
  if (pos_width > win->paint_x) return t3_true;

  new_block_size = block_size + (n << 1);
  new_block_size_bytes = _t3_put_value(new_block_size, new_block_size_str);

  /* WARNING: from this point on, the block_size and new_block_size variables have
     a new meaning: the actual size of the block, rather than including the bit
     indicating the width of the character as well. */
  block_size >>= 1;
  new_block_size >>= 1;

  /* Move the data after the insertion point up by the size of the character
     string to insert and the difference in block size header size. */
  memmove(win->lines[win->paint_y].data + i + new_block_size + new_block_size_bytes,
          win->lines[win->paint_y].data + i + block_size + block_size_bytes,
          win->lines[win->paint_y].length - i - block_size - block_size_bytes);
  /* Copy the bytes of the new character into the string. */
  memcpy(win->lines[win->paint_y].data + i + block_size + block_size_bytes, str, n);
  /* If applicable, move the data for this block by the difference in block size header size. */
  if (new_block_size_bytes != block_size_bytes) {
    memmove(win->lines[win->paint_y].data + i + new_block_size_bytes,
            win->lines[win->paint_y].data + i + block_size_bytes, new_block_size);
  }
  /* Copy in the new block size header. */
  memcpy(win->lines[win->paint_y].data + i, new_block_size_str, new_block_size_bytes);

  win->lines[win->paint_y].length += n + (new_block_size_bytes - block_size_bytes);
  return t3_true;
}

/** Write one or more blocks to a window.
    @param win The window to write to.
    @param blocks The string containing the blocks.
    @param n The number of bytes in @p blocks.
    @return A boolean indicating success.
*/
static t3_bool _win_write_blocks(t3_window_t *win, const char *blocks, size_t n) {
  uint32_t block_size;
  size_t block_size_bytes;
  size_t k;
  int i;
  int width = 0;
  int extra_spaces = 0;
  uint32_t extra_spaces_attr;
  t3_bool result = t3_true;

  if (win->lines == NULL) return t3_false;

  if (win->paint_y >= win->height || win->paint_x >= win->width || n == 0) return t3_true;

  for (k = 0; k < n; k += (block_size >> 1) + block_size_bytes) {
    block_size = _t3_get_value(blocks + k, &block_size_bytes);
    if (win->paint_x + width + _T3_BLOCK_SIZE_TO_WIDTH(block_size) > win->width) break;
    width += _T3_BLOCK_SIZE_TO_WIDTH(block_size);
  }

  if (k < n) {
    extra_spaces = win->width - win->paint_x - width;
    extra_spaces_attr = get_block_attr(blocks + k);
  }
  n = k;

  if (win->cached_pos_line != win->paint_y || win->cached_pos_width > win->paint_x) {
    win->cached_pos_line = win->paint_y;
    win->cached_pos = 0;
    win->cached_pos_width = win->lines[win->paint_y].start;
  }

  if (win->lines[win->paint_y].length == 0) {
    /* Empty line. */
    if (!ensure_space(win->lines + win->paint_y, n)) return t3_false;
    win->lines[win->paint_y].start = win->paint_x;
    memcpy(win->lines[win->paint_y].data, blocks, n);
    win->lines[win->paint_y].length += n;
    win->lines[win->paint_y].width = width;
    win->cached_pos_line = -1;
  } else if (win->lines[win->paint_y].start + win->lines[win->paint_y].width <= win->paint_x) {
    /* Add characters after existing characters. */
    char default_attr_str[8];
    size_t default_attr_size;
    int diff = win->paint_x - (win->lines[win->paint_y].start + win->lines[win->paint_y].width);

    default_attr_size = create_space_block(_t3_map_attr(win->default_attrs), default_attr_str);

    if (!ensure_space(win->lines + win->paint_y, n + diff * (default_attr_size))) return t3_false;

    for (i = diff; i > 0; i--) {
      memcpy(win->lines[win->paint_y].data + win->lines[win->paint_y].length, default_attr_str,
             default_attr_size);
      win->lines[win->paint_y].length += default_attr_size;
    }

    memcpy(win->lines[win->paint_y].data + win->lines[win->paint_y].length, blocks, n);
    win->lines[win->paint_y].length += n;
    win->lines[win->paint_y].width += width + diff;
  } else if (win->paint_x + width <= win->lines[win->paint_y].start) {
    /* Add characters before existing characters. */
    char default_attr_str[8];
    size_t default_attr_size;
    int diff = win->lines[win->paint_y].start - (win->paint_x + width);

    default_attr_size = create_space_block(_t3_map_attr(win->default_attrs), default_attr_str);

    if (!ensure_space(win->lines + win->paint_y, n + diff * default_attr_size)) return t3_false;
    memmove(win->lines[win->paint_y].data + n + diff * default_attr_size,
            win->lines[win->paint_y].data, win->lines[win->paint_y].length);
    memcpy(win->lines[win->paint_y].data, blocks, n);
    for (i = diff; i > 0; i--) {
      memcpy(win->lines[win->paint_y].data + n, default_attr_str, default_attr_size);
      n += default_attr_size;
    }
    win->lines[win->paint_y].length += n;
    win->lines[win->paint_y].width += width + diff;
    win->lines[win->paint_y].start = win->paint_x;
    /* Inserting before existing characters invalidates the cached position. */
    win->cached_pos_line = -1;
  } else {
    /* Character (partly) overwrite existing chars. */
    int pos_width = win->cached_pos_width;
    size_t start_replace = 0, start_space_attr, start_spaces, end_replace, end_space_attr,
           end_spaces;
    int sdiff;
    char start_space_str[8], end_space_str[8];
    size_t start_space_bytes, end_space_bytes;

    /* Locate the first character that at least partially overlaps the position
       where this string is supposed to go. Note that this loop will always be
       entered once, because win->cached_pos will always be < line length.
       To see why this is the case, consider that when win->cached_pos equals the
       line length. Then win->paint_x equals the width of the line and that case
       will be handled above. */
    block_size = 0; /* Shut up the compiler. */
    for (i = win->cached_pos; i < win->lines[win->paint_y].length;
         i += (block_size >> 1) + block_size_bytes) {
      block_size = _t3_get_value(win->lines[win->paint_y].data + i, &block_size_bytes);
      if (_T3_BLOCK_SIZE_TO_WIDTH(block_size) + pos_width > win->paint_x) break;

      pos_width += _T3_BLOCK_SIZE_TO_WIDTH(block_size);
    }

    win->cached_pos = i;
    win->cached_pos_width = pos_width;

    start_replace = i;

    /* If the character only partially overlaps, we replace the first part with
       spaces with the attributes of the old character. */
    start_space_attr = get_block_attr(win->lines[win->paint_y].data + start_replace);
    start_spaces = win->paint_x >= win->lines[win->paint_y].start ? win->paint_x - pos_width : 0;

    /* Now we need to find which other character(s) overlap. However, the current
       string may overlap with a double width character but only for a single
       position. In that case we will replace the trailing portion of the character
       with spaces with the old character's attributes. */
    pos_width += _T3_BLOCK_SIZE_TO_WIDTH(block_size);

    i += (block_size >> 1) + block_size_bytes;

    /* If the character where we start overwriting already fully overlaps with the
       new string, then we need to only replace this and any spaces that result
       from replacing the trailing portion need to use the start space attribute */
    if (pos_width >= win->paint_x + width) {
      end_space_attr = start_space_attr;
      end_replace = i;
    } else {
      for (; i < win->lines[win->paint_y].length; i += (block_size >> 1) + block_size_bytes) {
        block_size = _t3_get_value(win->lines[win->paint_y].data + i, &block_size_bytes);
        pos_width += _T3_BLOCK_SIZE_TO_WIDTH(block_size);
        if (pos_width >= win->paint_x + width) break;
      }

      end_space_attr = get_block_attr(win->lines[win->paint_y].data + i);
      end_replace =
          i < win->lines[win->paint_y].length ? (int)(i + (block_size >> 1) + block_size_bytes) : i;
    }

    end_spaces = pos_width > win->paint_x + width ? pos_width - win->paint_x - width : 0;

    start_space_bytes = create_space_block(start_space_attr, start_space_str);
    end_space_bytes = create_space_block(end_space_attr, end_space_str);

    /* Move the existing characters out of the way. */
    sdiff = n + end_spaces * end_space_bytes + start_spaces * start_space_bytes -
            (end_replace - start_replace);
    if (sdiff > 0 && !ensure_space(win->lines + win->paint_y, sdiff)) return t3_false;

    memmove(win->lines[win->paint_y].data + end_replace + sdiff,
            win->lines[win->paint_y].data + end_replace,
            win->lines[win->paint_y].length - end_replace);

    for (i = start_replace; start_spaces > 0; start_spaces--) {
      memcpy(win->lines[win->paint_y].data + i, start_space_str, start_space_bytes);
      i += start_space_bytes;
    }

    memcpy(win->lines[win->paint_y].data + i, blocks, n);
    i += n;
    for (; end_spaces > 0; end_spaces--) {
      memcpy(win->lines[win->paint_y].data + i, end_space_str, end_space_bytes);
      i += end_space_bytes;
    }

    win->lines[win->paint_y].length += sdiff;
    if (win->lines[win->paint_y].start + win->lines[win->paint_y].width < width + win->paint_x)
      win->lines[win->paint_y].width = width + win->paint_x - win->lines[win->paint_y].start;
    if (win->lines[win->paint_y].start > win->paint_x) {
      win->lines[win->paint_y].width += win->lines[win->paint_y].start - win->paint_x;
      win->lines[win->paint_y].start = win->paint_x;
      win->cached_pos_line = -1;
    }
  }
  win->paint_x += width;

  if (extra_spaces > 0) {
    char extra_space_str[8];
    size_t extra_space_bytes;

    extra_space_bytes = create_space_block(extra_spaces_attr, extra_space_str);

    for (i = 0; i < extra_spaces; i++)
      result &= _win_write_blocks(win, extra_space_str, extra_space_bytes);
  }

  return result;
}

/** Add a string with explicitly specified size to a t3_window_t with specified attributes.
    @param win The t3_window_t to add the string to.
    @param str The string to add.
    @param n The size of @p str.
    @param attr The attributes to use.
    @retval ::T3_ERR_SUCCESS on succes
    @retval ::T3_ERR_NONPRINT if a control character was encountered.
    @retval ::T3_ERR_ERRNO otherwise.

    The default attributes are combined with the specified attributes, with
    @p attr used as the priority attributes. All other t3_win_add* functions are
    (indirectly) implemented using this function.

    It is important that combining characters are provided in the same string as the
    characters they are to combine with. In particular, this function does not check for
    conjoining Jamo in the existing window data and explicitly prevents joining.
*/
int t3_win_addnstr(t3_window_t *win, const char *str, size_t n, t3_attr_t attrs) {
  size_t bytes_read;
  char block[1 + 6 + UTF8_MAX_BYTES];
  uint32_t c;
  int retval = T3_ERR_SUCCESS;
  int width;
  int attrs_idx;
  size_t block_bytes;

  attrs = _t3_term_sanitize_attrs(attrs);

  attrs = t3_term_combine_attrs(attrs, win->default_attrs);
  attrs_idx = _t3_map_attr(attrs);
  if (attrs_idx < 0) return T3_ERR_OUT_OF_MEMORY;

  int width_state = 0;
  for (; n > 0; n -= bytes_read, str += bytes_read) {
    bytes_read = n;
    c = t3_utf8_get(str, &bytes_read);

    int old_width_state = width_state;
    width = t3_utf8_wcwidth_ext(c, &width_state);
    if (old_width_state != 0 && width_state == 0) {
      /* Ending a block with a conjoining Jamo character can cause problems when the
         succeeding cell later is overwritten with a joining character. To prevent this
         issue, insert a zero-with non-joiner. */
      if (width_state != 0) {
        _win_add_zerowidth(win, "\xE2\x80\x8C", 3);
      }
    }
    /* UC_CATEGORY_MASK_Cn is for unassigned/reserved code points. These are
       not necessarily unprintable. */
    if (width < 0 || uc_is_general_category_withtable(c, T3_UTF8_CONTROL_MASK)) {
      retval = T3_ERR_NONPRINT;
      continue;
    } else if (width == 0) {
      _win_add_zerowidth(win, str, bytes_read);
      continue;
    }

    block_bytes = _t3_put_value(attrs_idx, block + 1);
    memcpy(block + 1 + block_bytes, str, bytes_read);
    block_bytes += bytes_read;
    _t3_put_value((block_bytes << 1) + (width == 2 ? 1 : 0), block);
    block_bytes++;

    if (!_win_write_blocks(win, block, block_bytes)) return T3_ERR_ERRNO;
  }
  /* Ending a block with a conjoining Jamo character can cause problems when the
     succeeding cell later is overwritten with a joining character. To prevent this
     issue, insert a zero-with non-joiner. */
  if (width_state != 0) {
    _win_add_zerowidth(win, "\xE2\x80\x8C", 3);
  }
  return retval;
}

/** Add a nul-terminated string to a t3_window_t with specified attributes.
    @param win The t3_window_t to add the string to.
    @param str The nul-terminated string to add.
    @param attr The attributes to use.
    @return See ::t3_win_addnstr.

        See ::t3_win_addnstr for further information.
*/
int t3_win_addstr(t3_window_t *win, const char *str, t3_attr_t attr) {
  return t3_win_addnstr(win, str, strlen(str), attr);
}

/** Add a single character to a t3_window_t with specified attributes.
    @param win The t3_window_t to add the string to.
    @param c The character to add.
    @param attr The attributes to use.
    @return See ::t3_win_addnstr.

        @p c must be an ASCII character. See ::t3_win_addnstr for further information.
*/
int t3_win_addch(t3_window_t *win, char c, t3_attr_t attr) {
  return t3_win_addnstr(win, &c, 1, attr);
}

/** Add a string with explicitly specified size to a t3_window_t with specified attributes and
   repetition.
    @param win The t3_window_t to add the string to.
    @param str The string to add.
    @param n The size of @p str.
    @param attr The attributes to use.
    @param rep The number of times to repeat @p str.
    @return See ::t3_win_addnstr.

        All other t3_win_add*rep functions are (indirectly) implemented using this
    function. See ::t3_win_addnstr for further information.
*/
int t3_win_addnstrrep(t3_window_t *win, const char *str, size_t n, t3_attr_t attr, int rep) {
  int i, ret;

  for (i = 0; i < rep; i++) {
    ret = t3_win_addnstr(win, str, n, attr);
    if (ret != 0) return ret;
  }
  return 0;
}

/** Add a nul-terminated string to a t3_window_t with specified attributes and repetition.
    @param win The t3_window_t to add the string to.
    @param str The nul-terminated string to add.
    @param attr The attributes to use.
    @param rep The number of times to repeat @p str.
    @return See ::t3_win_addnstr.

    See ::t3_win_addnstrrep for further information.
*/
int t3_win_addstrrep(t3_window_t *win, const char *str, t3_attr_t attr, int rep) {
  return t3_win_addnstrrep(win, str, strlen(str), attr, rep);
}

/** Add a character to a t3_window_t with specified attributes and repetition.
    @param win The t3_window_t to add the string to.
    @param c The character to add.
    @param attr The attributes to use.
    @param rep The number of times to repeat @p c.
    @return See ::t3_win_addnstr.

    See ::t3_win_addnstrrep for further information.
*/
int t3_win_addchrep(t3_window_t *win, char c, t3_attr_t attr, int rep) {
  return t3_win_addnstrrep(win, &c, 1, attr, rep);
}

/** Get the next t3_window_t, when iterating over the t3_window_t's for drawing.
    @param ptr The last t3_window_t that was handled.
*/
static t3_window_t *get_previous_window(t3_window_t *ptr) {
  if (ptr->shown && ptr->tail != NULL) {
    ptr = ptr->tail;
    if (ptr->shown) return ptr;
  }

  do {
    while (ptr->prev != NULL) {
      ptr = ptr->prev;
      if (ptr->shown) return ptr;
    }
    ptr = ptr->parent;
  } while (ptr != NULL);
  return NULL;
}

static t3_bool write_spaces_to_terminal_window(int attr_idx, int count) {
  char space_str[64];
  size_t space_str_bytes;
  int i;
  t3_bool result = true;

  space_str_bytes = create_space_block(attr_idx, space_str);
  if (count > 1) {
    memcpy(space_str + space_str_bytes, space_str, space_str_bytes);
    if (count > 2) {
      memcpy(space_str + space_str_bytes * 2, space_str, space_str_bytes * 2);
      if (count > 4) memcpy(space_str + space_str_bytes * 4, space_str, space_str_bytes * 4);
    }
  }

  for (i = count / 8; i > 0; i--)
    result &= _win_write_blocks(_t3_terminal_window, space_str, space_str_bytes * 8);
  result &= _win_write_blocks(_t3_terminal_window, space_str, space_str_bytes * (count & 7));
  return result;
}

/** @internal
    @brief Redraw a terminal line, based on all visible t3_window_t structs.
    @param terminal The t3_window_t representing the cached terminal contents.
    @param line The line to redraw.
    @return A boolean indicating whether redrawing succeeded without memory errors.
*/
t3_bool _t3_win_refresh_term_line(int line) {
  line_data_t *draw;
  t3_window_t *ptr;
  int y, x, parent_y, parent_x, parent_max_y, parent_max_x;
  int data_start, length, paint_x;
  t3_bool result = t3_true;
  uint32_t block_size;
  size_t block_size_bytes;

  _t3_terminal_window->paint_y = line;
  _t3_terminal_window->lines[line].width = 0;
  _t3_terminal_window->lines[line].length = 0;
  _t3_terminal_window->lines[line].start = 0;

  for (ptr = _t3_tail != NULL && !_t3_tail->shown ? get_previous_window(_t3_tail) : _t3_tail;
       ptr != NULL; ptr = get_previous_window(ptr)) {
    if (ptr->lines == NULL) continue;

    y = t3_win_get_abs_y(ptr);
    if (y > line || y + ptr->height <= line) continue;

    if (ptr->parent == NULL) {
      parent_y = 0;
      parent_max_y = _t3_terminal_window->height;
      parent_x = 0;
      parent_max_x = _t3_terminal_window->width;
    } else {
      t3_window_t *parent = ptr->parent;
      parent_y = INT_MIN;
      parent_max_y = INT_MAX;
      parent_x = INT_MIN;
      parent_max_x = INT_MAX;

      do {
        int tmp;
        tmp = t3_win_get_abs_y(parent);
        if (tmp > parent_y) parent_y = tmp;
        tmp += parent->height;
        if (tmp < parent_max_y) parent_max_y = tmp;

        tmp = t3_win_get_abs_x(parent);
        if (tmp > parent_x) parent_x = tmp;
        tmp += parent->width;
        if (tmp < parent_max_x) parent_max_x = tmp;

        parent = parent->parent;
      } while (parent != NULL);
    }

    /* Skip lines that are clipped by the parent window. */
    if (line < parent_y || line >= parent_max_y) continue;

    if (parent_x < 0) parent_x = 0;
    if (parent_max_x > _t3_terminal_window->width) parent_max_x = _t3_terminal_window->width;

    draw = ptr->lines + line - y;
    x = t3_win_get_abs_x(ptr);

    /* Skip lines that are fully clipped by the parent window. */
    if (x >= parent_max_x || x + draw->start + draw->width < parent_x) continue;

    data_start = 0;
    /* Draw/skip unused leading part of line. */
    if (x + draw->start >= parent_x) {
      int start;
      if (x + draw->start > parent_max_x)
        start = parent_max_x - x;
      else
        start = draw->start;

      if (ptr->default_attrs == 0) {
        _t3_terminal_window->paint_x = x + start;
      } else if (x >= parent_x) {
        _t3_terminal_window->paint_x = x;
        result &= write_spaces_to_terminal_window(_t3_map_attr(ptr->default_attrs), start);
      } else {
        _t3_terminal_window->paint_x = parent_x;
        result &=
            write_spaces_to_terminal_window(_t3_map_attr(ptr->default_attrs), start - parent_x + x);
      }
    } else /* if (x < parent_x) */ {
      _t3_terminal_window->paint_x = parent_x;

      for (paint_x = x + draw->start; data_start < draw->length;
           data_start += (block_size >> 1) + block_size_bytes) {
        block_size = _t3_get_value(draw->data + data_start, &block_size_bytes);
        if (paint_x + _T3_BLOCK_SIZE_TO_WIDTH(block_size) > _t3_terminal_window->paint_x) break;
        paint_x += _T3_BLOCK_SIZE_TO_WIDTH(block_size);
      }

      if (data_start < draw->length && paint_x < _t3_terminal_window->paint_x) {
        paint_x += _T3_BLOCK_SIZE_TO_WIDTH(block_size);
        result &= write_spaces_to_terminal_window(get_block_attr(draw->data + data_start),
                                                  paint_x - _t3_terminal_window->paint_x);
        data_start += (block_size >> 1) + block_size_bytes;
      }
    }

    paint_x = _t3_terminal_window->paint_x;
    for (length = data_start; length < draw->length;
         length += (block_size >> 1) + block_size_bytes) {
      block_size = _t3_get_value(draw->data + length, &block_size_bytes);
      if (paint_x + _T3_BLOCK_SIZE_TO_WIDTH(block_size) > parent_max_x) break;
      paint_x += _T3_BLOCK_SIZE_TO_WIDTH(block_size);
    }

    if (length != data_start)
      result &=
          _win_write_blocks(_t3_terminal_window, draw->data + data_start, length - data_start);

    /* Add a space for the multi-cell character that is crossed by the parent clipping. */
    if (length < draw->length && paint_x == parent_max_x - 1)
      result &= write_spaces_to_terminal_window(get_block_attr(draw->data + length), 1);

    if (ptr->default_attrs != 0 && draw->start + draw->width < ptr->width &&
        x + draw->start + draw->width < parent_max_x) {
      result &= write_spaces_to_terminal_window(_t3_map_attr(ptr->default_attrs),
                                                x + ptr->width <= parent_max_x
                                                    ? ptr->width - draw->start - draw->width
                                                    : parent_max_x - x - draw->start - draw->width);
    }
  }

  /* If the default attributes for the terminal are not only a foreground color,
     we need to ensure that we paint the terminal. */
  if ((_t3_terminal_window->default_attrs & ~T3_ATTR_FG_MASK) != 0) {
    if (_t3_terminal_window->lines[line].start != 0) {
      _t3_terminal_window->paint_x = 0;
      result &=
          write_spaces_to_terminal_window(_t3_map_attr(_t3_terminal_window->default_attrs), 1);
    }

    if (_t3_terminal_window->lines[line].width + _t3_terminal_window->lines[line].start <
        _t3_terminal_window->width) {
      /* Make sure we fill the whole line. Adding the final character to an otherwise
         empty line doesn't do anything for us. */
      if (_t3_terminal_window->lines[line].width == 0) {
        _t3_terminal_window->paint_x = 0;
        result &=
            write_spaces_to_terminal_window(_t3_map_attr(_t3_terminal_window->default_attrs), 1);
      }
      _t3_terminal_window->paint_x = _t3_terminal_window->width - 1;
      result &=
          write_spaces_to_terminal_window(_t3_map_attr(_t3_terminal_window->default_attrs), 1);
    }
  }

  return result;
}

/** Clear current t3_window_t painting line to end. */
void t3_win_clrtoeol(t3_window_t *win) {
  if (win->paint_y >= win->height || win->lines == NULL) return;

  if (win->paint_x <= win->lines[win->paint_y].start) {
    win->lines[win->paint_y].length = 0;
    win->lines[win->paint_y].width = 0;
    win->lines[win->paint_y].start = 0;
  } else if (win->paint_x < win->lines[win->paint_y].start + win->lines[win->paint_y].width) {
    int sumwidth = win->lines[win->paint_y].start, i;
    uint32_t block_size;
    size_t block_size_bytes;

    block_size = _t3_get_value(win->lines[win->paint_y].data, &block_size_bytes);
    for (i = 0; i < win->lines[win->paint_y].length &&
                sumwidth + _T3_BLOCK_SIZE_TO_WIDTH(block_size) <= win->paint_x;
         i += (block_size >> 1) + block_size_bytes) {
      sumwidth += _T3_BLOCK_SIZE_TO_WIDTH(block_size);
      block_size = _t3_get_value(win->lines[win->paint_y].data + i, &block_size_bytes);
    }

    if (sumwidth < win->paint_x) {
      int spaces = win->paint_x - sumwidth;
      char space_str[8];
      size_t space_str_bytes;

      space_str_bytes = create_space_block(_t3_map_attr(win->default_attrs), space_str);
      if ((int)(spaces * space_str_bytes) < win->lines[win->paint_y].length - i ||
          ensure_space(win->lines + win->paint_y,
                       spaces * space_str_bytes - win->lines[win->paint_y].length + i)) {
        win->paint_x = sumwidth;
        for (; spaces > 0; spaces--) _win_write_blocks(win, space_str, space_str_bytes);
      }
    }

    win->lines[win->paint_y].length = i;
    win->lines[win->paint_y].width = win->paint_x - win->lines[win->paint_y].start;
  }
}

#define ABORT_ON_FAIL(x)                    \
  do {                                      \
    int retval;                             \
    if ((retval = (x)) != 0) return retval; \
  } while (0)

/** Draw a box on a t3_window_t.
    @param win The t3_window_t to draw on.
    @param y The line of the t3_window_t to start drawing on.
    @param x The column of the t3_window_t to start drawing on.
    @param height The height of the box to draw.
    @param width The width of the box to draw.
    @param attr The attributes to use for drawing.
    @return See ::t3_win_addnstr.
*/
int t3_win_box(t3_window_t *win, int y, int x, int height, int width, t3_attr_t attr) {
  int i;

  attr = t3_term_combine_attrs(attr | T3_ATTR_ACS, win->default_attrs);

  if (y >= win->height || y + height > win->height || x >= win->width || x + width > win->width ||
      win->lines == NULL)
    return -1;

  t3_win_set_paint(win, y, x);
  ABORT_ON_FAIL(t3_win_addch(win, T3_ACS_ULCORNER, attr));
  ABORT_ON_FAIL(t3_win_addchrep(win, T3_ACS_HLINE, attr, width - 2));
  ABORT_ON_FAIL(t3_win_addch(win, T3_ACS_URCORNER, attr));
  for (i = 1; i < height - 1; i++) {
    t3_win_set_paint(win, y + i, x);
    ABORT_ON_FAIL(t3_win_addch(win, T3_ACS_VLINE, attr));
    t3_win_set_paint(win, y + i, x + width - 1);
    ABORT_ON_FAIL(t3_win_addch(win, T3_ACS_VLINE, attr));
  }
  t3_win_set_paint(win, y + height - 1, x);
  ABORT_ON_FAIL(t3_win_addch(win, T3_ACS_LLCORNER, attr));
  ABORT_ON_FAIL(t3_win_addchrep(win, T3_ACS_HLINE, attr, width - 2));
  ABORT_ON_FAIL(t3_win_addch(win, T3_ACS_LRCORNER, attr));
  return T3_ERR_SUCCESS;
}

/** Clear current t3_window_t painting line to end and all subsequent lines fully. */
void t3_win_clrtobot(t3_window_t *win) {
  if (win->lines == NULL) return;

  t3_win_clrtoeol(win);
  for (win->paint_y++; win->paint_y < win->height; win->paint_y++) {
    win->lines[win->paint_y].length = 0;
    win->lines[win->paint_y].width = 0;
    win->lines[win->paint_y].start = 0;
  }
}

/** Find the top-most window at a location
    @return The top-most window at the specified location, or @c NULL if no
        window covers the specified location.
*/
t3_window_t *t3_win_at_location(int search_y, int search_x) {
  t3_window_t *ptr, *result = NULL;
  int y, x, parent_y, parent_x, parent_max_y, parent_max_x;

  for (ptr = _t3_tail != NULL && !_t3_tail->shown ? get_previous_window(_t3_tail) : _t3_tail;
       ptr != NULL; ptr = get_previous_window(ptr)) {
    y = t3_win_get_abs_y(ptr);
    if (y > search_y || y + ptr->height <= search_y) continue;

    x = t3_win_get_abs_x(ptr);
    if (x > search_x || x + ptr->width <= search_x) continue;

    if (ptr->parent != NULL) {
      t3_window_t *parent = ptr->parent;
      parent_y = INT_MIN;
      parent_max_y = INT_MAX;
      parent_x = INT_MIN;
      parent_max_x = INT_MAX;

      do {
        int tmp;
        tmp = t3_win_get_abs_y(parent);
        if (tmp > parent_y) parent_y = tmp;
        tmp += parent->height;
        if (tmp < parent_max_y) parent_max_y = tmp;

        tmp = t3_win_get_abs_x(parent);
        if (tmp > parent_x) parent_x = tmp;
        tmp += parent->width;
        if (tmp < parent_max_x) parent_max_x = tmp;

        parent = parent->parent;
      } while (parent != NULL);

      if (search_y < parent_y || search_y >= parent_max_y) continue;
      if (search_x < parent_x || search_x >= parent_max_x) continue;
    }

    result = ptr;
  }
  return result;
}

/** @} */
