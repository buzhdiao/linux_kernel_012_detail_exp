/*
 * exec.c —— execve：从文件系统加载程序并执行
 * 对应书中 7.6 节"执行程序"（a.out格式 / 执行execve / 缺页中断 / 实验）
 * 来源：官方仓 fs/exec.c 的教学精简版
 *
 * 格式说明（教学版"flat a.out"）：不做 a.out 头解析，直接把文件的
 * 全部内容当作"链接到线性 0 的纯代码数据镜像"（ld -Ttext=0 --oformat
 * binary 的产物），文件长度即镜像长度，入口 = 镜像偏移 0。
 *
 * do_execve 的做法（新任务版，比"替换当前进程"更直观）：
 *   1. 找空任务槽（find_empty_process 的 nr）
 *   2. 从文件读出程序镜像（file 层）
 *   3. 镜像内容写入新任务的线性空间：每 1KB 分配物理页（get_free_page
 *      + memcpy），put_page 挂进新任务的页目录/页表（64MB*nr 处）
 *   4. 手工构造新任务的 task_struct：LDT 基址 = 64MB*nr、TSS 的
 *      cs/eip/ss/esp 指向用户态（eip=0 即镜像入口）
 *   5. 挂 GDT，state=RUNNING——调度到它时从镜像入口开始执行
 * 这正是 0.11 do_execve 的骨架；官方版还有 a.out 头解析、参数/环境
 * 栈布局、demand paging（do_no_page 按需调页），本版整块加载
 */

#include "kernel.h"
#include "sched.h"
#include "fs.h"

extern int file_open(const char *, int);
extern int file_read(int, char *, int);
extern int file_close(int);

/* ---- 页表操作（来自 4P3 的 mm 层，这里补 exec 需要的部分） ---- */
extern unsigned char mem_map[];
#define LOW_MEM   0x100000
#define MAP_NR(a) (((a) - LOW_MEM) >> 12)

/* put_page —— 把物理页挂到线性地址的页表上（不存在则先建页表） */
static void put_page(unsigned long page, unsigned long address)
{
	unsigned long tmp, *page_table;

	page_table = (unsigned long *)((address >> 20) & 0xffc);
	if (*page_table & 1) {
		page_table = (unsigned long *)(0xfffff000 & *page_table);
	} else {
		tmp = get_free_page();
		if (!tmp)
			return;
		*page_table = tmp | 7;
		page_table = (unsigned long *)tmp;
	}
	page_table[(address >> 12) & 0x3ff] = page | 7;
	/* 刷新 TLB */
	__asm__ volatile("movl %%cr3, %%eax\n\tmovl %%eax, %%cr3" :: : "ax");
}

/* sched.c 的描述符写入（fork.c 里已在用） */
extern void set_tssldt_desc(int entry, void *addr, int type);
extern int find_empty_process(void);

static long exec_last_pid = 0;

/*
 * do_execve —— 加载 path 指向的程序，作为新任务运行
 * 返回新任务号（>=0）；失败负数
 */
int do_execve(const char *path)
{
	struct m_inode *mi;
	int fd, nr, i;
	unsigned long image_size, npages, new_base;
	unsigned char *kbuf;
	struct task_struct *p;
	unsigned long page;

	mi = namei(path);
	if (!mi) {
		printk("execve: %s not found\n\r", path);
		return -1;
	}
	image_size = mi->i_size;

	/* 读镜像到内核临时缓冲（逐块经文件层） */
	kbuf = (unsigned char *)get_free_page();	/* 先拿一页做中转不够——
							 * 教学：镜像小（<4KB），
							 * 用两页循环搬运 */
	{
		/* 简化：用文件层直接读到一系列新分配的物理页，
		 * 不经过大块内核缓冲 */
		unsigned char tmp[1024];
		unsigned long loaded = 0;

		(void)kbuf;
		fd = file_open(path, 1);
		if (fd < 0)
			return -1;

		nr = find_empty_process();
		if (nr < 0)
			return nr;
		exec_last_pid++;
		new_base = nr * 0x04000000;		/* 任务的 64MB 区起点 */

		npages = (image_size + 4095) / 4096;
		p = (struct task_struct *)get_free_page();
		if (!p)
			return -1;
		task[nr] = p;

		/* 每页：分配物理页 → 从文件读 4KB → put_page 映射到
		 * new_base + page*4096 → 页内容已经在物理页里 */
		for (i = 0; (unsigned long)i < npages; i++) {
			unsigned long phy = get_free_page();
			int n, left = image_size - loaded, j;

			if (!phy)
				return -1;
			if (left > 4096)
				left = 4096;
			/* 文件读出到临时页再拷（file_read 每次最多跨 1 块，
			 * 这里逐 1KB 读进物理页对应位置） */
			{
				int got = 0;
				while (got < left) {
					n = file_read(fd, tmp, 1024);
					if (n <= 0)
						break;
					for (j = 0; j < n; j++)
						((unsigned char *)phy)[loaded + got + j] = tmp[j];
					got += n;
				}
			}
			put_page(phy, new_base + i * 4096);
			loaded += left;
		}
		file_close(fd);

		/* ---- 手工构造 task_struct（复制 task0 骨架再改） ---- */
		*p = *current;
		p->state = TASK_UNINTERRUPTIBLE;
		p->pid = ++exec_last_pid;
		p->counter = p->priority = 15;
		p->start_code = new_base;
		p->tss.back_link = 0;
		p->tss.esp0 = (unsigned long)p + 4096;	/* 页顶作内核栈 */
		p->tss.ss0 = 0x10;
		p->tss.cr3 = 0;				/* 共享页目录 */
		p->tss.eip = 0;				/* 镜像入口 = 偏移 0 */
		p->tss.eflags = 0x202;			/* IF=1 */
		p->tss.eax = p->tss.ecx = p->tss.edx = p->tss.ebx = 0;
		p->tss.esp = 0x60000;			/* 用户栈（空闲物理区） */
		/* 给用户栈一个真实物理页（没有它第一次压栈就是缺页，
		 * 本版 do_no_page 未实现按需调页） */
		{
			unsigned long stk = get_free_page();
			if (stk)
				put_page(stk, new_base + 0x60000);
		}
		p->tss.ebp = p->tss.esi = p->tss.edi = 0;
		p->tss.cs = 0x0f;			/* LDT 用户代码段 */
		p->tss.ss = p->tss.ds = p->tss.es = 0x17;
		p->tss.fs = p->tss.gs = 0x17;
		p->tss.ldt = ((nr << 4) + 0x30);	/* _LDT(nr) */
		p->tss.trace_bitmap = 0x80000000;

		/* LDT 两个用户段基址指到自己的 64MB 区 */
		{
			unsigned long base = new_base;
			struct desc_struct *d = p->ldt;
			d[1].a = (d[1].a & 0x0000ffff) | ((base & 0xffff) << 16);
			d[1].b = (d[1].b & 0x00ffff00) | ((base >> 16) & 0xff) | (base & 0xff000000);
			d[2].a = (d[2].a & 0x0000ffff) | ((base & 0xffff) << 16);
			d[2].b = (d[2].b & 0x00ffff00) | ((base >> 16) & 0xff) | (base & 0xff000000);
		}

		set_tssldt_desc(2 * nr + 5, &(p->tss), 0x89);
		set_tssldt_desc(2 * nr + 6, &(p->ldt), 0x82);
		p->state = TASK_RUNNING;

		printk("execve %s: task %d pid %d, %d bytes loaded to 0x%08lx\n\r",
		       path, nr, p->pid, image_size, new_base);
		return nr;
	}
}

/* exec_demo —— 7.6 验证：执行 /hello.exe */
void exec_demo(void)
{
	do_execve("/hello.exe");
}
