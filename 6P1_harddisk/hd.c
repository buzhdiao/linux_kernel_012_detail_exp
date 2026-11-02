/*
 * hd.c —— 硬盘基本结构：ATA 控制器探测 + MBR 分区表解析
 * 对应书中 6.1 节"硬盘的基本结构"（初始化硬盘信息 / 读取分区表）
 * 来源：官方仓 kernel/blk_drv/hd.c 的教学精简版
 *
 * ATA(PIO) 硬盘编程模型——主通道寄存器：
 *   0x1F0 数据口（16 位读写）
 *   0x1F1 错误/特征
 *   0x1F2 扇区数
 *   0x1F3-0x1F5 LBA 低/中/高 8 位
 *   0x1F6 驱动器/LBA 最高 4 位（bit6=1 表示 LBA 模式）
 *   0x1F7 命令/状态口（0x20=读扇区，0xEC=IDENTIFY，bit3=1 数据就绪）
 *
 * 读扇区流程：写扇区数/LBA → 写命令 → 等状态 bit3 → 从 0x1F0 读 256 字
 * IDENTIFY：发 0xEC → 有响应则读回 256 字的设备信息（型号/扇区数）
 * MBR：0 号扇区末尾 0x55AA；偏移 446 起每 16 字节一个分区表项，共 4 项：
 *   [0]=活动标志 [1..3]=CHS起始 [4]=类型 [5..7]=CHS结束 [8..11]=起始LBA [12..15]=扇区数
 */

#include "kernel.h"

#define HD_DATA    0x1f0
#define HD_ERROR   0x1f1
#define HD_NSECTOR 0x1f2
#define HD_SECTOR  0x1f3
#define HD_LCYL    0x1f4
#define HD_HCYL    0x1f5
#define HD_CURRENT 0x1f6
#define HD_STATUS  0x1f7
#define HD_COMMAND HD_STATUS
/* 状态寄存器位 */
#define HD_BSY  0x80
#define HD_DRQ  0x08

#define HD_CMD_IDENTIFY 0xec
#define HD_CMD_READ     0x20

#define NR_PARTITIONS 4

/* 一个分区表项（MBR 偏移 446 起每 16 字节） */
struct partition_info {
	unsigned char active;		/* 0x80 = 可引导 */
	unsigned char chs_start[3];
	unsigned char type;		/* 0x83 = Linux */
	unsigned char chs_end[3];
	unsigned long lba_start;	/* 起始 LBA（小端） */
	unsigned long sectors;		/* 分区扇区数 */
};

/* 收集到的硬盘信息（书中 hd_info 的雏形） */
static unsigned long hd_total_sectors;
static char hd_model[41];

/* 从 0x1F0 读 count 个字到内存（数据口是 16 位的，必须用 inw） */
static void hd_read_words(unsigned short *buf, int count)
{
	int i;

	for (i = 0; i < count; i++)
		buf[i] = inw_p(HD_DATA);
}

/* 等待状态寄存器：BSY 清零且 DRQ 置位；返回 0 成功 */
static int hd_wait_drq(void)
{
	unsigned char st;
	int tries = 100000;

	while (tries--) {
		st = inb_p(HD_STATUS);
		if (!(st & HD_BSY) && (st & HD_DRQ))
			return 0;
	}
	printk("hd: wait DRQ timeout, status=0x%02x\n\r", st);
	return -1;
}

/*
 * hd_identify —— 发 IDENTIFY 命令收集设备信息
 * 型号在返回数据的第 27~46 字（ASCII），总扇区数在第 60 字
 */
static int hd_identify(void)
{
	unsigned short buf[256];
	int i;

	outb_p(0, HD_NSECTOR);
	outb_p(0, HD_SECTOR);
	outb_p(0, HD_LCYL);
	outb_p(0, HD_HCYL);
	outb_p(0xa0, HD_CURRENT);	/* 主盘，LBA 位 */
	outb_p(HD_CMD_IDENTIFY, HD_COMMAND);

	if (inb_p(HD_STATUS) == 0)	/* 无设备：状态为 0 */
		return -1;
	if (hd_wait_drq())
		return -1;

	hd_read_words(buf, 256);

	/* 型号：字节对是大端的 ASCII，逐字交换 */
	for (i = 0; i < 20; i++) {
		hd_model[i * 2] = (char)(buf[27 + i] >> 8);
		hd_model[i * 2 + 1] = (char)(buf[27 + i] & 0xff);
	}
	hd_model[40] = 0;

	hd_total_sectors = ((unsigned long)buf[61] << 16) | buf[60];
	return 0;
}

/*
 * hd_read_sector —— LBA 模式读一个扇区到 buf（512 字节）
 * 后续 6P3 会把它改造成"请求队列 + 中断完成"的完整驱动
 */
void hd_read_sector(unsigned long lba, unsigned char *buf)
{
	outb_p(1, HD_NSECTOR);
	outb_p(lba & 0xff, HD_SECTOR);
	outb_p((lba >> 8) & 0xff, HD_LCYL);
	outb_p((lba >> 16) & 0xff, HD_HCYL);
	outb_p(0xe0 | ((lba >> 24) & 0x0f), HD_CURRENT);
	outb_p(HD_CMD_READ, HD_COMMAND);

	if (hd_wait_drq())
		return;
	hd_read_words((unsigned short *)buf, 256);
}

/* 打印分区表：类型/活动/起始 LBA/扇区数 */
static void print_partitions(unsigned char *mbr)
{
	struct partition_info *p = (struct partition_info *)(mbr + 446);
	int i;

	for (i = 0; i < NR_PARTITIONS; i++, p++) {
		if (p->type == 0)
			continue;		/* 空表项 */
		printk("  part %d: type=0x%02x %s start_lba=%d sectors=%d (%dMB)\n\r",
		       i, p->type, p->active == 0x80 ? "[boot]" : "",
		       p->lba_start, p->sectors, p->sectors >> 11);
	}
}

/* hd_init —— 探测硬盘并读分区表（6.1 的两个任务） */
void hd_init(void)
{
	unsigned char mbr[512];

	printk("\n[6P1] hard disk: identify + partition table\n\r");

	if (hd_identify()) {
		printk("hd: no disk found on primary master\n\r");
		return;
	}
	printk("model: %s\n\r", hd_model);
	printk("total sectors: %d (%d MB)\n\r",
	       hd_total_sectors, hd_total_sectors >> 11);

	hd_read_sector(0, mbr);
	if (mbr[510] != 0x55 || mbr[511] != 0xaa) {
		printk("mbr: no 0x55AA signature (blank disk?)\n\r");
		return;
	}
	printk("mbr signature OK, partitions:\n\r");
	print_partitions(mbr);
}
