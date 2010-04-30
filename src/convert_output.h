#ifndef T3_CONVERT_OUTPUT_H
#define T3_CONVERT_OUTPUT_H

#include "terminal.h"

T3Bool _t3_init_output_buffer(void);
T3Bool _t3_init_output_iconv(const char *encoding);
void _t3_output_buffer_print(void);

#endif
