/*
 * fs.c —— 挂载根文件系统：读超级块 + 根 inode + 列根目录
 * 对应书中 7.1 节"初始化超级块"，7.2 的路径查找雏形在这里一并演示
 * 来源：官方仓 fs/super.c + fs/inode.c 的教学精简版
 *
 * 设备与块的换算：Minix 逻辑块 1KB，软盘扇区 512B——
 *   1 个逻辑块 = 2 个连续物理扇区；dev 0x0200 = 第一个软驱
 */

#include "kernel.h"
#include "sched.h"
#include "fs.h"

/* 根文件系统在第一个硬盘（-hda 挂的 Minix 镜像，整盘无分区表）：
 * 0x0300 = 主设备 3（硬盘）次设备 0。硬盘驱动（6P3）已经过验证，
 * 比第二软驱更可靠；镜像用顶层 tools/mkfs.py 生成 */
#define ROOT_DEV 0x0300

struct super_block super_block[NR_SUPER];

/* ---- 块层适配：1KB 逻辑块读写（软盘/硬盘统一入口） ---- */
void blk_read(int dev, unsigned long block, void *buf)
{
	if (dev == 0x0200 || dev == 0x021c) {	/* 软盘：1 块 = 2 扇区 */
		floppy_read_sector(block * 2, buf);
		floppy_read_sector(block * 2 + 1, (char *)buf + 512);
	} else {			/* 硬盘：走请求队列 */
		extern int hd_do_request(int, unsigned long, unsigned char *, int);
		hd_do_request(dev, block * 2, buf, 0);
		hd_do_request(dev, block * 2 + 1, (char *)buf + 512, 0);
	}
}

static void blk_write(int dev, unsigned long block, void *buf)
{
	/* 写路径留给 7P3 的文件写入实验 */
	printk("blk_write: not implemented yet\n\r");
}

/*
 * read_super —— 读设备上的超级块并校验
 * magic 不对 = 这不是 Minix 文件系统，拒绝挂载
 */
struct super_block *read_super(int dev)
{
	struct super_block *s = &super_block[0];
	/* 注意：blk_read 按整块（1KB）写，缓冲必须给足 1KB——
	 * 曾经用 18 字节的 d_super_block 局部变量接，把栈写爆了
	 * （返回地址被毁，症状是 read_super 的 return 就"跳飞"） */
	unsigned char buf[BLOCK_SIZE];
	struct d_super_block ds;

	if (s->s_dev == dev)
		return s;		/* 已挂载 */

	blk_read(dev, 1, buf);		/* 超级块固定在 1 号块 */
	ds = *(struct d_super_block *)buf;
	if (ds.s_magic != MINIX_SUPER_MAGIC) {
		printk("read_super: bad magic 0x%04x (expect 0x137f)\n\r", ds.s_magic);
		return 0;
	}
	s->s_ninodes = ds.s_ninodes;
	s->s_nzones = ds.s_nzones;
	s->s_imap_blocks = ds.s_imap_blocks;
	s->s_zmap_blocks = ds.s_zmap_blocks;
	s->s_firstdatazone = ds.s_firstdatazone;
	s->s_log_zone_size = ds.s_log_zone_size;
	s->s_max_size = ds.s_max_size;
	s->s_magic = ds.s_magic;
	s->s_dev = dev;

	printk("[super] inodes=%d zones=%d imap=%d zmap=%d firstdata=%d\n\r",
	       s->s_ninodes, s->s_nzones, s->s_imap_blocks, s->s_zmap_blocks,
	       s->s_firstdatazone);
	return s;
}

/* inode 表起始块 = 2(位图前) + imap + zmap */
static inline unsigned long inode_table_block(struct super_block *s)
{
	return 2 + s->s_imap_blocks + s->s_zmap_blocks;
}

/*
 * iget —— 取一个 inode 进内存（教学版：单 inode 槽位，够用）
 * 1 块装 32 个 d_inode：所在块 = 表起始 + (nr-1)/32，块内偏移 (nr-1)%32
 */
struct m_inode *iget2(int dev, unsigned short nr);

/* iget 已升级为带缓存的 iget2（见下方 7.2 节），此处保留兼容名 */
struct m_inode *iget(int dev, unsigned short nr)
{
	return iget2(dev, nr);
}

/*
 * bmap —— 文件内块号 → 磁盘逻辑块号（0.11 的核心映射函数）
 * file_block < 7：直接块 i_zone[file_block]
 * 7 ≤ file_block < 7+512：经 i_zone[7] 一级间接块（1KB 里装 512 个 u16）
 */
unsigned long bmap(struct m_inode *inode, unsigned long file_block)
{
	struct super_block *s = &super_block[0];
	unsigned short buf[BLOCK_SIZE / 2];

	if (file_block < 7)
		return inode->i_zone[file_block];

	/* 一级间接 */
	if (file_block < 7 + 512) {
		if (!inode->i_zone[7])
			return 0;		/* 间接块不存在 */
		blk_read(inode->i_dev, inode->i_zone[7], buf);
		return buf[file_block - 7];
	}
	(void)s;
	return 0;				/* 双间接未用到（1.44MB 盘装不下） */
}

/*
 * list_dir —— 列目录：逐目录项读出打印（7.2 的 namei 前奏）
 * 一个 1KB 块装 64 个目录项
 */
void list_dir(struct m_inode *dir)
{
	unsigned char buf[BLOCK_SIZE];
	struct dir_entry *de;
	unsigned long blocks = (dir->i_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
	unsigned long b, i;

	for (b = 0; b < blocks; b++) {
		unsigned long zone = bmap(dir, b);
		if (!zone)
			continue;
		blk_read(dir->i_dev, zone, buf);
		for (i = 0; i < BLOCK_SIZE / 16; i++) {
			de = (struct dir_entry *)buf + i;
			if (!de->inode)
				continue;
			printk("  %s (inode %d)\n\r", de->name, de->inode);
		}
	}
}

/* mount_root —— 7.1 的完整流程：读超级块 → 根 inode → 列目录 */
void mount_root(void)
{
	struct super_block *s;
	struct m_inode *root;

	printk("[7P1] mount root filesystem (dev=0x%04x, Minix V1)\n\r", ROOT_DEV);

	s = read_super(ROOT_DEV);
	if (!s) {
		printk("mount failed: not a Minix filesystem\n\r");
		return;
	}
	printk("mount: magic OK (0x137f)\n\r");

	root = iget(ROOT_DEV, ROOT_INO);
	printk("root inode: mode=0x%04x size=%d zone0=%d\n\r",
	       root->i_mode, root->i_size, root->i_zone[0]);
	if (!(root->i_mode & S_IFDIR)) {
		printk("root is not a directory!\n\r");
		return;
	}
	printk("root directory entries:\n\r");
	list_dir(root);
}

/* =====================================================================
 * 7.2 补充：inode 缓存（8 槽 + 哈希链）+ 路径查找 namei
 * 对应书中 7.2 节"管理inode"（管理inode元信息 / 管理位图 / 目录结构）
 * ===================================================================== */

#define NR_IHASH 16
static struct m_inode inode_pool[8];		/* 内存 inode 槽 */
static struct m_inode *inode_hash[NR_IHASH];	/* (dev<<16|num) 哈希链 */
static int inode_slot;				/* 简单轮转分配 */

static inline int ihash(int dev, int num)
{
	return ((dev << 2) ^ num) % NR_IHASH;
}

/*
 * iget2 —— 带缓存的 inode 获取：先查哈希（命中 i_count++ 返回），
 * 未命中从池里轮转取一个空/未引用的槽，从磁盘读入，挂哈希链。
 * 7P1 的单槽 iget 升级为这个版本（mount_root 等旧调用兼容）
 */
struct m_inode *iget2(int dev, unsigned short nr)
{
	struct m_inode *mi;
	int h = ihash(dev, nr), i;
	unsigned char buf[BLOCK_SIZE];
	struct d_inode *di;

	for (mi = inode_hash[h]; mi; mi = mi->b_next_i)
		if (mi->i_dev == dev && mi->i_num == nr) {
			mi->i_count++;
			return mi;
		}

	/* 找空槽（i_count==0）或轮转淘汰 */
	for (i = 0; i < 8; i++)
		if (inode_pool[(inode_slot + i) % 8].i_count == 0)
			break;
	mi = &inode_pool[inode_slot = (inode_slot + i) % 8];
	if (mi->i_count == 0 && mi->i_num) {
		/* 从旧哈希链摘除 */
		struct m_inode **p = &inode_hash[ihash(mi->i_dev, mi->i_num)];
		while (*p && *p != mi)
			p = &(*p)->b_next_i;
		*p = mi->b_next_i;
	}

	blk_read(dev, inode_table_block(&super_block[0]) + (nr - 1) / 32, buf);
	di = (struct d_inode *)buf + (nr - 1) % 32;
	mi->i_mode = di->i_mode;
	mi->i_size = di->i_size;
	mi->i_nlinks = di->i_nlinks;
	{
		int j;
		for (j = 0; j < 9; j++)
			mi->i_zone[j] = di->i_zone[j];
	}
	mi->i_count = 1;
	mi->i_dev = dev;
	mi->i_num = nr;
	mi->b_next_i = inode_hash[h];
	inode_hash[h] = mi;
	return mi;
}

/* iput —— 引用计数归还 */
void iput(struct m_inode *mi)
{
	if (mi->i_count)
		mi->i_count--;
}

/*
 * namei —— 按路径名查 inode（书中 7.2/7.3 的"通过路径查询inode"）
 * 逐级解析：从根目录 inode 开始，每级在目录块里找名字匹配的项，
 * iget2 进入下一级。'/' 分隔，支持绝对路径（/a/b/c）
 * 找不到返回 0
 */
struct m_inode *namei(const char *path)
{
	struct m_inode *dir;
	unsigned char buf[BLOCK_SIZE];
	unsigned long blocks, b, i;

	if (*path != '/')
		return 0;			/* 只支持绝对路径 */
	dir = iget2(ROOT_DEV, ROOT_INO);

	while (*path == '/')
		path++;
	if (!*path)
		return dir;			/* 根本身 */

	while (*path) {
		char name[NAME_LEN + 1];
		int len = 0, found = 0;
		struct dir_entry *de;

		while (path[len] && path[len] != '/')
			len++;			/* 取当前路径分量 */
		for (i = 0; i < len; i++)
			name[i] = path[i];
		name[len] = 0;

		blocks = (dir->i_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
		for (b = 0; b < blocks && !found; b++) {
			unsigned long zone = bmap(dir, b);
			if (!zone)
				continue;
			blk_read(dir->i_dev, zone, buf);
			for (i = 0; i < BLOCK_SIZE / 16; i++) {
				de = (struct dir_entry *)buf + i;
				if (!de->inode)
					continue;
				{
					/* 目录项名字是定长 14 字节，比较前 len 个 */
					int j, ok = 1;
					for (j = 0; j < len; j++)
						if (de->name[j] != name[j])
							ok = 0;
					if (de->name[len] && ok)
						ok = 0;
					if (ok && de->inode) {
						iput(dir);
						dir = iget2(dir->i_dev, de->inode);
						found = 1;
						break;
					}
				}
			}
		}
		if (!found) {
			iput(dir);
			return 0;
		}
		path += len;
		while (*path == '/')
			path++;
		if (!*path)
			return dir;
	}
	return dir;
}

/*
 * namei_demo —— 7.2 验证：按名查找 /hello 和 /etc，打印 inode 信息
 */
void namei_demo(void)
{
	struct m_inode *mi;

	mi = namei("/hello");
	if (mi)
		printk("[namei] /hello: inode %d mode=0x%04x size=%d zone0=%d\n\r",
		       mi->i_num, mi->i_mode, mi->i_size, mi->i_zone[0]);
	else
		printk("[namei] /hello NOT FOUND\n\r");

	mi = namei("/etc");
	if (mi)
		printk("[namei] /etc:   inode %d mode=0x%04x (dir=%s)\n\r",
		       mi->i_num, mi->i_mode,
		       (mi->i_mode & S_IFDIR) ? "yes" : "no");
	else
		printk("[namei] /etc NOT FOUND\n\r");

	mi = namei("/no/such/file");
	printk("[namei] /no/such/file: %s\n\r", mi ? "unexpected" : "not found (OK)");
}
