/*
 * memory.c —— 页式内存管理（fork 与写时复制的地基）
 * 对应书中 4.3 节"写时复制"部分，来源：官方仓 mm/memory.c（教学精简版）
 *
 * 管理模型（0.11 经典设计）：
 *   - 线性空间：每任务 64MB（TASK_SIZE），64 个任务铺满 4GB；
 *     全部任务共用一张页目录（CR3 = 0），任务 n 独占目录项 16n ~ 16n+15
 *   - 物理页池：1MB ~ 16MB，共 3840 页，由 mem_map[] 按引用计数管理：
 *       0 = 空闲，1 = 独占使用，n > 1 = 被 n 个映射共享
 *   - 写时复制（COW）：fork 复制页表时把父子双方的页表项都改成只读并
 *       计数 +1；任一方写时触发 14 号缺页异常 → do_wp_page 复制私有副本
 *
 * 一个关键前提：CR0.WP = 0（head.S 开分页时只置了 PG 位）。
 * x86 规定 WP=0 时"内核态写不受只读位约束"——于是内核写自己的
 * 数据页不会因被标成只读而缺页，只有用户态（CPL3）的写会触发 COW。
 * 这正是 0.11 的 task0 用户空间与内核空间重叠（都是线性 0 起）却安然无恙的原因
 */

#include "kernel.h"
#include "sched.h"

#define LOW_MEM      0x100000			/* 1MB 以下的内存不归页管理器管 */
#define PAGING_PAGES ((16 * 1024 * 1024 - LOW_MEM) >> 12)	/* 3840 页 */
#define MAP_NR(addr) (((addr) - LOW_MEM) >> 12)
#define PAGE_DIRTY   0x40

unsigned long HIGH_MEMORY = 16 * 1024 * 1024;	/* 物理内存上限 */

/* 引用计数表：下标 = 物理页号（从 1MB 起算） */
unsigned char mem_map[PAGING_PAGES] = { 0, };

/* 复制一整页（4KB = 1024 个长字）：ESI 源，EDI 目的 */
#define copy_page(from, to) \
	__asm__ volatile("cld ; rep ; movsl" \
		:: "S"(from), "D"(to), "c"(1024) : "memory")

/* 刷新 TLB：重写 CR3（页表改完必须做，否则 CPU 可能用旧映射） */
#define invalidate() \
	__asm__ volatile("movl %%cr3, %%eax\n\tmovl %%eax, %%cr3" :: : "ax")

/*
 * get_free_page —— 申请一个物理页
 * 从池尾向池头扫描 mem_map 找 0（空闲），置 1（独占），
 * 并把整页清零（页表/用户数据都要求零页）。返回页的物理地址，0 = 失败
 */
unsigned long get_free_page(void)
{
	int i;

	for (i = PAGING_PAGES - 1; i >= 0; i--) {
		if (mem_map[i] == 0) {
			unsigned long addr = LOW_MEM + ((unsigned long)i << 12);
			unsigned long *p = (unsigned long *)addr;
			int n;

			mem_map[i] = 1;
			for (n = 0; n < 1024; n++)
				p[n] = 0;
			return addr;
		}
	}
	printk("out of memory\n\r");
	return 0;
}

/*
 * free_page —— 释放物理页：引用计数减 1，减到 0 即空闲
 */
void free_page(unsigned long addr)
{
	if (addr < LOW_MEM)
		return;
	if (addr >= HIGH_MEMORY) {
		printk("trying to free nonexistent page\n\r");
		return;
	}
	addr -= LOW_MEM;
	addr >>= 12;
	if (mem_map[addr]--)
		return;
	mem_map[addr] = 0;		/* 计数下溢说明出了 bug，纠正为空闲 */
	printk("trying to free free page\n\r");
}

/*
 * free_page_tables —— 释放一段线性空间（4MB 对齐）的全部页表和页
 * from  线性地址（必须 4MB 对齐，即页目录项边界）
 * size  字节数（向上取整成若干个 4MB 区间）
 */
int free_page_tables(unsigned long from, unsigned long size)
{
	unsigned long *pg_table;
	unsigned long *dir, nr;

	if (from & 0x3fffff) {
		printk("free_page_tables: wrong alignment\n\r");
		return -1;
	}
	if (!from) {			/* 内核自己的映射区不许释放 */
		printk("trying to free swapper memory\n\r");
		return -1;
	}
	size = (size + 0x3fffff) >> 22;		/* 涉及几个目录项 */
	dir = (unsigned long *)((from >> 20) & 0xffc);	/* 目录项地址 = 线性高10位*4 */

	for ( ; size-- > 0; dir++) {
		if (!(1 & *dir))		/* 目录项为空，本 4MB 无映射 */
			continue;
		pg_table = (unsigned long *)(0xfffff000 & *dir);
		for (nr = 0; nr < 1024; nr++) {	/* 逐页释放 */
			if (*pg_table) {
				if (1 & *pg_table)
					free_page(0xfffff000 & *pg_table);
				*pg_table = 0;
			}
			pg_table++;
		}
		free_page(0xfffff000 & *dir);	/* 页表本身也是一页 */
		*dir = 0;
	}
	invalidate();
	return 0;
}

/*
 * copy_page_tables —— fork 的核心：复制一段线性空间的页映射（写时复制）
 * from/to  源/目的线性地址（4MB 对齐）
 * size     字节数
 *
 * 逐目录项处理：为每个非空的源页表分配一张新页表，把有效表项逐个复制；
 * 复制时把"读写位"清掉（this_page &= ~2）——源表项也一并改成只读，
 * 从此父子对这个页的任何用户态写都会缺页，进入 do_wp_page 分身
 * 共享页的引用计数 +1（mem_map++）。
 *
 * from==0（task0 的空间）有个特殊约定：只复制页表前 0xA0 项 = 640KB，
 * 即 task0 用户段的实际限长，避免把整个 4MB 都复制一遍
 */
int copy_page_tables(unsigned long from, unsigned long to, long size)
{
	unsigned long *from_page_table;
	unsigned long *to_page_table;
	unsigned long this_page;
	unsigned long *from_dir, *to_dir;
	unsigned long nr;

	if ((from & 0x3fffff) || (to & 0x3fffff)) {
		printk("copy_page_tables: wrong alignment\n\r");
		return -1;
	}
	from_dir = (unsigned long *)((from >> 20) & 0xffc);
	to_dir = (unsigned long *)((to >> 20) & 0xffc);
	size = ((unsigned long)(size + 0x3fffff)) >> 22;

	for ( ; size-- > 0; from_dir++, to_dir++) {
		if (1 & *to_dir) {		/* 目标区间已有映射，拒绝 */
			printk("copy_page_tables: already exist\n\r");
			return -1;
		}
		if (!(1 & *from_dir))		/* 源区间没映射，跳过 */
			continue;

		from_page_table = (unsigned long *)(0xfffff000 & *from_dir);
		if (!(to_page_table = (unsigned long *)get_free_page()))
			return -1;		/* 没内存分配新页表 */

		*to_dir = (unsigned long)to_page_table | 7;	/* 存在+用户+可写 */
		nr = (from == 0) ? 0xA0 : 1024;	/* task0 只需前 640KB */

		for ( ; nr-- > 0; from_page_table++, to_page_table++) {
			this_page = *from_page_table;
			if (!(this_page & 1))	/* 表项无效（未映射） */
				continue;

			this_page &= ~2;	/* 关键：改为只读（写时复制） */
			*to_page_table = this_page;

			if (this_page > LOW_MEM) {
				*from_page_table = this_page;	/* 源也改只读 */
				this_page -= LOW_MEM;
				this_page >>= 12;
				mem_map[this_page]++;		/* 共享计数 +1 */
			}
			/* 1MB 以下的页（内核镜像/BIOS 区）不参与计数，
			 * 内核态写它们也不缺页（CR0.WP=0），见文件头注释 */
		}
	}
	invalidate();
	return 0;
}

/*
 * un_wp_page —— 写时复制的执行者
 * table_entry 指向那个"只读且被共享"的页表项
 * 两种情况：
 *   计数 == 1（只剩自己）：说明另一方的映射已解除，直接恢复读写位即可
 *   计数 > 1：分配新页，复制内容，旧页计数 -1，表项指向新页（读写）
 */
void un_wp_page(unsigned long *table_entry)
{
	unsigned long old_page, new_page;

	old_page = 0xfffff000 & *table_entry;

	if (old_page >= LOW_MEM && mem_map[MAP_NR(old_page)] == 1) {
		*table_entry |= 2;		/* 独占：恢复读写位即可 */
		invalidate();
		return;
	}

	new_page = get_free_page();
	if (old_page >= LOW_MEM)
		mem_map[MAP_NR(old_page)]--;	/* 旧页少了一个共享者 */
	copy_page(old_page, new_page);		/* 复制 4KB 内容 */
	*table_entry = new_page | 7;		/* 指向私有副本（读写） */
	invalidate();
}

/*
 * do_wp_page —— 14 号缺页异常（写保护）的 C 处理函数
 * address 是 CR2 里的故障线性地址。从页目录/页表两级索引出表项地址：
 *   目录项 = 页目录基址 + (address >> 20) & 0xffc
 *   表项   = 目录项里的页表基址 + (address >> 10) & 0xffc
 *
 * 与官方的差异：官方对 address < TASK_SIZE 直接 panic（内核映射被写
 * 保护报错），但本内核允许 task0 的用户代码写自己的页——它的线性
 * 地址 < 64MB，走同样的 COW 逻辑即可（内核态写不受影响，CR0.WP=0）
 */
void do_wp_page(long error_code, long address)
{
	un_wp_page((unsigned long *)
		(((address >> 10) & 0xffc) +
		 (0xfffff000 & *((unsigned long *)((address >> 20) & 0xffc)))));
}

/*
 * do_no_page —— 访问了没有映射的线性地址（表项不存在）
 * 本实验还没实现按需调页（exec 才需要，见 7P6），先报错停机
 */
void do_no_page(long error_code, long address)
{
	printk("page not present at 0x%08lx (error 0x%lx)\n\r", address, error_code);
	printk("do_no_page (demand paging) comes with exec in 7P6, halted\n\r");
	__asm__ volatile("cli; hlt");
	for (;;)
		;
}

/*
 * mem_init —— 标记 [start, end) 为可用主存，其余（内核/缓冲区）为已用
 */
long mem_init(long start, long end)
{
	long i;
	long free = 0;

	for (i = 0; i < PAGING_PAGES; i++)
		mem_map[i] = 1;			/* 先全部视为已用 */

	i = (start - LOW_MEM) >> 12;		/* 起始页号 */
	for (; i < (end >> 12) && i < PAGING_PAGES; i++) {
		mem_map[i] = 0;			/* 主存区间标成空闲 */
		free++;
	}
	return free;
}

long count_free_pages(void)
{
	long i, free = 0;

	for (i = 0; i < PAGING_PAGES; i++)
		if (mem_map[i] == 0)
			free++;
	return free;
}
