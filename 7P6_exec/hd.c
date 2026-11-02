/*
 * hd.c —— 硬盘驱动：请求队列 + IRQ14 中断驱动读写
 * 对应书中 6.3 节"硬盘驱动"（读写硬盘 / 读分区表）
 * 来源：官方仓 kernel/blk_drv/hd.c + ll_rw_blk.c 的教学精简版
 *
 * 同步版（6.1）的问题：CPU 轮询等 DRQ，盘慢时白烧 CPU。
 * 完整驱动的做法（本实验）：
 *   1. 上层把读写请求挂进请求队列（ll_rw_block）
 *   2. 队列空则立刻发起队首请求（ATA 编程），发起后开 IRQ14
 *   3. CPU 睡眠（hlt/任务睡眠）；控制器完成 → IRQ14 → handler 读数据、
 *      唤醒等待任务、继续发起下一个请求
 * 这就是"块设备请求队列"模型——所有块设备（硬盘/软盘）共用
 */

#include "kernel.h"
#include "sched.h"

#define HD_DATA    0x1f0
#define HD_NSECTOR 0x1f2
#define HD_SECTOR  0x1f3
#define HD_LCYL    0x1f4
#define HD_HCYL    0x1f5
#define HD_CURRENT 0x1f6
#define HD_STATUS  0x1f7
#define HD_COMMAND HD_STATUS
#define HD_BSY  0x80
#define HD_DRQ  0x08
#define HD_CMD_READ  0x20
#define HD_CMD_WRITE 0x30

#define NR_REQUEST 8

/* 请求块：块设备层的"工作单" */
struct hd_request {
	int rq_dev;
	unsigned long rq_sector;	/* 起始 LBA */
	unsigned char *rq_buffer;	/* 数据缓冲 */
	int rq_write;			/* 0=读 1=写 */
	int rq_done;			/* 完成标志 */
};

static struct hd_request request[NR_REQUEST];
static int rq_head, rq_tail;		/* 队列：tail 取，head 放 */
static struct task_struct *hd_waiter;	/* 睡眠等待"队列有变化"的任务 */

/* 数据口 16 位写（写扇区用） */
static inline void outw_p(unsigned short v, unsigned short port)
{
	__asm__ volatile("outw %0,%1\n\tjmp 1f\n1:\tjmp 1f\n1:" :: "a"(v), "d"(port));
}

/* 发起一个请求：ATA 编程（读写皆可） */
static void hd_start(struct hd_request *rq)
{
	unsigned long lba = rq->rq_sector;
	int i;

	outb_p(1, HD_NSECTOR);
	outb_p(lba & 0xff, HD_SECTOR);
	outb_p((lba >> 8) & 0xff, HD_LCYL);
	outb_p((lba >> 16) & 0xff, HD_HCYL);
	outb_p(0xe0 | ((lba >> 24) & 0x0f), HD_CURRENT);
	outb_p(rq->rq_write ? HD_CMD_WRITE : HD_CMD_READ, HD_COMMAND);

	/* 写命令需要立刻把数据灌进控制器（ PIO 写的 DRQ 立即置位） */
	if (rq->rq_write) {
		unsigned short *p = (unsigned short *)rq->rq_buffer;
		int tries = 100000;
		while (!(inb_p(HD_STATUS) & HD_DRQ) && tries--)
			;
		for (i = 0; i < 256; i++)
			outw_p(p[i], HD_DATA);
	}
}

/* 完成当前请求：唤醒等待者，队列还有请求就继续发起 */
static void hd_complete(void)
{
	request[rq_tail].rq_done = 1;

	if (rq_tail != rq_head) {
		rq_tail = (rq_tail + 1) % NR_REQUEST;
		if (rq_tail != rq_head)
			hd_start(&request[rq_tail]);
	} else {
		rq_tail = (rq_tail + 1) % NR_REQUEST;	/* 空队列复位 */
	}

	if (hd_waiter) {
		hd_waiter->state = TASK_RUNNING;
		hd_waiter = 0;
	}
}

/* IRQ14 中断的 C 处理：读请求的数据，然后完成它 */
void hd_interrupt_handler(void)
{
	struct hd_request *rq = &request[rq_tail];
	unsigned short *p;
	int i;

	if (!rq->rq_write) {
		unsigned short buf[256];
		int tries = 100000;
		while (!(inb_p(HD_STATUS) & HD_DRQ) && tries--)
			;
		for (i = 0; i < 256; i++)
			buf[i] = inw_p(HD_DATA);
		p = (unsigned short *)rq->rq_buffer;
		for (i = 0; i < 256; i++)
			p[i] = buf[i];
	}
	hd_complete();
}

/* hd_wait —— 睡眠等待自己的请求完成。
 * 关键：进系统调用时 IF=0（int 0x80 中断门），ATA 命令发出去之后
 * 完成中断永远进不来——睡眠循环里必须先 sti 放行中断
 * （本项目实测踩过：mount 时 IF=1 一切正常，syscall 里读文件就死锁） */
static void hd_wait_done(struct hd_request *rq)
{
	while (!rq->rq_done) {
		current->state = TASK_UNINTERRUPTIBLE;
		hd_waiter = current;
		__asm__ volatile("sti");	/* 放行完成中断 */
		schedule();
	}
}

/*
 * hd_do_request —— 同步接口：挂队列，等完成，返回
 * （官方是 ll_rw_block 异步挂队列 + make_request 调度发起，
 *   本实验把"发起"合并在这里，教学更直观）
 */
int hd_do_request(int dev, unsigned long sector, unsigned char *buf, int write)
{
	struct hd_request *rq;
	int was_idle = (rq_tail == rq_head);

	rq = &request[rq_head];
	rq->rq_dev = dev;
	rq->rq_sector = sector;
	rq->rq_buffer = buf;
	rq->rq_write = write;
	rq->rq_done = 0;
	rq_head = (rq_head + 1) % NR_REQUEST;

	if (was_idle)
		hd_start(rq);		/* 控制器空闲：立刻发起 */

	hd_wait_done(rq);
	return 0;
}

/* hd_init —— 解除 IRQ14 屏蔽（bit6），读分区表验证 */
void hd_init(void)
{
	unsigned char mbr[512];

	/* IRQ14 挂在从片上（从片 bit6），从片再经 IRQ2 级联到主片——
	 * 所以主片 bit2 和从片 bit6 都必须开放（少一个中断都到不了 CPU） */
	outb(inb_p(0x21) & ~0x04, 0x21);	/* 主片：开放 IRQ2 级联线 */
	outb(inb_p(0xa1) & ~0x40, 0xa1);	/* 从片：开放 IRQ14 */

	hd_do_request(0x300, 0, mbr, 0);
}

/* hd_read_write_demo —— 写一个扇区再读回验证（读改写闭环） */
void hd_read_write_demo(void)
{
	unsigned char buf[512];
	int i;

	for (i = 0; i < 512; i++)
		buf[i] = (unsigned char)(i & 0xff);	/* 测试图案 */

	hd_do_request(0x300, 64, buf, 1);		/* 写 LBA 64 */
	printk("write sector 64 done\n\r");

	for (i = 0; i < 512; i++)
		buf[i] = 0;
	hd_do_request(0x300, 64, buf, 0);		/* 读回 */
	for (i = 0; i < 512; i++) {
		if (buf[i] != (unsigned char)(i & 0xff)) {
			printk("verify FAILED at %d: %02x\n\r", i, buf[i]);
			return;
		}
	}
	printk("read back & verify OK (512 bytes match)\n\r");
}
