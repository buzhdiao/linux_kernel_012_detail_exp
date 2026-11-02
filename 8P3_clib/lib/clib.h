/*
 * lib/clib.h —— 用户态 C 库的接口（8.3 构建C语言库）
 * 用户程序 #include "clib.h" 后可以像写普通 C 一样调用
 * printf/getpid/read/write——库内部把它们翻译成 int 0x80
 */

#ifndef _CLIB_H
#define _CLIB_H

typedef int pid_t;

/* 系统调用原语（库内部使用） */
int syscall(int nr, int arg1, int arg2, int arg3);

/* 进程 */
pid_t getpid(void);
void exit_now(int code) __attribute__((noreturn));

/* 时间 */
int uptime(void);

/* 文件 */
int open(const char *path, int mode);
int read(int fd, void *buf, int n);
int write(int fd, const void *buf, int n);
int close(int fd);

/* 格式化输出：printf 的内核够用子集（%d %x %s %c） */
void printf(const char *fmt, ...);

#endif
