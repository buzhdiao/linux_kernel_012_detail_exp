/*
 * main.c —— 4P1 实验：创建 INIT 进程，切换到用户态
 * 对应书中 4.1 节"创建INIT进程"
 *
 * 初始化顺序（对齐官方仓 kernel/main.c 的骨架）：
 *   mem_init → trap_init → con_init → sched_init → move_to_user_mode
 *
 * 验证特权级切换成功的两个证据：
 *   1. 用户态读 CS = 0x0F（LDT 用户代码段，RPL=3）
 *   2. 用户态 int 0x80 进入内核打印——往返一次，说明 CPL3↔CPL0
 *      的通道（TSS.esp0 + 系统调用门）都是通的
 */

#include "kernel.h"
#include "sched.h"

#define EXT_MEM_K (*(unsigned short *)0x90002)

extern long mem_init(long start, long end);
extern void trap_init(void);

/* 用户态测试代码：从 move_to_user_mode 返回后执行 */
static void user_test(void)
{
	unsigned short cs, ss;
	int ret;

	/* 读当前代码段/栈段选择子（这些指令在用户态允许执行） */
	__asm__ volatile("mov %%cs, %0" : "=r"(cs));
	__asm__ volatile("mov %%ss, %0" : "=r"(ss));

	/*
	 * 系统调用：EAX 传 CS 值。若特权级切换成功，CS=0x0F，
	 * 内核侧 sys_demo 会打印 "nr = 15"
	 */
	__asm__ volatile("int $0x80" : "=a"(ret) : "a"((int)cs));
	(void)ss;

	/*
	 * 用户态不能再用 printk（那是内核函数，而且用户段也够不着
	 * 内核数据），后续输出都走系统调用。这里也不能 hlt（特权指令），
	 * 用死循环结束实验
	 */
	for (;;)
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

	printk("\n[4P1] create INIT process\n\r");
	printk("task[0] tss.esp0 = 0x%08lx, ldt sel = 0x%lx\n\r",
	       init_task.task.tss.esp0, init_task.task.tss.ldt);
	printk("switching to user mode (CPL 0 -> 3) ...\n\r");

	/* 切换到用户态，不返回（返回地址被 iret 吃掉了） */
	move_to_user_mode();

	/* 以下是用户态代码 */
	user_test();
}
