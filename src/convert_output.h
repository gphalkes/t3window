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
#ifndef T3_CONVERT_OUTPUT_H
#define T3_CONVERT_OUTPUT_H

#include "terminal.h"

T3_WINDOW_LOCAL t3_bool _t3_init_output_buffer(void);
T3_WINDOW_LOCAL void _t3_free_output_buffer(void);
T3_WINDOW_LOCAL t3_bool _t3_init_output_converter(const char *encoding);
T3_WINDOW_LOCAL void _t3_output_buffer_print(void);

#endif
