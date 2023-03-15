#include "../include/stdio.h"
#include "../include/syscall.h"

static char buffer[1024];

int printf(const char* fmt, ...) {
    va_list args;
    int i;
    va_start(args, fmt);
    i = vsprintf(buffer, fmt, args);
    va_end(args);
    write(STDOUT_FILENO, buffer, i);
    return i;
}