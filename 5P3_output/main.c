/*
 * main.c —— 5P3 实验：write 系统调用 + ANSI 转义序列
 * 对应书中 5.3 节"优化输出功能"（写标准输出 / ANSI转义序列）
 *
 * 子任务用 write() 输出带转义序列的内容：红色提示、绿色横幅、
 * 光标定位画一个角标——终端模拟器能力的展示
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

/* ANSI 序列的字符串形式（write 逐字节透传给终端状态机） */
static const char red_msg[]   = "\x1b[31m[writer] this line is RED\x1b[0m\n\r";
static const char green_msg[] = "\x1b[32m[writer] this line is GREEN\x1b[0m\n\r";
static const char yel_msg[]   = "\x1b[33m[writer] this line is YELLOW\x1b[0m\n\r";
static const char goto_msg[]  = "\x1b[24;60H\x1b[36mCORE\x1b[0m";  /* 定位右下角 */

static const char *msgs[] = { red_msg, green_msg, yel_msg };
static int msg_i;

void writer_task(void)
{
	sys_call(0, (int)"[writer] alive, colored lines follow:", 0);
	for (;;) {
		sys_call(8, (int)msgs[msg_i % 3], 128);	/* write(1, msg, len)：
							   消息以 0 结尾，内核按 0 截断 */
		msg_i++;
		if (msg_i == 3)
			sys_call(8, (int)goto_msg, 128);
		sys_call(7, 30, 0);			/* idle 0.3s */
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

	printk("\n[5P3] write + ANSI escape sequences\n\r");

	__asm__ volatile("sti");
	move_to_user_mode();

	if (fork_u() == 0) {
		writer_task();
	}

	for (;;) {
		spin();
		sys_call(7, 30, 0);
	}
}
