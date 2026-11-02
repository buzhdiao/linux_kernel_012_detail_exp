/*
 * main.c —— 4P2 实验：两个进程轮流运行
 * 对应书中 4.2 节"创建第二个进程"
 *
 * task0 = main 的延续（move_to_user_mode 之后进入 user_loop0），
 * task1 = sched_init 手工构造，TSS.eip 指向 user_task1。
 * 两个用户态任务干同样的事：每隔约 1 秒打印一行自己的编号。
 * 100Hz 时钟中断不断触发 schedule()，轮转调度让两行输出交替出现
 * ——这就是多任务的直观形态
 */

#include "kernel.h"
#include "sched.h"

#define EXT_MEM_K (*(unsigned short *)0x90002)

extern long mem_init(long start, long end);
extern void trap_init(void);

/*
 * 用户态发起系统调用的唯一通道：int 0x80（DPL=3 的调用门）。
 * EAX = 系统调用号，EBX = 参数。用户态绝不能直接 call 内核函数——
 * 那样代码确实能执行（用户代码段够得着内核地址），但一碰到特权指令
 * （如 printk 最终的 out 0x3d4 写光标）就 #GP，本实验的第一个版本
 * 正是栽在这里
 */
static int sys_call(int nr, int arg)
{
	int ret;
	__asm__ volatile("int $0x80" : "=a"(ret) : "a"(nr), "b"(arg));
	return ret;
}

/* 任务间短暂让出 CPU 的忙等延时（约 1 秒），期间时钟中断会抢占 */
static void delay_1s(void)
{
	volatile int i;
	for (i = 0; i < 20000000; i++)
		;
}

/* 任务 0 的用户态代码：main 的延续 */
static void user_loop0(void)
{
	for (;;) {
		sys_call(0, (int)"hello from task 0");
		delay_1s();
	}
}

/* 任务 1 的用户态代码：sched_init 里把它填进 task1 的 TSS.eip */
void user_task1(void)
{
	for (;;) {
		sys_call(1, (int)"hello from task 1");
		delay_1s();
	}
}

void main(void)
{
	long memory_end;

	memory_end = (1 << 20) + (EXT_MEM_K << 10);
	if (memory_end > 16 * 1024 * 1024)
		memory_end = 16 * 1024 * 1024;
	mem_init(1 * 1024 * 1024, memory_end);

	trap_init();		/* 含 0x20 时钟中断门 */
	con_init();
	sched_init();		/* task[0]/task[1] 挂上 GDT + 8254 设 100Hz */

	printk("\n[4P2] two processes take turns\n\r");

	/*
	 * 开中断（此前 head.S 到现在一直是 IF=0），时钟中断开始驱动调度。
	 * 放在 move_to_user_mode 之前：中断随时可能来，内核态里处理最安全
	 */
	__asm__ volatile("sti"); /* TEST A: 暂不开启 */ 

	move_to_user_mode();	/* 任务 0 进入用户态，开始它的循环 */

	user_loop0();
}
