#ifndef CONVERT_OUTPUT_H
#define CONVERT_OUTPUT_H

#include "terminal.h"

Bool init_output_buffer(void);
Bool init_output_iconv(const char *encoding);
Bool output_buffer_add(char c);
void output_buffer_print(void);

#endif
