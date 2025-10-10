#ifndef _SYS_TIME_H
#define _SYS_TIME_H

struct timeval {
    long    tv_sec;
    long    tv_usec;
};

struct timezone {
    int     tz_minuteswest;
    int     tz_dsttime;
};

#include <time.h>

#endif

