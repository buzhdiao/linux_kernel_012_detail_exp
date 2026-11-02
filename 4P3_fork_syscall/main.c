/*
 * main.c —— 4P3 实验：fork 系统调用 + 写时复制验证
 * 对应书中 4.3 节"第一个系统调用fork"
 *
 * 演示流程（task0 用户态）：
 *   1. fork() —— 内核 copy_process 出 task1（线性空间 64MB 起，
 *      页映射全部写时复制）
 *   2. 父子各自打印 fork 返回值（父 = 1，子 = 0）
 *   3. 子进程写 cow_var = 200：第一次写触发 14 号缺页（写保护）→
 *      do_wp_page 复制私有页 → 子进程从此看到 200
 *   4. 父进程读 cow_var 仍是 100 —— 两进程数据隔离的活证据
 *
 * 注意：fork 之后父子都不能再用栈上的局部变量做复杂操作之前，
 * 这里刻意让后续代码只用寄存器和全局变量——因为 task0 的用户栈
 * 物理页 < 1MB，fork 时也被标了只读，第一次写会走 COW（这个实验
 * 的 do_wp_page 放开了官方的 TASK_SIZE 检查，允许处理）
 */

#include "kernel.h"
#include "sched.h"

#define EXT_MEM_K (*(unsigned short *)0x90002)

extern void trap_init(void);

/* 写时复制的验证变量：位于内核镜像 .data（物理页 < 640KB），
 * fork 后父子通过各自的线性视图访问同一物理页（只读） */
volatile int cow_var = 100;

/* 用户态系统调用封装：EAX=调用号，EBX=参数（必须 static inline，
 * 否则 call 指令要写用户栈——见文件头注释） */
static inline int sys_call(int nr, int arg)
{
	int ret;
	__asm__ volatile("int $0x80" : "=a"(ret) : "a"(nr), "b"(arg));
	return ret;
}

/* fork 的用户态封装：无参数，返回值在 EAX */
static inline int fork_u(void)
{
	int ret;
	__asm__ volatile("int $0x80" : "=a"(ret) : "a"(2));
	return ret;
}

/* 自旋延时：写 volatile 全局变量，gcc 无法优化掉空循环 */
volatile int spin_sink;
static void spin(void)
{
	for (spin_sink = 0; spin_sink < 8000000; spin_sink++)
		;
}

void main(void)
{
	long memory_end;
	long free_pages;
	int pid;

	memory_end = (1 << 20) + (EXT_MEM_K << 10);
	if (memory_end > 16 * 1024 * 1024)
		memory_end = 16 * 1024 * 1024;
	free_pages = mem_init(1 * 1024 * 1024, memory_end);

	trap_init();			/* 含 0x0e 缺页门 + 0x20 时钟门 + 0x80 系统调用门 */
	con_init();
	sched_init();

	printk("\n[4P3] fork + copy-on-write\n\r");
	printk("free pages: %ld\n\r", free_pages);

	__asm__ volatile("sti");
	move_to_user_mode();

	/* ---------- fork ---------- */
	pid = fork_u();

	if (pid == 0) {
		/* ---- 子进程：fork 返回 0 ---- */
		sys_call(5, 0);			/* 打印自己的返回值 0 */
		cow_var = 200;			/* ★ 触发写保护缺页 → COW */
		for (;;) {
			sys_call(4, cow_var);	/* 打印 200 */
			spin();
		}
	}

	/* ---- 父进程：fork 返回子进程 pid ---- */
	sys_call(5, pid);			/* 打印返回值 1 */
	for (;;) {
		sys_call(4, cow_var);		/* 打印 100 */
		spin();
	}
}
