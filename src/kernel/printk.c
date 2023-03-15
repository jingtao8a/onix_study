#include "../include/printk.h"
#include "../include/stdio.h"
#include "../include/console.h"

static char buffer[1024];

int printk(const char* fmt, ...) {
    va_list args;
    int i;
    va_start(args, fmt);
    i = vsprintf(buffer, fmt, args);
    va_end(args);
    console_write(NULL, buffer, i);
    return i;        
}
