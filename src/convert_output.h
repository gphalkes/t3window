#ifndef T3_CONVERT_OUTPUT_H
#define T3_CONVERT_OUTPUT_H

#include "terminal.h"

T3_WINDOW_LOCAL t3_bool _t3_init_output_buffer(void);
T3_WINDOW_LOCAL t3_bool _t3_init_output_iconv(const char *encoding);
T3_WINDOW_LOCAL void _t3_output_buffer_print(void);

#endif
