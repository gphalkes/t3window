/* Complie with:
   clang -D_XOPEN_SOURCE -o compat_test compat_test.c \
     ../src/.objects/generated/.libs/chardata.o -I../src \
     -I../../t3shared/include -l t3window -lunistring
*/
#include "generated/chardata.h"
#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <uniwidth.h>
#include <wchar.h>

int t3_utf8_wcwidth(uint32_t c) {
  static const char nul;
  if (c >= 0x1160 && c < 0x11fa) {
    return 0;
  } else if (c == 0x00ad) {
    return 1;
  } else if (c == 0) {
    return -1;
  }
  return uc_width(c, &nul);
}

int new_width(uint32_t c) {
  uint8_t data = get_chardata(c);
  if ((data & 0x3f) != 0x3f) {
    return (int)(data >> 6) - 1;
  }
  return 1;
}

int available_since(uint32_t c) {
  uint8_t data = get_chardata(c);
  return data & 0x3f;
}

int main() {
  uint32_t i;
  setlocale(LC_ALL, "");

  for (i = 0; i < 0x110000; ++i) {
    int old = t3_utf8_wcwidth(i);
    int system = wcwidth(i);
    int new = new_width(i);
    if (old != new || (system != new &&available_since(i) < 0x3f)) {
      printf("Difference: %04X old: %d system: %d new: %d\n", i, old, system, new);
    }
  }
  return 0;
}
