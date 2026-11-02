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

static inline int sys_call(int nr, int arg, int arg2)
{
	int ret;
	__asm__ volatile("int $0x80" : "=a"(ret) : "a"(nr), "b"(arg), "c"(arg2));
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

static char line_buf[64];

/* 读者任务：依次演示规范模式 → 原始模式 → 关回显 */
void demo_task(void)
{
	int n, i;

	sys_call(0, (int)"[demo] 1) canonical: type a line + ENTER", 0);
	n = sys_call(6, (int)line_buf, 63);
	(void)n;
	sys_call(0, (int)"[demo]    got the line", 0);

	sys_call(9, 1, 0);	/* ioctl: 关 ICANON → 原始模式 */
	sys_call(0, (int)"[demo] 2) raw mode: 3 chars, each delivered at once", 0);
	for (i = 0; i < 3; i++) {
		n = sys_call(6, (int)line_buf, 63);
		(void)n;
		sys_call(0, (int)"[demo]    raw char delivered", 0);
	}

	sys_call(9, 1, 0);	/* 回到规范模式 */
	sys_call(9, 2, 0);	/* 关 ECHO：输入不回显（密码模式） */
	sys_call(0, (int)"[demo] 3) echo off: type, nothing shows until ENTER", 0);
	n = sys_call(6, (int)line_buf, 63);
	sys_call(0, (int)"[demo]    (echo was off) got the line", 0);
	sys_call(9, 2, 0);	/* 开回 ECHO */

	sys_call(0, (int)"[demo] done. arrow keys move the cursor!", 0);
	for (;;)
		sys_call(7, 100, 0);
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

	hd_init();

	printk("\n[6P1] done\n\r");

	for (;;) {
		sys_call(7, 100, 0);	/* idle */
	}
}
