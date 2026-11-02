/*
 * main.c —— 5P2 实验：tty 规范模式读（攒行）+ 阻塞 read 系统调用
 * 对应书中 5.2 节"优化输入功能"
 *
 * 演示与 5P1 的区别：5P1 是"每个按键唤醒一次"；本实验 read 攒到
 * 回车才返回一整行——中途的按键只回显不唤醒。这是 shell 的读法
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
	for (spin_sink = 0; spin_sink < 8000000; spin_sink++)
		;
}

static char line_buf[64];		/* 子进程的用户缓冲（.data，恒等可及） */

/* 读者任务：read() 阻塞等一整行，收到后打印 */
void kbd_reader(void)
{
	for (;;) {
		int n = sys_call(6, (int)line_buf, 63);	/* read(0, buf, 63) */
		line_buf[n] = 0;
		if (n > 0)
			sys_call(0, (int)"[reader] line received", 0);
	}
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

	printk("\n[5P2] tty read: type a line, press ENTER\n\r");

	__asm__ volatile("sti");
	move_to_user_mode();

	if (fork_u() == 0) {
		kbd_reader();
	}

	for (;;) {
		sys_call(0, (int)"[heartbeat] waiting for a line...", 0);
		spin();
		sys_call(7, 30, 0);	/* idle：hlt 让出 CPU，否则 TCG 忙转
					   饿死事件循环，sendkey 收不到 */
	}
}
