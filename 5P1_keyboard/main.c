/*
 * main.c —— 5P1 实验：键盘中断 + 扫描码解析 + 缓冲 + 回显
 * 对应书中 5.1 节"支持键盘"
 *
 * task0（fork 出的 task1 同款）：死循环 getchar()，拿到字符后打印
 * "got: X"。字符本身由键盘中断回显——所以按键会看到两个效果：
 *   1. 中断里的直接回显（con_putc）
 *   2. 任务从缓冲里取到后打印 "got: X"
 * 这正好演示"中断生产 / 任务消费"的异步模型
 */

#include "kernel.h"
#include "sched.h"

#define EXT_MEM_K (*(unsigned short *)0x90002)

extern void trap_init(void);
extern long mem_init(long start, long end);

static inline int sys_call(int nr, int arg)
{
	int ret;
	__asm__ volatile("int $0x80" : "=a"(ret) : "a"(nr), "b"(arg));
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
	for (spin_sink = 0; spin_sink < 2000000; spin_sink++)
		;
}

/* 把字符转成可打印形式（回车/退格等控制字符显示成 \r \b） */
static void print_got(int c)
{
	char name[2];

	name[0] = (c >= 32 && c < 127) ? c : '?';
	name[1] = 0;
	if (c == '\n')
		sys_call(0, (int)"got: ENTER");
	else if (c == '\b')
		sys_call(0, (int)"got: BACKSPACE");
	else
		sys_call(0, (int)name);
}

void kbd_reader(void)
{
	for (;;) {
		int c = sys_call(6, 0);		/* getchar：阻塞等键盘 */
		print_got(c);
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

	printk("\n[5P1] keyboard driver: type something!\n\r");
	printk("PIC IMR = 0x%02x (bit0=IRQ0 bit1=IRQ1, 0=open)\n\r", inb(0x21));

	__asm__ volatile("sti");
	move_to_user_mode();

	/* 父进程：fork 出一个读者，自己演示心跳 */
	if (fork_u() == 0) {
		kbd_reader();
	}

	for (;;) {
		sys_call(0, (int)"[heartbeat] type keys, reader echoes them");
		sys_call(7, 30);	/* idle 0.3s（hlt 睡眠） */
	}
}
