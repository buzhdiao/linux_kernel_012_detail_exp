/*
 * memory.c —— 物理内存管理初始化（mem_map 位图）
 * 对应书中 3.3.1 "初始化内存管理"
 *
 * 思路与官方仓 mm/memory.c 的 mem_init 一致：
 *   把物理内存按 4KB 一页划分，用一个字节数组 mem_map 管理每一页：
 *     0  —— 已使用（内核自己占着，不许再分）
 *     1  —— 空闲，可以分配
 *     >1 —— 有多个引用（写时复制/共享页的场景，第 4 章）
 *   之后申请物理页 = 在数组里找值为 1 的项置 0；
 *   释放物理页 = 对应项加回 1
 *
 * 本实验把 mem_map 放在 .bss 的静态数组里（16MB / 4KB = 4096 项），
 * 官方仓是运行时放在内存末尾动态计算的，教学版取其直观
 */

#include "kernel.h"

#define PAGE_SIZE   4096			/* 一页 4KB */
#define MAX_PAGES   (16 * 1024 * 1024 / PAGE_SIZE)	/* 16MB 共 4096 页 */

/* 每一项对应一个物理页的引用计数 */
static unsigned char mem_map[MAX_PAGES];

/*
 * mem_init —— 标记 [start, end) 区间内的页为空闲，其余为已用
 * start  主内存起始（内核镜像 + 缓冲区之后），字节地址，按页取整
 * end    物理内存末尾
 * 返回空闲页数
 */
long mem_init(long start, long end)
{
	long i;
	long free = 0;

	/* 先全部标成"已用"（0） */
	for (i = 0; i < MAX_PAGES; i++)
		mem_map[i] = 0;

	/* [start, end) 区间内的页标记为空闲（1） */
	i = (start + PAGE_SIZE - 1) / PAGE_SIZE;	/* 起始页号（向上取整） */
	for (; i < end / PAGE_SIZE && i < MAX_PAGES; i++) {
		mem_map[i] = 1;
		free++;
	}
	return free;
}

/* 返回空闲页总数（用于在 main 里打印核对） */
long count_free_pages(void)
{
	long i, free = 0;

	for (i = 0; i < MAX_PAGES; i++)
		if (mem_map[i] == 1)
			free++;
	return free;
}
