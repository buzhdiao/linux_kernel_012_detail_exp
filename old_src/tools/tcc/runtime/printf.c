#include <stdio.h>
#include <unistd.h>

char printbuf[256];

int printf(const char* fmt, ...) {
    va_list args;
    int i;

    va_start(args, fmt);
    write(1, printbuf, i = vsprintf(printbuf, fmt, args));
    va_end(args);

    return i;
}

