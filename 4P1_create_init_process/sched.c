/*
 * sched.c —— 进程的诞生：任务表、INIT 进程、TSS/LDT 挂接、特权级切换
 * 对应书中 4.1 节"创建INIT进程"
 *
 * 来源：官方仓 kernel/sched.c 的 sched_init 部分 + include/asm/system.h 的
 * move_to_user_mode 宏（本实验整理成函数便于打断点/注释）
 */

#include "kernel.h"
#include "sched.h"

#define PAGE_SIZE 4096

/* 任务 0（INIT 进程）本体：定义见 sched.h 的注释，初值逐字段标注 */
union task_union init_task = {
	/* ---- state, counter, priority, signal, blocked ---- */
	.task.state   = TASK_RUNNING,
	.task.counter = 15,		/* 与 priority 相同的时间片初值 */
	.task.priority = 15,
	/* ---- 进程布局 / pid 等：0 即可 ---- */
	/* ---- ldt[3]：空 + 用户代码段 + 用户数据段 ---- */
	.task.ldt = {
		{ 0, 0 },
		{ 0x9f, 0xc0fa00 },	/* 基址0 限长640KB 代码段 DPL=3 */
		{ 0x9f, 0xc0f200 },	/* 基址0 限长640KB 数据段 DPL=3 */
	},
	/* ---- tss ---- */
	.task.tss = {
		/* esp0：内核栈顶 = 本页顶端；ss0：内核数据段选择子 */
		.esp0 = (long)(init_task.stack + PAGE_SIZE),
		.ss0 = 0x10,
		/* cr3：页目录（0x0，恒等映射） */
		.cr3 = 0,
		/* 用户态现场先填成数据段选择子，实际值由
		 * move_to_user_mode 的 iret 现场取代 */
		.ss = 0x17, .ds = 0x17, .es = 0x17,
		.fs = 0x17, .gs = 0x17,
		.ldt = _LDT(0),		/* 本进程 LDT 的 GDT 选择子 */
		.trace_bitmap = 0x80000000,
	},
};

/*
 * 启动栈：head.S 的 lss stack_start 用，只服务"从 head.S 到这里"的
 * 引导阶段；之后任务 0 的内核栈是 init_task 页内的那个
 */

/* 任务指针表：task[0] 永远是 INIT 进程（书中的 INITPROC） */
struct task_struct *task[NR_TASKS] = { &(init_task.task), };

/* 当前进程指针：从此内核的一切"当前"都看它（官方同款） */
struct task_struct *current = &(init_task.task);

static unsigned long user_stack[PAGE_SIZE >> 2];

/* lss 的操作数：4 字节栈顶 + 2 字节栈段选择子（0x10 内核数据段） */
struct {
	unsigned long esp;
	unsigned short ss;
} stack_start = { user_stack + (PAGE_SIZE >> 2), 0x10 };

/* head.S 的 GDT/IDT（.globl 导出） */
extern unsigned long gdt[];

/*
 * 写一个 TSS/LDT 描述符到 GDT 的第 entry 项。
 * 注意：GDT 每项 8 字节，而 gdt 数组的元素是 4 字节的 unsigned long，
 * 所以数组下标要乘 2 —— 传"第几项"，函数内部换算，避免调用方踩坑
 * （这里曾踩过：gdt+5 按指针运算只前进了 20 字节 = 第 2.5 项）
 * 描述符布局（8 字节）：limit(0-15) | base低16(16-31) | base中8 | 类型 |
 *                     0x00 | base高8
 * 类型：0x89 = 有效的 386 TSS；0x82 = LDT
 */
static void set_tssldt_desc(int entry, void *addr, int type)
{
	unsigned long a = (unsigned long)addr;
	unsigned long *d = gdt + 2 * entry;	/* 第 entry 项 = 下标 entry*2 */

	d[0] = ((a & 0xffff) << 16) | 104;	/* limit 固定 104 字节（386 TSS 大小） */
	d[1] = (a & 0xff000000) | ((a >> 16) & 0xff) | (type << 8);
}

/*
 * sched_init —— 进程子系统的初始化
 * 1. 把任务 0 的 TSS/LDT 描述符写进 GDT 的第 5/6 项
 * 2. 其余任务的 GDT 项清零（每个任务占两项：TSS + LDT）
 * 3. ltr/lldt 加载任务 0 的 TSS/LDT——CPU 从此"知道"当前任务是谁
 */
void sched_init(void)
{
	int i;
	unsigned long *p;

	set_tssldt_desc(5, &(init_task.task.tss), 0x89);
	set_tssldt_desc(6, &(init_task.task.ldt), 0x82);

	/* 第 7 项起是各任务的 TSS/LDT 对（每任务 2 项 = 4 个 long），全部清零 */
	p = gdt + 2 * 7;
	for (i = 1; i < NR_TASKS; i++) {
		task[i] = 0;
		*p++ = 0;
		*p++ = 0;
		*p++ = 0;
		*p++ = 0;
	}

	/* ltr/lldt 的操作数是选择子：任务0 TSS = 5<<3 = 0x28，LDT = 6<<3 = 0x30 */
	__asm__ volatile("ltr  %%ax" :: "a"(_TSS(0)));
	__asm__ volatile("lldt %%ax" :: "a"(_LDT(0)));
}

/*
 * move_to_user_mode —— 全书到目前为止最重要的一次"状态切换"
 *
 * 原理：CPU 只有在中断/异常返回（iret）和任务切换时才改变 CPL。
 * 想主动降特权级，就伪造一个"从用户态进内核态"的栈现场再 iret：
 *
 *   pushl $0x17   ← SS：用户数据段选择子（6<<3=0x30 的 LDT? 不，
 *                    是 LDT 表内第 2 项：2<<3|4|3 = 0x17，TI=1 表示查 LDT）
 *   pushl %esp    ← ESP：沿用当前栈（任务 0 特殊，先借内核栈一用）
 *   pushfl        ← EFLAGS：把当前的 IF 等标志原样带过去
 *   pushl $0x0f   ← CS：用户代码段（LDT 第 1 项：1<<3|4|3 = 0x0F）
 *   pushl $1f     ← EIP：标签 1 处，iret 后从这里继续
 *   iret          ← 弹现场，发现 CS.RPL=3，切到 CPL=3，栈也换成刚压的 SS:ESP
 *
 * 1f 之后：数据段寄存器还在用内核段，逐个换成用户数据段 0x17
 * （CS 已经是 0x0F，不能直接 mov，只能由远跳转/中断返回改变）
 */
void move_to_user_mode(void)
{
	__asm__ volatile(
		"movl %%esp, %%eax\n\t"		/* 保存当前栈指针 */
		"pushl $0x17\n\t"		/* SS = 用户数据段 */
		"pushl %%eax\n\t"		/* ESP */
		"pushfl\n\t"			/* EFLAGS */
		"pushl $0x0f\n\t"		/* CS = 用户代码段，RPL=3 */
		"pushl $1f\n\t"			/* EIP = 下面的标签 1 */
		"iret\n"			/* 返回"现场"→ 进入用户态 */
		"1:\tmovl $0x17, %%eax\n\t"
		"movw %%ax, %%ds\n\t"		/* 数据段全部换成用户段 */
		"movw %%ax, %%es\n\t"
		"movw %%ax, %%fs\n\t"
		"movw %%ax, %%gs"
		::: "ax", "memory");
}
