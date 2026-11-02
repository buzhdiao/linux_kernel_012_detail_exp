/*
 * sched.h —— 进程控制块（task_struct）与 TSS/LDT 的定义
 * 对应书中 4.1.1 "创建进程控制块"
 *
 * 来源：官方仓 include/linux/sched.h（教学裁剪版）
 * 裁剪说明：去掉了信号处理（sigaction[32]）、文件系统字段（pwd/root/filp 等）、
 * 用户/组 ID、进程族谱指针——它们分别属于第 8 章、第 7 章和进程管理完善阶段，
 * 到对应章节的实验里再加回来。字段顺序保持与官方一致，方便对照。
 */

#ifndef _SCHED_H
#define _SCHED_H

#define HZ 100				/* 时钟中断频率，第 4 章末启用 */
#define NR_TASKS 64			/* 系统最多同时存在的进程数 */

#define TASK_RUNNING          0	/* 就绪/运行中 */
#define TASK_INTERRUPTIBLE    1	/* 可中断睡眠 */
#define TASK_UNINTERRUPTIBLE  2	/* 不可中断睡眠 */
#define TASK_ZOMBIE           3	/* 僵尸（已退出待回收） */
#define TASK_STOPPED          4	/* 被信号停止 */

/*
 * TSS（任务状态段）：CPU 硬件认识的"进程上下文"。
 * 每个 TSS 共 104 字节（0x68），ltr 指令加载它的选择子后，
 * 发生特权级切换（如用户态 int 0x80）时 CPU 自动从这里取 SS0:ESP0；
 * 任务切换时（ljmp 到 TSS 选择子）CPU 把整套寄存器存老取新
 */
struct tss_struct {
	long back_link;		/* 前一个任务的 TSS 反向链接 */
	long esp0;		/* 0 特权级（内核态）栈指针 */
	long ss0;		/* 0 特权级栈段选择子 */
	long esp1;		/* 1 特权级栈（本内核不用） */
	long ss1;
	long esp2;		/* 2 特权级栈（本内核不用） */
	long ss2;
	long cr3;		/* 页目录基址——每个进程独立地址空间的基础 */
	long eip;		/* 以下为通用寄存器现场 */
	long eflags;
	long eax, ecx, edx, ebx;
	long esp;		/* 3 特权级（用户态）栈 */
	long ebp;
	long esi;
	long edi;
	long es;		/* 段寄存器现场 */
	long cs;
	long ss;
	long ds;
	long fs;
	long gs;
	long ldt;		/* 本进程的 LDT 选择子 */
	long trace_bitmap;	/* IO 允许位图偏移（0x8000 表示从 TSS 末尾开始） */
};

/* 段描述符就两个 32 位，用 a（低）、b（高）访问 */
struct desc_struct {
	unsigned long a, b;
};

/*
 * task_struct —— 进程控制块（PCB），内核认识一个进程的全部信息
 */
struct task_struct {
/* 这些是调度相关的硬字段 */
	long state;		/* -1 不可运行, 0 就绪, >0 停止 */
	long counter;		/* 时间片，调度依据 */
	long priority;		/* 优先级（时间片初值） */
	long signal;		/* 信号位图（第 8 章） */
	long blocked;		/* 被屏蔽的信号位图 */
/* 进程布局信息 */
	int exit_code;
	unsigned long start_code;	/* 代码起始线性地址 */
	unsigned long end_code;
	unsigned long end_data;
	unsigned long brk;		/* 堆顶 */
	unsigned long start_stack;	/* 栈起始 */
/* 进程标识 */
	long pid;
	long pgrp;
	long session;
	long leader;
/* 本进程的 LDT：[0]=空 [1]=用户代码段 [2]=用户数据段
 * 每个进程的段基址不同（nr * 64MB），实现线性地址空间隔离 */
	struct desc_struct ldt[3];
/* 本进程的 TSS */
	struct tss_struct tss;
};

/*
 * 任务 0（INIT 进程）的初始值。
 * ldt 两个用户段的值：
 *   {0x9f, 0xc0fa00} → 基址 0、限长 0x9f 页(640KB)、可执行、DPL=3
 *   {0x9f, 0xc0f200} → 同上但可读写（数据/栈）
 * tss 关键值：
 *   esp0 = PAGE_SIZE + &init_task：任务 0 的内核栈在自己 4KB 页的顶端
 *   ss0  = 0x10 内核数据段；cr3 = &pg_dir（0x0，恒等映射的页目录）
 *   后面 6 个 0x17：用户态各段先填成数据段选择子
 *   ldt  = _LDT(0)：任务 0 的 LDT 在 GDT 里的描述符选择子
 */
#define _LDT(n) ((n << 4) + 0x30)	/* FIRST_LDT_ENTRY=6 → 6<<3=0x30，进程间步进 16 */
#define _TSS(n) ((n << 4) + 0x28)	/* FIRST_TSS_ENTRY=5 → 5<<3=0x28 */

extern struct task_struct *task[NR_TASKS];
extern struct task_struct *current;

/*
 * 任务 0 的"家"：task_struct 和它自己的内核栈共用一个 4KB 页，
 * 结构体在页的低处，栈从页顶向下长（tss.esp0 = &stack[PAGE_SIZE]）
 */
union task_union {
	struct task_struct task;
	char stack[4096];
};

extern union task_union init_task;

/* 定时器：100Hz，LATCH = 1193180 / 100 为 8254 通道 0 的计数初值 */
#define LATCH (1193180 / HZ)

extern long volatile jiffies;

/* sched.c */
void sched_init(void);
void schedule(void);
void do_timer(void);

/*
 * switch_to —— 切换到第 n 号进程（实现见 sched.c）
 * ljmp 到目标 TSS 的选择子触发硬件任务切换：CPU 把当前全部寄存器
 * 存进当前 TSS，再从目标 TSS 装载全套现场（含 CR3/LDT/EIP）
 */
void switch_to(int n);

/* sched.c */

/*
 * move_to_user_mode —— 从内核态（CPL=0）切到用户态（CPL=3）
 * 手法：往栈里伪造一份"中断返回现场"（SS=0x17 用户栈段、ESP、EFLAGS、
 * CS=0x0F 用户代码段、EIP=标签 1），然后 iret。
 * CPU 执行 iret 时发现目标 CS 的 RPL=3，自动切换到用户态，
 * 并从 TSS 取 SS0:ESP0 备份……本实验里逐行注释见 sched.c
 */
void move_to_user_mode(void);

#endif
