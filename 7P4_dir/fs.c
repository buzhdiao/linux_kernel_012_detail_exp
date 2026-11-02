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

/* 块 IO 的共享 1KB 缓冲：栈只有 4KB，调用链上多个 1KB 局部缓冲会把栈
 * 压穿（实测：栈顶侵入 IDT 区域，异常门被覆盖，三重故障）。
 * 单内核线程时复用安全；blk_read/blk_write 及其调用方都在同一层使用 */
static unsigned char blkbuf[BLOCK_SIZE];

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
	struct d_super_block ds;

	if (s->s_dev == dev)
		return s;		/* 已挂载 */

	blk_read(dev, 1, blkbuf);	/* 超级块固定在 1 号块 */
	ds = *(struct d_super_block *)blkbuf;
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
	unsigned char *buf = blkbuf;
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

	blk_read(dev, inode_table_block(&super_block[0]) + (nr - 1) / 32, blkbuf);
	di = (struct d_inode *)blkbuf + (nr - 1) % 32;
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
	unsigned char *buf = blkbuf;
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

/* =====================================================================
 * 7.3 补充：进程文件表 + open/read/close
 * 对应书中 7.3 节"管理普通文件"（打开文件 / 读操作 / 读写）
 * 来源：官方仓 fs/open.c + fs/file_dev.c + fs/file_table.c 精简版
 * ===================================================================== */

#define NR_OPEN 8		/* 每进程最多打开的文件数 */

struct file {
	unsigned short f_mode;		/* 1=读 2=写 */
	int f_pos;			/* 读写偏移 */
	struct m_inode *f_inode;	/* 指向缓存的 inode */
};

struct file file_table[NR_OPEN];	/* 全系统文件表（0.11 的设计） */

/*
 * file_open —— open 的内核部分：namei + 在文件表里领一个结构
 * 返回 fd（文件表下标）；失败 -1
 */
int file_open(const char *path, int mode)
{
	struct m_inode *mi = namei(path);
	int i;

	if (!mi)
		return -1;
	if (mi->i_mode & S_IFDIR) {	/* 目录不给 read（简化限制） */
		iput(mi);
		return -1;
	}
	for (i = 0; i < NR_OPEN; i++) {
		if (!file_table[i].f_inode) {
			file_table[i].f_mode = mode;
			file_table[i].f_pos = 0;
			file_table[i].f_inode = mi;
			return i;
		}
	}
	iput(mi);
	return -1;			/* 文件表满 */
}

/*
 * file_read —— read 的内核部分：offset → bmap → 读块 → 拷贝
 * 跨块读由循环处理；文件尾返回 0
 */
int file_read(int fd, char *buf, int n)
{
	struct file *f = &file_table[fd];
	struct m_inode *mi;
	int done = 0;

	if (fd < 0 || fd >= NR_OPEN || !f->f_inode)
		return -1;
	mi = f->f_inode;

	while (done < n && f->f_pos < (int)mi->i_size) {
		unsigned long file_block = f->f_pos / BLOCK_SIZE;
		int off_in_block = f->f_pos % BLOCK_SIZE;
		int chunk;
		unsigned long zone;
		unsigned char blk[BLOCK_SIZE];

		zone = bmap(mi, file_block);
		if (!zone)
			break;			/* 稀疏洞：按 0 处理到此为止 */
		blk_read(mi->i_dev, zone, blk);

		chunk = BLOCK_SIZE - off_in_block;
		if (chunk > (int)mi->i_size - f->f_pos)
			chunk = mi->i_size - f->f_pos;
		if (chunk > n - done)
			chunk = n - done;

		{
			int i;
			for (i = 0; i < chunk; i++)
				buf[done + i] = blk[off_in_block + i];
		}
		f->f_pos += chunk;
		done += chunk;
	}
	return done;
}

/* file_close —— 归还文件表项和 inode 引用 */
int file_close(int fd)
{
	if (fd < 0 || fd >= NR_OPEN || !file_table[fd].f_inode)
		return -1;
	iput(file_table[fd].f_inode);
	file_table[fd].f_inode = 0;
	return 0;
}

/* =====================================================================
 * 7.4 补充：写路径——位图分配、inode 写回、mkdir/creat/rmdir
 * 对应书中 7.4 节"管理目录"（创建目录 / 创建文件 / rmdir删除目录 / 新建文件）
 * 来源：官方仓 fs/bitmap.c + fs/namei.c 的 mkdir/creat/rmdir 精简版
 * ===================================================================== */

void blk_write(int dev, unsigned long block, void *buf)
{
	extern int hd_do_request(int, unsigned long, unsigned char *, int);
	hd_do_request(dev, block * 2, buf, 1);
	hd_do_request(dev, block * 2 + 1, (char *)buf + 512, 1);
}

/* ---- 位图：把 inode/zone 位图块读进缓存（挂载时），分配 = 找 0 置 1 ---- */
static unsigned char imap_cache[BLOCK_SIZE];
static unsigned char zmap_cache[BLOCK_SIZE];
static int maps_loaded;

static void load_maps(void)
{
	struct super_block *s = &super_block[0];

	if (maps_loaded)
		return;
	blk_read(s->s_dev, 2, imap_cache);
	blk_read(s->s_dev, 2 + s->s_imap_blocks, zmap_cache);
	maps_loaded = 1;
}

static void save_maps(void)
{
	struct super_block *s = &super_block[0];

	blk_write(s->s_dev, 2, imap_cache);
	blk_write(s->s_dev, 2 + s->s_imap_blocks, zmap_cache);
}

/* new_block —— 从位图分配一个 inode 号（nr≥1）或 zone 号（≥firstdatazone） */
static int new_bitmap(unsigned char *map, int max, int skip)
{
	int i, j;

	for (i = skip / 8; i < max / 8; i++) {
		if (map[i] == 0xff)
			continue;
		for (j = 0; j < 8; j++) {
			int bit = i * 8 + j;
			if (bit < skip)
				continue;	/* 跳过保留号 */
			if (!(map[i] & (1 << j))) {
				map[i] |= 1 << j;
				return bit;
			}
		}
	}
	return 0;			/* 满（返回 0 表示失败） */
}

/* new_inode —— 分配 inode 号并写一个初始 inode 到磁盘 */
static int new_inode(struct super_block *s, int mode)
{
	int nr = new_bitmap(imap_cache, s->s_ninodes, 2);	/* 0 失败码 1 根保留 */
	struct d_inode *di;

	if (nr < 2)			/* 0 失败、1 根保留 */
		return 0;
	/* 该 inode 所在块的旧内容读出，改这 32 字节，写回 */
	blk_read(s->s_dev, inode_table_block(s) + (nr - 1) / 32, blkbuf);
	di = (struct d_inode *)blkbuf + (nr - 1) % 32;
	di->i_mode = mode;
	di->i_uid = 0;
	di->i_size = 0;
	di->i_mtime = 0;
	di->i_gid = 0;
	di->i_nlinks = 1;
	{
		int i;
		for (i = 0; i < 9; i++)
			di->i_zone[i] = 0;
	}
	blk_write(s->s_dev, inode_table_block(s) + (nr - 1) / 32, blkbuf);
	return nr;
}

/* new_zone —— 分配一个数据块并清零 */
static int new_zone(struct super_block *s)
{
	int z = new_bitmap(zmap_cache, s->s_nzones, s->s_firstdatazone);
	int i;

	if (!z || z < (int)s->s_firstdatazone)
		return 0;
	for (i = 0; i < BLOCK_SIZE; i++)
		blkbuf[i] = 0;
	blk_write(s->s_dev, z, blkbuf);
	return z;
}

/* add_entry —— 往目录的末尾追加一个目录项（扩展目录 size） */
static int add_entry(struct m_inode *dir, const char *name, int ino)
{
	unsigned long file_block = dir->i_size / BLOCK_SIZE;
	int off_in_block = dir->i_size % BLOCK_SIZE;
	unsigned long zone;
	struct dir_entry *de;
	int i;

	if (off_in_block == 0) {		/* 需要一个新块：分配直接块 */
		zone = new_zone(&super_block[0]);
		if (!zone)
			return -1;
		if (file_block < 7) {
			dir->i_zone[file_block] = zone;
		} else {
			return -1;		/* 间接块分配留给读者扩展 */
		}
	} else {
		zone = bmap(dir, file_block);
		blk_read(dir->i_dev, zone, blkbuf);
	}
	de = (struct dir_entry *)blkbuf + off_in_block / 16;
	de->inode = ino;
	for (i = 0; i < NAME_LEN; i++)
		de->name[i] = name[i] ? name[i] : 0;
	blk_write(dir->i_dev, zone, blkbuf);
	dir->i_size += 16;

	/* 目录 inode 的 size 变了：写回 inode 表 */
	{
		struct super_block *s = &super_block[0];
		struct d_inode *di;
		blk_read(dir->i_dev, inode_table_block(s) + (dir->i_num - 1) / 32, blkbuf);
		di = (struct d_inode *)blkbuf + (dir->i_num - 1) % 32;
		di->i_size = dir->i_size;
		{
			int j;
			for (j = 0; j < 7; j++)
				di->i_zone[j] = dir->i_zone[j];
		}
		blk_write(dir->i_dev, inode_table_block(s) + (dir->i_num - 1) / 32, blkbuf);
	}
	return 0;
}

/* fs_mkdir —— mkdir(path)：新目录含 . 和 ..，挂进父目录 */
int fs_mkdir(const char *path)
{
	struct super_block *s = &super_block[0];
	char parent[64], name[NAME_LEN + 1];
	int i, len = 0, ino, z;
	struct m_inode *pdir;
	struct dir_entry *de;

	printk("[mkdir] enter '%s'\n\r", path);
	for (i = 0; path[i]; i++)
		if (path[i] == '/')
			len = i;		/* 最后一个斜杠前是父目录 */
	for (i = 0; i < len; i++)
		parent[i] = path[i];
	parent[len] = 0;
	if (len == 0) {
		parent[0] = '/';
		parent[1] = 0;		/* 父目录就是根：/name 形式 */
	}
	for (i = 0; i < NAME_LEN && path[len + 1 + i]; i++)
		name[i] = path[len + 1 + i];
	name[i] = 0;

	load_maps();
	printk("[mkdir] maps loaded\n\r");
	pdir = namei(parent);
	printk("[mkdir] parent='%s' -> %s\n\r", parent,
	       pdir ? "ok" : "NOT FOUND");
	if (!pdir || !(pdir->i_mode & S_IFDIR)) {
		printk("mkdir: parent not found\n\r");
		return -1;
	}

	z = new_zone(s);			/* 目录数据块：. 和 .. */
	printk("[mkdir] new zone = %d\n\r", z);
	if (!z)
		return -1;
	{
			struct dir_entry *e;
		int j;
		for (j = 0; j < BLOCK_SIZE; j++)
			blkbuf[j] = 0;
		e = (struct dir_entry *)blkbuf;
		e[0].inode = 0;			/* 回填在建 inode 后 */
		e[1].inode = pdir->i_num;
		for (i = 0; i < NAME_LEN; i++) {
			e[0].name[i] = (i == 0) ? '.' : 0;
			e[1].name[i] = (i < 2) ? ".."[i] : 0;
		}
		(void)de;
		blk_write(s->s_dev, z, blkbuf);
	}

	ino = new_inode(s, S_IFDIR | 0755);
	printk("[mkdir] new inode = %d\n\r", ino);
	if (!ino)
		return -1;
	/* 回填 . 指向自己 */
	{
		blk_read(s->s_dev, z, blkbuf);
		((struct dir_entry *)blkbuf)[0].inode = ino;
		blk_write(s->s_dev, z, blkbuf);
	}
	/* 内存 inode 同步（供 add_entry 写 size 用） */
	{
		struct m_inode *mi = iget2(s->s_dev, ino);
		mi->i_size = 32;
		mi->i_zone[0] = z;
	}
	add_entry(pdir, name, ino);
	save_maps();
	iput(pdir);
	printk("mkdir %s OK (inode %d)\n\r", path, ino);
	return ino;
}
/* fs_creat —— 新建空普通文件并登记进父目录（简版：不含 O_TRUNC 语义） */
int fs_creat(const char *path)
{
	struct super_block *s = &super_block[0];
	char parent[64], name[NAME_LEN + 1];
	int i, len = 0, ino;
	struct m_inode *pdir;

	for (i = 0; path[i]; i++)
		if (path[i] == '/')
			len = i;
	for (i = 0; i < len; i++)
		parent[i] = path[i];
	parent[len] = 0;
	if (len == 0) {
		parent[0] = '/';
		parent[1] = 0;		/* 父目录就是根：/name 形式 */
	}
	for (i = 0; i < NAME_LEN && path[len + 1 + i]; i++)
		name[i] = path[len + 1 + i];
	name[i] = 0;

	load_maps();
	pdir = namei(parent);
	if (!pdir || !(pdir->i_mode & S_IFDIR))
		return -1;
	ino = new_inode(s, S_IFREG | 0644);
	if (!ino)
		return -1;
	add_entry(pdir, name, ino);
	save_maps();
	iput(pdir);
	printk("creat %s OK (inode %d)\n\r", path, ino);
	return ino;
}

/* fs_rmdir —— 删除空目录：目录项清零、位图回收、inode 释放 */
int fs_rmdir(const char *path)
{
	struct super_block *s = &super_block[0];
	char parent[64], name[NAME_LEN + 1];
	int i, len = 0;
	struct m_inode *pdir, *mi;
	struct dir_entry *de;
	unsigned long b;

	for (i = 0; path[i]; i++)
		if (path[i] == '/')
			len = i;
	for (i = 0; i < len; i++)
		parent[i] = path[i];
	parent[len] = 0;
	if (len == 0) {
		parent[0] = '/';
		parent[1] = 0;		/* 父目录就是根：/name 形式 */
	}
	for (i = 0; i < NAME_LEN && path[len + 1 + i]; i++)
		name[i] = path[len + 1 + i];
	name[i] = 0;

	mi = namei(path);
	if (!mi || !(mi->i_mode & S_IFDIR)) {
		printk("rmdir: not a directory\n\r");
		return -1;
	}
	if (mi->i_size != 32) {		/* 只有 . .. 两项才允许删（简版检查） */
		printk("rmdir: not empty (size=%d)\n\r", mi->i_size);
		iput(mi);
		return -1;
	}
	load_maps();
	pdir = namei(parent);

	/* 父目录里找到该名字的项，inode 清 0（0=空闲项） */
	for (b = 0; b < (pdir->i_size + BLOCK_SIZE - 1) / BLOCK_SIZE; b++) {
		unsigned long zone = bmap(pdir, b);
		int j;
		if (!zone)
			continue;
		blk_read(pdir->i_dev, zone, blkbuf);
		for (j = 0; j < BLOCK_SIZE / 16; j++) {
			de = (struct dir_entry *)blkbuf + j;
			if (de->inode == mi->i_num) {
				int k, ok = 1;
				for (k = 0; k < NAME_LEN; k++)
					if (de->name[k] != name[k])
						ok = 0;
				if (ok) {
					de->inode = 0;
					blk_write(pdir->i_dev, zone, blkbuf);
					goto removed;
				}
			}
		}
	}
	printk("rmdir: entry not found in parent\n\r");
	iput(mi); iput(pdir);
	return -1;
removed:
	/* inode 号和 zone 号的位图回收（简版：只回收 inode 号；
	 * zone 0 对应位清掉） */
	imap_cache[mi->i_num >> 3] &= ~(1 << (mi->i_num & 7));
	zmap_cache[mi->i_zone[0] >> 3] &= ~(1 << (mi->i_zone[0] & 7));
	save_maps();
	iput(mi); iput(pdir);
	printk("rmdir %s OK\n\r", path);
	return 0;
}

/* dir_demo —— 7.4 验证：建目录建文件删目录再核对 */
void dir_demo(void)
{
	struct m_inode *mi;

	fs_mkdir("/mydir");
	fs_creat("/mydir/inner.txt");
	fs_mkdir("/mydir2");

	printk("root now has:\n\r");
	{
		struct m_inode *root = iget2(ROOT_DEV, ROOT_INO);
		list_dir(root);
		iput(root);
	}

	mi = namei("/mydir/inner.txt");
	printk("/mydir/inner.txt: %s (inode %d)\n\r",
	       mi ? "FOUND" : "NOT FOUND", mi ? mi->i_num : 0);

	fs_rmdir("/mydir2");
	fs_rmdir("/mydir");			/* 非空：应被拒绝 */
	printk("after rmdir /mydir (should refuse, not empty):\n\r");
	mi = namei("/mydir");
	printk("/mydir: %s\n\r", mi ? "still there (OK)" : "gone (WRONG)");
}
