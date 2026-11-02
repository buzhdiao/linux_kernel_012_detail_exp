/*
 * floppy.c —— 软盘驱动：FDC(μPD765/8272A) 编程 + 8237 DMA 传输
 * 对应书中 6.4 节"软盘驱动"（软盘的原理 / 定时器 / DMA / 控制器 / 操作函数）
 * 来源：官方仓 kernel/blk_drv/floppy.c 的教学精简版（单驱动器、1.44MB、读扇区）
 *
 * FDC 端口：
 *   0x3F2 DOR  数字输出：马达、DMA/IRQ 使能、复位、驱动器选择
 *   0x3F4 主状态：bit0=0 FDC 可收命令字节 / bit1=0 FDC 有数据可读，
 *               bit4=1 命令进行中，bit5=1 非DMA方式（我们用 DMA，应为0）
 *   0x3F5 数据口：命令/参数/结果都从这里逐字节进出
 *
 * 读一个扇区的完整序列：
 *   马达开(DOR) → SPECIFY(DMA模式) → SEEK(等中断) → 读中断状态
 *   → 设置 8237 DMA 通道2 → 发 READ 命令(9字节) → FDC 经 DMA 灌数据
 *   → IRQ6 → 读 7 字节结果（ST0-3/C/H/R/N）→ 马达关
 */

#include "kernel.h"
#include "sched.h"

#define FD_DOR      0x3f2		/* 数字输出寄存器 */
#define FD_STATUS   0x3f4		/* 主状态寄存器 */
#define FD_DATA     0x3f5		/* 数据口 */
/* 8237 DMA 通道 2（软盘固定用通道 2） */
#define DMA_MASK    0x0a
#define DMA_MODE    0x0b
#define DMA_FF      0x0c		/* 清 D 触发器（地址/计数字节序翻转） */
#define DMA_CH2_ADDR 0x04
#define DMA_CH2_COUNT 0x05
#define DMA_PAGE_CH2  0x81		/* 页面寄存器（A23-A16） */

/* 等待 FDC 可以收命令字节（RQM：bit7=1 且 bit6=0） */
static int fd_wait_command(void)
{
	int tries = 10000;

	while (tries--) {
		unsigned char st = inb_p(FD_STATUS);
		if ((st & 0xc0) == 0x80)	/* bit7=1, bit6=0 */
			return 0;
	}
	printk("floppy: command port timeout\n\r");
	return -1;
}

/* 等待 FDC 有结果字节可读（bit7=1 且 bit6=1） */
static int fd_wait_result(void)
{
	int tries = 10000;

	while (tries--) {
		unsigned char st = inb_p(FD_STATUS);
		if ((st & 0xc0) == 0xc0)
			return 0;
	}
	printk("floppy: result port timeout\n\r");
	return -1;
}

/* 发 n 个命令字节 */
static int fd_send(const unsigned char *cmd, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		if (fd_wait_command())
			return -1;
		outb_p(cmd[i], FD_DATA);
	}
	return 0;
}

/* 收 n 个结果字节到 buf */
static int fd_result(unsigned char *buf, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		if (fd_wait_result())
			return -1;
		buf[i] = inb_p(FD_DATA);
	}
	return 0;
}

/* 当前操作的软驱号（0=第一驱 /dev/fd0，1=第二驱 /dev/fd1），
 * 由 fs.c 按 ROOT_DEV 设置——DOR 的驱动器/马达位和命令字节都用它 */
int floppy_current_drive = 0;

/* 软盘中断（IRQ6）睡眠等待：floppy_interrupt 桩置位 */
static volatile int fd_interrupt_flag;
static struct task_struct *fd_waiter;

void floppy_interrupt_flag_set(void)
{
	fd_interrupt_flag = 1;
	if (fd_waiter) {
		fd_waiter->state = TASK_RUNNING;
		fd_waiter = 0;
	}
}

static void fd_wait_interrupt(void)
{
	while (!fd_interrupt_flag) {
		current->state = TASK_UNINTERRUPTIBLE;
		fd_waiter = current;
		schedule();
	}
	fd_interrupt_flag = 0;
}

/* SEEK 到指定柱面：发 SEEK 命令 → 等中断 → SENSE INTERRUPT STATUS */
static int fd_seek(int cylinder)
{
	unsigned char cmd[3] = { 0x0f, (unsigned char)(floppy_current_drive << 2),
				 (unsigned char)cylinder };
	unsigned char result[2];

	if (fd_send(cmd, 3))
		return -1;
	fd_wait_interrupt();		/* SEEK 完成产生中断 */

	if (fd_send((unsigned char *)"\x08", 1))	/* SENSE INTERRUPT STATUS */
		return -1;
	if (fd_result(result, 2))
		return -1;
	if ((result[0] & 0xc0) != 0) {	/* ST0 高两位非 00 = 异常结束 */
		printk("floppy: seek error ST0=0x%02x\n\r", result[0]);
		return -1;
	}
	if (result[1] != cylinder) {	/* PCN 应等于目标柱面 */
		printk("floppy: seek to %d got PCN=%d\n\r", cylinder, result[1]);
		return -1;
	}
	return 0;
}

/* 设置 8237 通道 2：单次 DMA 读（设备→内存），地址/计数 */
static void dma_setup(unsigned long addr, int count)
{
	outb(0x02, DMA_MASK);		/* 先屏蔽通道 2 */
	outb(0x00, DMA_FF);		/* 清 D 触发器：接下来按低字节在前写 */

	/* 模式字节：bit2-3=01 单次传输，bit5=0 地址递增，bit6=0 写设备端=读盘，通道2 */
	outb(0x46, DMA_MODE);
	outb(addr & 0xff, DMA_CH2_ADDR);		/* 地址低 8 位 */
	outb((addr >> 8) & 0xff, DMA_CH2_ADDR);		/* 高 8 位 */
	outb((addr >> 16) & 0xff, DMA_PAGE_CH2);	/* 页面（A16-A23） */
	outb((count - 1) & 0xff, DMA_CH2_COUNT);
	outb(((count - 1) >> 8) & 0xff, DMA_CH2_COUNT);
	outb(0x02, DMA_MASK);		/* 解除屏蔽：通道 2 就绪 */
}

/*
 * floppy_read_sector —— 读 1.44MB 软盘的一个物理扇区
 * sector: 0 起的逻辑扇区号（1.44MB = 80 柱面 × 2 磁头 × 18 扇区/道）
 * 映射：cyl = lba / 36；head = (lba / 18) % 2；sec = lba % 18 + 1
 */
int floppy_read_sector(int lba, unsigned char *buf)
{
	unsigned char cmd[9];
	unsigned char result[7];
	int cyl, head, sec;

	cyl = lba / 36;
	head = (lba / 18) & 1;
	sec = lba % 18 + 1;

	/* 复位脉冲：DOR bit2 先清再置，让 FDC 回到已知初始状态 */
	outb(0x0c, FD_DOR);		/* bit2=0：FDC 复位 */
	outb(0x1c, FD_DOR);		/* bit2=1：使能 + 马达0 + DMA/IRQ */
	{
		extern void sys_idle_ticks(int);
		int i;
		unsigned char result[2];

		sys_idle_ticks(20);	/* 复位产生 4 个中断，逐个 SENSE 排空 */
		for (i = 0; i < 4; i++) {
			if (fd_send((unsigned char *)"\x08", 1) == 0)
				fd_result(result, 2);
		}

		sys_idle_ticks(50);	/* 马达起转延时（~0.5s） */

		/* SPECIFY：SRT/HUT + DMA 方式（0x02 bit7=0）。
		 * FDC 复位后必须先 SPECIFY 才能 SEEK/READ（实测踩过） */
		if (fd_send((unsigned char *)"\x03\xdf\x02", 3))
			goto fail;
	}

	if (fd_seek(cyl))
		goto fail;

	dma_setup((unsigned long)buf, 512);	/* DMA 就位：数据直接进 buf */

	cmd[0] = 0xe6;			/* MT=0, MFM=1, SK=0, READ */
	cmd[1] = (floppy_current_drive << 2) | head;	/* 驱动器号 + 磁头号 */
	cmd[2] = cyl;
	cmd[3] = head;
	cmd[4] = sec;
	cmd[5] = 2;			/* 扇区长度码 2 = 512 字节 */
	if (fd_send(cmd, 9))
		goto fail;

	fd_wait_interrupt();		/* DMA 灌完数据后 IRQ6 到达 */

	if (fd_result(result, 7))
		goto fail;
	if (result[0] & 0xc0) {
		printk("floppy: read error ST0=0x%02x ST1=0x%02x ST2=0x%02x\n\r",
		       result[0], result[1], result[2]);
		goto fail;
	}

	outb(0x0c | floppy_current_drive, FD_DOR);	/* 关马达 */
	return 0;
fail:
	outb(0x0c | floppy_current_drive, FD_DOR);
	return -1;
}

/* floppy_init —— 解除 IRQ6 屏蔽（主片 bit6） */
void floppy_init(void)
{
	outb(inb_p(0x21) & ~0x40, 0x21);
	printk("[floppy] IRQ6 unmasked, driver ready\n\r");
}

/* floppy_demo —— 读启动盘的 1 号扇区（setup 的前 512 字节），
 * 里面应有 setup.S 的字符串 "setup is running" */
void floppy_demo(void)
{
	unsigned char buf[512];
	int i, found = 0;

	printk("[6P4] floppy: reading sector 1 (setup image) via DMA...\n\r");
	if (floppy_read_sector(1, buf)) {
		printk("floppy: read failed\n\r");
		return;
	}
	for (i = 0; i < 500; i++) {
		if (buf[i] == 's' && buf[i + 1] == 'e' && buf[i + 2] == 't'
		    && buf[i + 3] == 'u' && buf[i + 4] == 'p') {
			found = 1;
			break;
		}
	}
	printk("floppy: %s (\"setup\" string %s data)\n\r",
	       found ? "READ OK" : "content unexpected",
	       found ? "found in" : "not in");
	printk("first bytes: %02x %02x %02x %02x\n\r", buf[0], buf[1], buf[2], buf[3]);
}
