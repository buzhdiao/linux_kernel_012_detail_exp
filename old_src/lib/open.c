#define __LIBRARY__
#include <unistd.h>
#include <stdarg.h>

_syscall0(int, ls)

int open(const char * filename, int flag, ...) {
    register int res;
    va_list arg;

    va_start(arg,flag);
    __asm__("int $0x80"
            :"=a" (res)
            :"a" (__NR_open),"b" (filename),"c" (flag),
            "d" (va_arg(arg,int)));

    if (res >= 0)
        return res;
    errno = -res;
    return -1;
}

