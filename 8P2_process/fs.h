/*
 * fs.h —— Minix V1 文件系统的磁盘结构与内核结构
 * 对应书中 7.1 节"Minix文件系统"（文件系统的基本结构 / 定义超级块 / 初始化超级块）
 * 来源：官方仓 include/linux/fs.h 的教学精简版
 */

#ifndef _FS_H
#define _FS_H

#define MINIX_SUPER_MAGIC 0x137F
#define BLOCK_SIZE  1024		/* 逻辑块 1KB = 2 个软盘扇区 */

#define NR_SUPER 1			/* 本实验只挂一个文件系统 */
#define ROOT_INO 1			/* 根目录 inode 号固定为 1 */

/* ---- 磁盘上的超级块（块 1，1024 字节里只用前 18 字节） ---- */
struct d_super_block {
	unsigned short s_ninodes;	/* inode 总数 */
	unsigned short s_nzones;	/* 逻辑块总数 */
	unsigned short s_imap_blocks;	/* inode 位图占的块数 */
	unsigned short s_zmap_blocks;	/* zone 位图占的块数 */
	unsigned short s_firstdatazone;/* 第一个数据块号 */
	unsigned short s_log_zone_size;	/* log2(块/扇区) = 0（1KB 块） */
	unsigned long s_max_size;	/* 文件最大长度 */
	unsigned short s_magic;		/* 0x137F = Minix V1 */
	unsigned short s_state;
	unsigned long s_time;
};

/* ---- 磁盘上的 inode（32 字节，V1 的 zone 号是 16 位） ---- */
struct d_inode {
	unsigned short i_mode;
	unsigned short i_uid;
	unsigned long i_size;
	unsigned long i_mtime;
	unsigned char i_gid;
	unsigned char i_nlinks;
	unsigned short i_zone[9];	/* 7 直接 + 1 一级间接 + 1 双间接 */
};

/* ---- 目录项（16 字节） ---- */
#define NAME_LEN 14
struct dir_entry {
	unsigned short inode;		/* 0 = 空闲项 */
	char name[NAME_LEN];
};

/* ---- 内核里的"挂载了的"超级块 ---- */
struct super_block {
	unsigned short s_ninodes;
	unsigned short s_nzones;
	unsigned short s_imap_blocks;
	unsigned short s_zmap_blocks;
	unsigned short s_firstdatazone;
	unsigned short s_log_zone_size;
	unsigned long s_max_size;
	unsigned short s_magic;
	int s_dev;			/* 挂载的设备号 */
};

/* ---- 内核里的 inode（内存版：磁盘 inode + 引用计数 + 位置） ---- */
struct m_inode {
	unsigned short i_mode;
	unsigned short i_uid;
	unsigned long i_size;
	unsigned long i_mtime;
	unsigned char i_gid;
	unsigned char i_nlinks;
	unsigned short i_zone[9];
	unsigned short i_count;		/* 引用计数 */
	unsigned short i_dev;		/* 所在设备 */
	unsigned short i_num;		/* inode 号 */
	struct m_inode *b_next_i;	/* inode 哈希链（7.2） */
};

/* 文件类型位 */
#define S_IFDIR  0x4000
#define S_IFREG  0x8000

#endif
