/* Copyright (C) 2012,2018 G.P. Halkes
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
#include "log.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#ifdef _T3_WINDOW_DEBUG
static FILE *log_file;

static void close_log(void) { fclose(log_file); }

void init_log(void) {
  if (log_file == NULL) {
    log_file = fopen("libt3windowlog.txt", "a");
    if (log_file) atexit(close_log);
  }
}

void lprintf(const char *fmt, ...) {
  if (log_file) {
    va_list args;

    va_start(args, fmt);
    vfprintf(log_file, fmt, args);
    fflush(log_file);
    va_end(args);
  }
}

#endif
