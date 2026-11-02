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
 * 任务 1：本实验手工构造的第二个进程（书中 4.2 的"准备进程控制块"）。
 * 它和任务 0 共用同一套用户段描述符（基址 0，恒等映射下都指向内核镜像
 * 里的代码），用户栈放在 0x70000 起的空闲物理内存（向低地址生长）。
 * tss.eip 直接指向它的用户态入口 user_task1（main.c）。
 * 它的内核栈在自己的 4KB 页里（task1_union）。
 */
union task_union task1_union = {
	.task.state   = TASK_RUNNING,
	.task.counter = 15,
	.task.priority = 15,
	.task.pid     = 1,
	.task.ldt = {
		{ 0, 0 },
		{ 0x9f, 0xc0fa00 },	/* 与任务 0 相同的用户代码段 */
		{ 0x9f, 0xc0f200 },	/* 与任务 0 相同的用户数据段 */
	},
	.task.tss = {
		.esp0 = (long)(task1_union.stack + PAGE_SIZE),
		.ss0 = 0x10,
		.cr3 = 0,			/* 恒等映射，同一张页目录 */
		.eflags = 0x202,		/* IF=1：任务切换进来就能响应时钟中断 */
		.eip = 0,			/* sched_init 里填 user_task1 的地址 */
		.cs = 0x0f,			/* 用户代码段（LDT[1]，RPL=3） */
		.esp = 0x70000,			/* 用户栈顶 */
		.ss = 0x17,
		.ds = 0x17, .es = 0x17, .fs = 0x17, .gs = 0x17,
		.ldt = _LDT(1),
		.trace_bitmap = 0x80000000,
	},
};

/*
 * sched_init —— 进程子系统的初始化
 * 1. 把任务 0 的 TSS/LDT 描述符写进 GDT 的第 5/6 项
 * 2. 把任务 1 的写进第 7/8 项（本实验手工创建，4.3 起 fork 负责）
 * 3. 其余任务的 GDT 项清零
 * 4. ltr/lldt 加载任务 0 的 TSS/LDT——CPU 从此"知道"当前任务是谁
 * 5. 把 8254 定时器设成 100Hz，为时钟中断驱动的调度做准备
 */
void sched_init(void)
{
	int i;
	unsigned long *p;
	extern void user_task1(void);	/* main.c 里任务 1 的用户态入口 */

	/* 任务 1 的入口在 TSS 里补上（编译期常量拿不到函数地址） */
	task1_union.task.tss.eip = (long)user_task1;

	set_tssldt_desc(5, &(init_task.task.tss), 0x89);
	set_tssldt_desc(6, &(init_task.task.ldt), 0x82);
	set_tssldt_desc(7, &(task1_union.task.tss), 0x89);
	set_tssldt_desc(8, &(task1_union.task.ldt), 0x82);

	/* 第 9 项起（任务 2 及以后）的 TSS/LDT 对清零 */
	task[1] = &(task1_union.task);
	p = gdt + 2 * 9;
	for (i = 2; i < NR_TASKS; i++) {
		task[i] = 0;
		*p++ = 0;
		*p++ = 0;
		*p++ = 0;
		*p++ = 0;
	}

	/* ltr/lldt 的操作数是选择子：任务0 TSS = 5<<3 = 0x28，LDT = 6<<3 = 0x30 */
	__asm__ volatile("ltr  %%ax" :: "a"(_TSS(0)));
	__asm__ volatile("lldt %%ax" :: "a"(_LDT(0)));

	/*
	 * 8254 定时器通道 0：方式 3（方波），初值 LATCH = 1193180/100，
	 * 即每 10ms 一次 IRQ0 → 100Hz 时钟中断
	 */
	outb_p(0x36, 0x43);		/* 控制字：通道0，先低后高，方式3，二进制 */
	outb_p(LATCH & 0xff, 0x40);	/* 初值低字节 */
	outb(LATCH >> 8, 0x40);		/* 初值高字节，写入即开始计数 */
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

/*
 * do_timer —— 时钟中断的 C 侧处理（由 asm.S 的 timer_interrupt 调用）
 * jiffies 累计开机以来的时钟滴答；当前进程时间片减 1，减到 0 触发调度
 * 官方仓的 do_timer 还处理进程运行时间统计，教学版只留调度相关
 */
long volatile jiffies = 0;

void do_timer(void)
{
	jiffies++;
	if (--current->counter <= 0)
		schedule();
}

/*
 * schedule —— 调度器（书中 4.2.4 "实现进程切换"）
 *
 * 本实验实现最简单的轮转调度：从当前进程的下一个位置开始找
 * 就绪（state == TASK_RUNNING）的进程；找不到就让自己继续跑
 * （时间片重新充满）。找到了就 switch_to 过去。
 *
 * 官方仓的 schedule 还有睡眠进程唤醒（counter 随时间回升）等逻辑，
 * 到第 8 章管理进程时再引入
 */
void schedule(void)
{
	int i, cur = 0, next;

	/* 找出当前进程在 task[] 里的下标 */
	for (i = 0; i < NR_TASKS; i++)
		if (task[i] == current) {
			cur = i;
			break;
		}

	/* 从下一个位置开始轮转找就绪进程；转一圈没找到就回自己 */
	next = cur;
	for (i = 1; i < NR_TASKS; i++) {
		next = (cur + i) % NR_TASKS;
		if (task[next] && task[next]->state == TASK_RUNNING)
			break;
	}

	if (task[next] != current)
		switch_to(next);
	else
		current->counter = current->priority;
}

/*
 * switch_to —— 硬件任务切换（书中 4.2.4 "实现进程切换"的核心）
 *
 * ljmp 一个 6 字节的"远指针"（4 字节偏移 + 2 字节段选择子），
 * 当选择子指向 TSS 描述符时，CPU 不做普通跳转，而是执行任务切换：
 *   1. 把当前全部寄存器（含 EFLAGS、段寄存器、CR3）存入当前 TSS
 *   2. 从目标 TSS 装载全套寄存器，标记目标 TSS 为 busy
 *   3. 从目标 TSS 的 EIP 继续执行——新进程"从上次离开的地方"复活
 *
 * 实现细节：先 xchgl 交换 current 指针（软件记账），再 ljmp（硬件切换）。
 * __tmp 结构就是那 6 字节远指针：偏移恒为 0，选择子是目标 TSS
 */
void switch_to(int n)
{
	struct {
		long offset;		/* 恒为 0 */
		unsigned short sel;	/* 目标 TSS 的选择子 */
	} __attribute__((packed)) __tmp;
	long dummy;

	if (task[n] == current || !task[n])
		return;

	__tmp.offset = 0;
	__tmp.sel = _TSS(n);
	__asm__ volatile("xchgl %%ecx, %1\n\t"	/* current <-> 新进程指针 */
			 "ljmp *%0"		/* 硬件任务切换 */
			 :: "m"(__tmp), "m"(current), "c"((long)task[n])
			 : "memory");
	(void)dummy;
}
