/* Copyright (C) 2011 G.P. Halkes
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
#ifndef T3_UTF8_H
#define T3_UTF8_H

/** @defgroup t3window_other Functions, constants and enums. */
/** @addtogroup t3window_other */
/** @{ */
#include <stddef.h>
#include <stdint.h>
#include <unictype.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <t3window/window_api.h>

/** Category mask for libunistring's @c uc_is_general_category_withtable for
    finding control characters. */
#define T3_UTF8_CONTROL_MASK                                                               \
  (UC_CATEGORY_MASK_Cs | UC_CATEGORY_MASK_Cf | UC_CATEGORY_MASK_Co | UC_CATEGORY_MASK_Cc | \
   UC_CATEGORY_MASK_Zl | UC_CATEGORY_MASK_Zp)

T3_WINDOW_API uint32_t t3_utf8_get(const char *src, size_t *size);
T3_WINDOW_API size_t t3_utf8_put(uint32_t c, char *dst);

T3_WINDOW_API int t3_utf8_wcwidth(uint32_t c);

T3_WINDOW_API int t3_utf8_wcwidth_ext(uint32_t c, int *state);
/** @} */
#ifdef __cplusplus
} /* extern "C" */
#endif
#endif
