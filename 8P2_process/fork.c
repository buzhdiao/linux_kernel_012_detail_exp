/*
 * fork.c —— fork 系统调用的实现
 * 对应书中 4.3 节"定义fork"，来源：官方仓 kernel/fork.c（教学精简版）
 *
 * fork 做三件事：
 *   1. find_empty_process：在 task[] 里找个空位，分配唯一 pid
 *   2. copy_process：复制父进程的 task_struct，改造成子进程的初始状态
 *   3. copy_mem（在 copy_process 内）：LDT 基址改到子进程自己的 64MB
 *      区间，copy_page_tables 把父进程的页映射复制过来（写时复制）
 *
 * 子进程的"复活点"：与父进程完全相同——从 int 0x80 的下一条指令继续，
 * 但 EAX = 0（父进程的 EAX = 子进程 pid）。父子从此各奔东西
 */

#include "kernel.h"
#include "sched.h"

extern int errno;		/* 简化的 errno（负值错误码约定见 sys_fork） */

#define EAGAIN 11

/* sched.c 里的 GDT 描述符写入函数（那里是 static，这里 extern 用） */
extern void set_tssldt_desc(int entry, void *addr, int type);

static long last_pid = 0;

/*
 * find_empty_process —— 找一个空闲的任务槽位
 * task[0] 是 INIT 永不复用；pid 从 1 递增（本实验进程不退出，无需查重）
 * 返回任务号（task[] 下标），失败返回负数
 */
int find_empty_process(void)
{
	int i;

	for (i = 1; i < NR_TASKS; i++)
		if (!task[i])
			break;
	if (i >= NR_TASKS)
		return -EAGAIN;
	return i;
}

/*
 * copy_mem —— 子进程的地址空间：自己的 64MB + 父进程映射的写时复制副本
 * 子进程 nr 的线性区间 = [nr*64MB, (nr+1)*64MB)，
 * LDT 代码/数据段基址都要改成 nr*64MB（描述符里 base 字段分散在
 * 高低两个 long 里，set_desc_base 负责重装）
 */
static void set_desc_base(struct desc_struct *d, unsigned long base)
{
	d->a = (d->a & 0x0000ffff) | ((base & 0xffff) << 16);
	/* b 的 bit0-7 = base16-23，bit24-31 = base24-31；其余位保留 */
	d->b = (d->b & 0x00ffff00) | ((base >> 16) & 0xff) | (base & 0xff000000);
}

static int copy_mem(int nr, struct task_struct *p)
{
	unsigned long new_base = nr * TASK_SIZE;

	/* 改子进程 LDT 两个用户段的基址（段限长/属性原样继承） */
	set_desc_base(&p->ldt[1], new_base);
	set_desc_base(&p->ldt[2], new_base);
	p->start_code = new_base;

	/*
	 * 复制页表：把父进程（task0，线性 0 起）的映射复制到子进程的
	 * 64MB 区间，全部置只读（写时复制）。限长 640KB 与用户段限长一致
	 */
	return copy_page_tables(0, new_base, 640 * 1024);
}

/*
 * copy_process —— 把父进程的现场改造成子进程
 * nr    任务号（find_empty_process 的结果）
 * regs  父进程 int 0x80 时压在内核栈上的完整现场（asm.S 组织好）
 *
 * 子进程 TSS 的关键差异：
 *   eax = 0        —— fork 在子进程里返回 0
 *   eip 不变        —— 和父进程一样从 int 0x80 后继续
 *   esp 不变        —— 用户栈指针相同（物理页通过 COW 共享/分化）
 *   内核栈换新       —— esp0 指向子进程自己的 task_union 页顶
 */
int copy_process(int nr, struct pt_regs *regs)
{
	struct task_struct *p;
	unsigned long page;

	page = get_free_page();		/* 子进程的 task_union 页 */
	if (!page)
		return -EAGAIN;
	p = (struct task_struct *)page;
	task[nr] = p;

	*p = *current;			/* 整体复制：调度字段、ldt、tss 全拿来 */
	p->pid = ++last_pid;
	p->counter = p->priority = 15;	/* 时间片重新充满 */
	p->state = TASK_UNINTERRUPTIBLE;	/* 收尾前先不让调度器看见 */

	/* ---- 内核栈：子进程自己的页顶 ---- */
	p->tss.back_link = 0;
	p->tss.esp0 = page + 4096;
	p->tss.ss0 = 0x10;

	/* ---- 用户态现场：来自父进程的 pt_regs ---- */
	p->tss.eip = regs->eip;		/* 从 int 0x80 的下一条指令继续 */
	p->tss.eflags = regs->eflags;
	p->tss.eax = 0;			/* ★ fork 在子进程返回 0 */
	p->tss.ecx = regs->ecx;
	p->tss.edx = regs->edx;
	p->tss.ebx = regs->ebx;
	p->tss.esp = regs->esp;		/* 用户栈指针（int 0x80 硬件压栈的） */
	p->tss.ebp = regs->ebp;
	p->tss.esi = regs->esi;
	p->tss.edi = regs->edi;
	p->tss.es = regs->es & 0xffff;
	p->tss.cs = regs->cs & 0xffff;
	/* ★ esp/ss 只有"用户态 → 内核态"的 int 0x80 才由 CPU 压栈
	 * （特权级不变时硬件只压 eip/cs/eflags，pt_regs 末尾两项不存在，
	 * 读到的是栈上残留垃圾——曾踩坑：task0 内核态直接 fork，子进程
	 * TSS.ss 被填成垃圾 0x0f00，首次切换进子任务即 #TS）。
	 * 内核态 fork 的第二个坑：子进程【不能】直接用父栈——它一跑
	 * （printk 等调用链帧）就会把父进程 int 0x80 / 时钟中断的现场
	 * 帧踩掉，父进程被换回时 iret 到垃圾地址（EIP=0x40）→ #GP 风暴。
	 * 正确做法：把父栈"当前栈顶以上"的存活调用链整段拷到子进程
	 * 自己的栈页顶部，子进程在副本上继续（帧内的返回地址指向内核
	 * 代码，两份都有效） */
	if ((regs->cs & 0xffff) == 0x08) {
		extern unsigned long stack_start;	/* 父栈顶（sched.c） */
		unsigned long top = stack_start;
		unsigned long src = (unsigned long)regs + 14 * 4; /* int 0x80 时的父 esp */
		unsigned long bytes = top - src;
		unsigned long *d, *s, n;

		p->tss.ss = 0x10;
		p->tss.esp = p->tss.esp0 - bytes;
		d = (unsigned long *)p->tss.esp;
		s = (unsigned long *)src;
		n = (bytes + 3) / 4;
		while (n--)
			*d++ = *s++;
	} else {
		p->tss.esp = regs->esp;
		p->tss.ss = regs->ss & 0xffff;
	}
	p->tss.ds = regs->ds & 0xffff;
	p->tss.fs = regs->fs & 0xffff;
	p->tss.gs = regs->gs & 0xffff;
	p->tss.ldt = _LDT(nr);		/* 指向自己的 LDT（GDT 第 2nr+6 项） */
	p->tss.trace_bitmap = 0x80000000;

	/* ---- 地址空间：自己的 64MB 区间 + 写时复制的父进程映射 ---- */
	if (copy_mem(nr, p)) {
		task[nr] = 0;
		free_page(page);
		return -EAGAIN;
	}

	/* ---- 挂上 GDT：TSS 第 2nr+5 项、LDT 第 2nr+6 项 ---- */
	set_tssldt_desc(2 * nr + 5, &(p->tss), 0x89);
	set_tssldt_desc(2 * nr + 6, &(p->ldt), 0x82);

	p->state = TASK_RUNNING;	/* 就绪：下次调度就可能跑 */
	return p->pid;
}

/*
 * sys_fork —— 系统调用 nr=2 的入口（由 sys_dispatch 调）
 * 成功：父进程拿到子进程 pid（写回父进程 pt_regs 的 eax），
 *       子进程在 copy_process 里已拿 0
 */
int sys_fork(struct pt_regs *regs)
{
	int nr;

	nr = find_empty_process();
	if (nr < 0)
		return -EAGAIN;
	return copy_process(nr, regs);
}
