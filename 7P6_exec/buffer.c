/*
 * buffer.c —— 高速缓冲区管理
 * 对应书中 6.2 节"管理缓冲区"（初始化 / 申请 / 读写缓冲区）
 * 来源：官方仓 fs/buffer.c 的教学精简版
 *
 * 为什么要有缓冲区：CPU 访问内存快、访问磁盘慢。读盘前先查缓存
 * （命中就不用读盘），写盘先写缓存（攒着批量刷）。Linux 0.11 的
 * 整个文件系统都建立在缓冲区之上。
 *
 * 数据结构（0.11 经典设计）：
 *   - buffer_head：每个 1KB 缓冲块的元信息（设备号/块号/状态/两条链）
 *   - 哈希链：dev+blocknr → 快速查找"这个块在不在缓存"
 *   - 空闲链：双向链环，getblk 从末尾取（最近未用的），brelse 放回头部
 *     （近似 LRU）
 */

#include "kernel.h"

#define NR_BUFFERS 32		/* 教学规模：32 个 1KB 缓冲块 */
#define NR_HASH    17		/* 哈希桶数（取素数） */

struct buffer_head {
	char *b_data;			/* 指向 1KB 数据区 */
	unsigned long b_blocknr;	/* 逻辑块号 */
	unsigned short b_dev;		/* 设备号（0x300 = 主盘） */
	unsigned char b_uptodate;	/* 数据有效（与磁盘一致） */
	unsigned char b_dirt;		/* 脏：改过未写回 */
	unsigned char b_count;		/* 引用计数 */
	struct buffer_head *b_next;	/* 哈希链 */
	struct buffer_head *b_prev_free, *b_next_free;	/* 空闲双向链 */
};

static struct buffer_head buffer_heads[NR_BUFFERS];
static struct buffer_head *hash_table[NR_HASH];
static struct buffer_head *free_list;	/* 空闲链环的头 */

/* 块 → 哈希桶（简单异或散列） */
static inline int hash(int dev, unsigned long block)
{
	return (dev ^ block) % NR_HASH;
}

/* 从哈希链找：dev+block 是否已在缓存 */
static struct buffer_head *find_buffer(int dev, unsigned long block)
{
	struct buffer_head *h = hash_table[hash(dev, block)];

	while (h) {
		if (h->b_dev == dev && h->b_blocknr == block)
			return h;
		h = h->b_next;
	}
	return 0;
}

/* 摘链/挂链：buffer 进出哈希表 */
static void remove_from_hash(struct buffer_head *bh)
{
	struct buffer_head **p = &hash_table[hash(bh->b_dev, bh->b_blocknr)];

	while (*p) {
		if (*p == bh) {
			*p = bh->b_next;
			return;
		}
		p = &(*p)->b_next;
	}
}

static void add_to_hash(struct buffer_head *bh)
{
	int i = hash(bh->b_dev, bh->b_blocknr);

	bh->b_next = hash_table[i];
	hash_table[i] = bh;
}

/* buffer_init —— 从物理页池切出缓冲块，建两条链 */
void buffer_init(void)
{
	static unsigned long pages[NR_BUFFERS / 4];
	int i;

	for (i = 0; i < NR_BUFFERS; i++) {
		struct buffer_head *bh = &buffer_heads[i];

		if (i % 4 == 0) {	/* 每 4 个 1KB 缓冲块共享一页 */
			pages[i / 4] = get_free_page();
			if (!pages[i / 4]) {
				printk("[buffer] out of pages at %d\n\r", i);
				return;
			}
		}
		bh->b_data = (char *)(pages[i / 4] + (i % 4) * 1024);
		bh->b_dev = 0;
		bh->b_blocknr = 0;
		bh->b_count = 0;
		bh->b_uptodate = 0;
		bh->b_dirt = 0;
		bh->b_next = 0;

		/* 串成空闲双向链环 */
		bh->b_next_free = free_list;
		bh->b_prev_free = free_list ? free_list->b_prev_free : bh;
		if (free_list) {
			free_list->b_prev_free->b_next_free = bh;
			free_list->b_prev_free = bh;
		} else {
			bh->b_next_free = bh;	/* 第一个：自环 */
			bh->b_prev_free = bh;
			free_list = bh;
		}
	}
	printk("[buffer] %d buffers initialized, free chain OK\n\r", NR_BUFFERS);
}

/*
 * getblk —— 申请一个 dev/block 对应的缓冲块
 * 命中：直接返回（b_count++）；未命中：空闲链尾取一个（最近最少用），
 * 换绑到新的 dev/block，调用方负责填数据（bread 里读盘）
 */
static struct buffer_head *getblk(int dev, unsigned long block)
{
	struct buffer_head *bh = find_buffer(dev, block);
	struct buffer_head *tail;

	if (bh) {
		bh->b_count++;		/* 缓存命中 */
		return bh;
	}

	/* 未命中：从空闲链尾取（离"最近使用"最远的一个） */
	if (!free_list) {
		printk("getblk: no free buffer\n\r");
		return 0;
	}
	tail = free_list->b_prev_free;
	if (tail->b_count) {		/* 都被引用（教学规模下不发生） */
		printk("getblk: no free buffer (all busy)\n\r");
		return 0;
	}
	if (tail->b_blocknr)
		remove_from_hash(tail);	/* 它原来顶替过别的块，摘下来 */

	tail->b_dev = dev;
	tail->b_blocknr = block;
	tail->b_count = 1;
	tail->b_uptodate = 0;
	tail->b_dirt = 0;
	add_to_hash(tail);

	/* LRU 触摸：把它挪到链头（下次从尾部取时不会先轮到它） */
	if (tail != free_list) {
		/* 摘出 tail */
		tail->b_prev_free->b_next_free = tail->b_next_free;
		tail->b_next_free->b_prev_free = tail->b_prev_free;
		/* 插到 free_list 前面（即链尾的"头"侧） */
		tail->b_next_free = free_list;
		tail->b_prev_free = free_list->b_prev_free;
		free_list->b_prev_free->b_next_free = tail;
		free_list->b_prev_free = tail;
	}
	return tail;
}

/* brelse —— 用完释放：引用计数减 1，归位空闲链头（最近使用） */
static void brelse(struct buffer_head *bh)
{
	if (!bh || !bh->b_count)
		return;
	if (--bh->b_count)
		return;
	/* 已无人引用：把它插到空闲链头（free_list 指向它） */
	if (free_list != bh) {
		bh->b_prev_free->b_next_free = bh->b_next_free;
		bh->b_next_free->b_prev_free = bh->b_prev_free;
		bh->b_next_free = free_list;
		bh->b_prev_free = free_list->b_prev_free;
		free_list->b_prev_free->b_next_free = bh;
		free_list->b_prev_free = bh;
		free_list = bh;
	}
}

/*
 * bread —— 读一个块（缓存命中免读盘；否则读盘填缓存）
 * 返回 buffer_head（调用方用 b_data 访问数据），用完 brelse
 */
struct buffer_head *bread(int dev, unsigned long block)
{
	struct buffer_head *bh = getblk(dev, block);
	int hit;

	if (!bh)
		return 0;
	hit = bh->b_uptodate;
	if (!bh->b_uptodate) {
		/* 1KB 逻辑块 = 2 个 512B 扇区，各发一个请求（6P3 的队列版） */
		extern int hd_do_request(int, unsigned long, unsigned char *, int);
		hd_do_request(dev, block * 2, (unsigned char *)bh->b_data, 0);
		hd_do_request(dev, block * 2 + 1, (unsigned char *)bh->b_data + 512, 0);
		bh->b_uptodate = 1;
	}
	printk("bread(%d, %d): %s\n\r", dev, block,
	       hit ? "HIT (cache)" : "MISS (disk read)");
	return bh;
}

/* buffer_demo —— 6.2 的验证：同一块读两遍，第二遍应命中 */
void buffer_demo(void)
{
	struct buffer_head *bh;

	bh = bread(0x300, 0);		/* MISS：读盘（MBR） */
	if (bh) {
		printk("  mbr[510..511] = %02x %02x (expect 55 aa)\n\r",
		       (unsigned char)bh->b_data[510],
		       (unsigned char)bh->b_data[511]);
		brelse(bh);
	}
	bh = bread(0x300, 0);		/* HIT：缓存直接返回 */
	if (bh) {
		printk("  data still valid: %02x %02x\n\r",
		       (unsigned char)bh->b_data[510],
		       (unsigned char)bh->b_data[511]);
		brelse(bh);
	}
}
