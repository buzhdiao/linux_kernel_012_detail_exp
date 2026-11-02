/*
 * lib/clib.c —— 用户态 C 库实现
 * 对应书中 8.3 节"构建C语言库"
 * 来源：官方仓 lib/ + tools/tcc/runtime/ 的教学精简版
 *
 * 库的三个组成部分：
 *   1. 系统调用封装：int 0x80 的安全包装（寄存器约定集中在这一处）
 *   2. 文件/进程接口：open/read/close/getpid……直接映射到系统调用
 *   3. printf：在用户态做格式化（复用内核 vsprintf 的算法思想），
 *      输出经 write(1, ...)——"用户态格式化 + 系统调用输出"正是
 *      真实 libc 的分层
 */

#include "clib.h"

int syscall(int nr, int arg1, int arg2, int arg3)
{
	int ret;
	__asm__ volatile("int $0x80" : "=a"(ret)
			 : "a"(nr), "b"(arg1), "c"(arg2), "d"(arg3));
	return ret;
}

pid_t getpid(void)          { return syscall(13, 0, 0, 0); }
void exit_now(int code)     { syscall(16, code, 0, 0); for (;;); }
int uptime(void)            { return syscall(14, 0, 0, 0); }
int open(const char *p, int m)  { return syscall(10, (int)p, m, 0); }
int read(int fd, void *b, int n){ return syscall(11, fd, (int)b, n); }
int write(int fd, const void *b, int n) { return syscall(8, (int)b, n, 0); }
int close(int fd)           { return syscall(12, fd, 0, 0); }

/* ---- printf：极简变参格式化（%d %x %s %c %%） ---- */

/* 把无符号数转成 ASCII（复用内核 number 的算法） */
static char *num2str(char *s, unsigned long v, int base, int *len)
{
	char tmp[32];
	int i = 0;

	do {
		int d = v % base;
		tmp[i++] = d < 10 ? '0' + d : 'a' + d - 10;
		v /= base;
	} while (v);
	*len = i;
	while (i--)
		*s++ = tmp[i];
	return s;
}

void printf(const char *fmt, ...)
{
	char out[512];
	char *o = out;
	int a = 0;		/* 变参游标（教学版：按 int 步进取参） */
	int i;
	int total = 0;

	/* 变参指针：直接按栈布局取（等价 va_arg 的手工版）。
	 * int $0x80 之前这些参数在栈上连续排布 */
	{
		int *args = (int *)&fmt + 1;	/* fmt 之后的第 1 个参数 */

		for (i = 0; fmt[i]; i++) {
			if (fmt[i] != '%') {
				*o++ = fmt[i];
				continue;
			}
			switch (fmt[++i]) {
			case 'd': {
				int v = args[a++];
				if (v < 0) {
					*o++ = '-';
					v = -v;
				}
				o = num2str(o, v, 10, &i);
				i = -1;		/* 修正外层 for 的 ++i */
				break;
			}
			case 'x':
				o = num2str(o, args[a++], 16, &i);
				i = -1;
				break;
			case 's': {
				const char *s = (const char *)args[a++];
				while (*s)
					*o++ = *s++;
				break;
			}
			case 'c':
				*o++ = (char)args[a++];
				break;
			case '%':
				*o++ = '%';
				break;
			}
			(void)total;
		}
	}
	*o = 0;
	write(1, out, o - out);
}
