/*
 * tty.c —— 终端抽象层：tty_struct + 规范模式读
 * 对应书中 5.2 节"优化输入功能"（控制台和远程终端 / read系统调用）
 * 来源：官方仓 kernel/chr_drv/tty_io.c 的精简重写
 *
 * tty 的核心思想：在"设备驱动"和"系统调用"之间插一层中间结构——
 *   键盘中断 → (secondary 队列：攒行/回显) → read 系统调用从队列取
 * tty_struct 里挂着读写两个队列和一组属性（termios，5.4 展开）。
 * 有了这层抽象，"控制台"和"串口终端"对用户程序来说就是同一个
 * read/write 接口——只是背后的驱动不同
 *
 * 本实验实现规范模式（canonical）读：
 *   - 回车前的字符攒在 secondary 队列，read 一次返回一整行
 *   - 回显继续由键盘中断直接做（5.3 会挪到规范的 con_write 路径）
 */

#include "kernel.h"
#include "sched.h"

#include "tty.h"

/* 控制台 tty（本内核只有这一个终端设备；串口终端留待扩展） */
struct tty_struct tty_table[1] = {
	{ { 0 }, { 0 }, 0 }
};

/*
 * tty_do_char —— 键盘中断把解析好的字符交给 tty 层
 * 规范模式：攒字符；'\n' 置行完成标志并唤醒睡眠的读者
 */
void tty_do_char(char c)
{
	struct tty_queue *q = &tty_table[0].secondary;

	if ((q->head + 1) % TTY_BUF_SIZE == q->tail)
		return;				/* 队列满：丢弃 */

	q->buf[q->head] = c;
	q->head = (q->head + 1) % TTY_BUF_SIZE;

	if (c == '\n')
		tty_table[0].line_complete = 1;	/* 一行就绪 */

	/* 唤醒睡眠在 read 上的任务（下一节调度它就会跑到） */
	if (tty_table[0].reader) {
		tty_table[0].reader->state = TASK_RUNNING;
		tty_table[0].reader = 0;
	}
}

/*
 * tty_read —— sys_read(0, buf, n) 的内核实现（规范模式）
 * 没攒够一行就睡眠等待；有行则把 secondary 队列内容拷到用户缓冲
 * buf 是用户态地址——恒等映射下内核可直接访问（真正的
 * verify_area 检查随 5.3 的 write 一起说明）
 */
int tty_read(char *buf, int n)
{
	struct tty_struct *tty = &tty_table[0];
	struct tty_queue *q = &tty->secondary;
	int i = 0;

	while (!tty->line_complete) {
		/* 睡眠：绝不能在系统调用里忙等（int 0x80 门 IF=0） */
		current->state = TASK_UNINTERRUPTIBLE;
		tty->reader = current;
		schedule();
	}

	while (i < n && q->tail != q->head) {
		char c = q->buf[q->tail];
		q->tail = (q->tail + 1) % TTY_BUF_SIZE;
		buf[i++] = c;
		if (c == '\n')
			break;
	}
	if (q->tail == q->head)
		tty->line_complete = 0;		/* 行取完了 */
	return i;
}

/*
 * keyboard.c 的 kbd_getchar 由 tty_read 取代——键盘中断的
 * kbd_put 后改为调用 tty_do_char（见 keyboard.c 的修改）
 */
