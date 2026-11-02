/*
 * main.c —— 5P4 实验：termios 标志（ICANON/ECHO）+ 方向键
 * 对应书中 5.4 节"修改终端状态"（方向键 / 控制台标志位）
 *
 * 演示流程（读者任务）：
 *   1. 规范模式读一行（5P2 行为）
 *   2. ioctl 关 ICANON → 原始模式：每字符即交付（vim/密码输入的模式）
 *   3. ioctl 关 ECHO → 输入不回显（密码模式），再开回来
 *   方向键（E0 前缀扫描码）在内核里直接移动屏幕光标
 */

#include "kernel.h"
#include "sched.h"

#define EXT_MEM_K (*(unsigned short *)0x90002)

extern void trap_init(void);
extern long mem_init(long start, long end);

static inline int sys_call(int nr, int arg, int arg2, int arg3)
{
	int ret;
	__asm__ volatile("int $0x80" : "=a"(ret) : "a"(nr), "b"(arg), "c"(arg2), "d"(arg3));
	return ret;
}

static inline int fork_u(void)
{
	int ret;
	__asm__ volatile("int $0x80" : "=a"(ret) : "a"(2));
	return ret;
}

volatile int spin_sink;
static void spin(void)
{
	for (spin_sink = 0; spin_sink < 4000000; spin_sink++)
		;
}

void main(void)
{
	long memory_end;

	memory_end = (1 << 20) + (EXT_MEM_K << 10);
	if (memory_end > 16 * 1024 * 1024)
		memory_end = 16 * 1024 * 1024;
	mem_init(1 * 1024 * 1024, memory_end);

	trap_init();
	con_init();
	sched_init();

	printk("\n[6P3] hd driver demo\n\r");
	__asm__ volatile("sti");

	/* 块设备驱动用"睡眠等中断完成"模型，必须在开中断之后跑 */
	hd_init();		/* 输出已精简，详见 6P1-6P4 */
	floppy_init();
	floppy_demo();

	/* 7.1~7.4：挂载根文件系统 + 目录操作（写路径） */
	{
		extern void mount_root(void);
		extern void dir_demo(void);
		extern void link_demo(void);
		mount_root();
		dir_demo();
		link_demo();
	}

	printk("\n[7P5] done\n\r");

	for (;;) {
		sys_call(7, 100, 0, 0);
	}
}
