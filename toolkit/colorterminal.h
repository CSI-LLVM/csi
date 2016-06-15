#ifndef __CSI_COLOR_TERMINAL_H__
#define __CSI_COLOR_TERMINAL_H__

#include <stdio.h>

#define CSI_TERM_COLOR_RED     "\x1b[31m"
#define CSI_TERM_COLOR_GREEN   "\x1b[32m"
#define CSI_TERM_COLOR_YELLOW  "\x1b[33m"
#define CSI_TERM_COLOR_BLUE    "\x1b[34m"
#define CSI_TERM_COLOR_MAGENTA "\x1b[35m"
#define CSI_TERM_COLOR_CYAN    "\x1b[36m"
#define CSI_TERM_COLOR_RESET   "\x1b[0m"

#define fprintf_cyan(stream, format, ...)          \
    do {                                           \
        fprintf(stream, CSI_TERM_COLOR_CYAN);      \
        fprintf(stream, format, ##__VA_ARGS__);    \
        fprintf(stream, CSI_TERM_COLOR_RESET);     \
    } while(0)

#endif
