/*
 * kernel.h —— 本实验的内核公共头文件
 * 类型定义、端口 IO 原语、控制台与 printk 的函数声明
 */

#ifndef _KERNEL_H
#define _KERNEL_H

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef unsigned long  u32l;

/* 向端口写一个字节 */
static inline void outb(u8 value, u16 port)
{
	__asm__ volatile("outb %0,%1" :: "a"(value), "d"(port));
}

/* 从端口读一个字节 */
static inline u8 inb(u16 port)
{
	u8 v;
	__asm__ volatile("inb %1,%0" : "=a"(v) : "d"(port));
	return v;
}

/* 带短暂停顿的端口写：访问 0x3D4 这类老式寄存器时更稳妥 */
static inline void outb_p(u8 value, u16 port)
{
	__asm__ volatile("outb %0,%1\n\tjmp 1f\n1:\tjmp 1f\n1:" :: "a"(value), "d"(port));
}

/* console.c */
void con_init(void);
void con_putc(char c);
void con_puts(const char *s);

/* printk.c */
int printk(const char *fmt, ...);

/* vsprintf.c */
int vsprintf(char *buf, const char *fmt, __builtin_va_list args);

#endif

/* memory.c */
long mem_init(long start, long end);
long count_free_pages(void);

/* traps.c */
void trap_init(void);

/* syscall.c */
void sys_print(int nr, const char *s);
