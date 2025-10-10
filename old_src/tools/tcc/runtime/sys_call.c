#define __LIBRARY__
#include <unistd.h>
#include <stdarg.h>

_syscall3(int,write,int,fd,const char *,buf,off_t,count)
_syscall1(int,exit,int,fd);
_syscall3(int,read,int,fd,const char *,buf,off_t,count)
_syscall2(int, mkdir, const char *, pathname, mode_t, mode)
_syscall1(int, rmdir, const char *, pathname)
_syscall1(int, chdir, const char *, filename)
_syscall3(int,execve,const char *,file,char **,argv,char **,envp)
_syscall0(int,fork);
_syscall1(int, pipe, unsigned int *, filedes)

_syscall3(pid_t,waitpid,pid_t,pid,int *,wait_stat,int,options)

pid_t wait(int * wait_stat) {
    return waitpid(-1,wait_stat,0);
}

int open(const char * filename, int flag, ...) {
    register int res;
    va_list arg;

    va_start(arg,flag);
    __asm__("int $0x80"
        :"=a" (res)
        :"0" (__NR_open),"b" (filename),"c" (flag),
        "d" (va_arg(arg,int)));
    if (res>=0)
        return res;
    errno = -res;
    return -1;
}
