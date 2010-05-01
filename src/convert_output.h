#ifndef T3_CONVERT_OUTPUT_H
#define T3_CONVERT_OUTPUT_H

#include "terminal.h"

t3_bool _t3_init_output_buffer(void);
t3_bool _t3_init_output_iconv(const char *encoding);
void _t3_output_buffer_print(void);

#endif
