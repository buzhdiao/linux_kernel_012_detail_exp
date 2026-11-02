/*
 * tty.h —— 终端队列与 tty_struct 定义
 * 对应书中 5.2 节，来源：官方仓 include/linux/tty.h（精简版）
 */

#ifndef _TTY_H
#define _TTY_H

#define TTY_BUF_SIZE 64

/* 环形字符队列：tty 层的基本数据结构（键盘缓冲/读攒行/写队列共用） */
struct tty_queue {
	unsigned long head;		/* 写位置 */
	unsigned long tail;		/* 读位置 */
	char buf[TTY_BUF_SIZE];
};

/*
 * tty_struct —— 一个终端的完整状态
 * secondary：规范模式攒行的队列（键盘中断写入，read 取走）
 * reader：睡眠在 read 上的任务（中断来数据时唤醒）
 * line_complete：secondary 里有一整行（以 \n 结尾）待读
 */
struct tty_struct {
	struct tty_queue secondary;
	struct tty_queue reserved;	/* 占位：5.3 的写队列/读原始队列 */
	int line_complete;
	struct task_struct *reader;
};

extern struct tty_struct tty_table[1];

void tty_do_char(char c);
int tty_read(char *buf, int n);

#endif
