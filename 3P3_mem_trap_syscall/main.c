/*
 * main.c —— 3P3 实验：内存管理初始化 + CPU 异常 + 系统调用骨架
 * 对应书中 3.3 节"设置内存和陷阱处理"
 *
 * main() 的初始化顺序从此定型（与官方仓 kernel/main.c 同构）：
 *   内存初始化 → 陷阱/异常初始化 → 控制台初始化 → ... → 演示与停机
 */

#include "kernel.h"

/* setup.S 采集的扩展内存大小（KB），存放在 0x90002 */
#define EXT_MEM_K (*(unsigned short *)0x90002)

extern long mem_init(long start, long end);
extern void trap_init(void);
extern void sys_demo(int nr);

void main(void)
{
	long memory_end;
	long free_pages;
	int ret;
	volatile int zero = 0;
	volatile int one = 1;

	/* ---- 内存初始化 ----
	 * 物理内存总量 = 1MB + 扩展内存；上限 16MB（页表只映射了 16MB） */
	memory_end = (1 << 20) + (EXT_MEM_K << 10);
	if (memory_end > 16 * 1024 * 1024)
		memory_end = 16 * 1024 * 1024;

	/* 主内存从 1MB 之后开始（0~1MB 是内核 + BIOS 区） */
	free_pages = mem_init(1 * 1024 * 1024, memory_end);

	/* ---- 异常与系统调用初始化 ---- */
	trap_init();

	/* ---- 控制台初始化并打印 ---- */
	con_init();

	printk("\n[3P3] memory & trap & syscall\n\r");
	printk("memory end: 0x%lx (%ld MB), free pages: %ld (%ld KB)\n\r",
	       memory_end, memory_end >> 20, free_pages, free_pages * 4);

	/* ---- 系统调用演示：int 0x80，EAX 放调用号 ---- */
	printk("trigger int 0x80 with nr=1 ...\n\r");
	__asm__ volatile("int $0x80" : "=a"(ret) : "a"(1));
	printk("returned from int 0x80\n\r");

	/* ---- CPU 异常演示：除零错误 ----
	 * 注意不能直接写 C 的 one/zero：除零是 C 的未定义行为，
	 * gcc 即使面对 volatile 变量也可能把除法整个优化掉，
	 * 必须用内嵌汇编强制生成 idivl 指令 */
	printk("trigger divide by zero ...\n\r");
	__asm__ volatile(
		"idivl %2"			/* EDX:EAX / 除数，除数为 0 触发 0 号异常 */
		: "=a"(ret)
		: "a"(one), "r"(zero), "d"(0));
	(void)ret;		/* 到不了这里：异常处理打印现场后停机 */

	for (;;)
		;
}
