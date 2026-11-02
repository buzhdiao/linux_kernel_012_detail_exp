/*
 * keyboard.c —— 键盘驱动：扫描码解析 + 环形缓冲 + 回显
 * 对应书中 5.1 节"支持键盘"，来源：官方仓 kernel/chr_drv/kboard.c（教学精简版）
 *
 * 键盘工作原理：
 *   - 按键/松键时，键盘控制器（8042）通过 IRQ1 通知 CPU，扫描码可从
 *     端口 0x60 读出
 *   - 第 1 套扫描码（Set 1）：按下 = 通码（make），松开 = 断码（break，
 *     通码 | 0x80）。本实验只处理通码，断码读掉丢弃
 *   - 通码 0x2A/0x36 = 左/右 Shift 按下，0xAA/0xB6 = 松开——影响后续
 *     字符的大小写映射
 *   - 通码 ≥ 0x80 的其余值（扩展键前缀 E0 等）本实验直接忽略
 *
 * 处理流程：IRQ1 → keyboard_interrupt 桩（asm.S）→ kbd_interrupt（本文件）
 *   读扫描码 → 查表转 ASCII → 入环形缓冲 → 回显到屏幕（中断上下文直接
 *   con_putc——0.11 是先进 tty 写队列再由定时器刷新，第 5.2 节再引入队列）
 */

#include "kernel.h"
#include "sched.h"

/* 端口：0x60 数据口，0x64 状态口（bit1=1 表示输出缓冲还有数据没读） */
#define KBD_DATA   0x60
#define KBD_STATUS 0x64

/* 通码 → 不经 Shift 的 ASCII 映射（只收录可打印字符和回车退格） */
static const char keymap_normal[58] = {
	0,   27,  '1', '2', '3', '4', '5', '6', '7', '8',	/* 00-09 */
	'9', '0', '-', '=', '\b', '\t', 'q', 'w', 'e', 'r',	/* 0A-13 */
	't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,	/* 14-1D enter, ctrl */
	'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	/* 1E-27 */
	'\'', '`', 0, '\\', 'z', 'x', 'c', 'v', 'b', 'n',	/* 28-31 lshift */
	'm', ',', '.', '/', 0, '*', 0, ' '			/* 32-39 rshift, alt, sp */
};

/* 通码 → 按住 Shift 的 ASCII 映射 */
static const char keymap_shift[58] = {
	0,   27,  '!', '@', '#', '$', '%', '^', '&', '*',
	'(', ')', '_', '+', '\b', '\t', 'Q', 'W', 'E', 'R',
	'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,
	'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
	'"', '~', 0, '|', 'Z', 'X', 'C', 'V', 'B', 'N',
	'M', '<', '>', '?', 0, '*', 0, ' '
};

/* Shift 状态（本实验不区分左右 Shift） */
static int shift_down;

/* ---------- 环形缓冲区：中断生产，syscall 消费 ---------- */
#define KBD_BUF_SIZE 32
static char kbd_buf[KBD_BUF_SIZE];
static int kbd_head, kbd_tail;		/* head=写位置，tail=读位置 */

static void kbd_put(char c)
{
	int next = (kbd_head + 1) % KBD_BUF_SIZE;

	if (next == kbd_tail)		/* 缓冲满：丢弃（真实内核会 beep） */
		return;
	kbd_buf[kbd_head] = c;
	kbd_head = next;
}

/*
 * kbd_interrupt —— IRQ1 的 C 侧处理（由 asm.S 的桩调用）
 */
void kbd_interrupt(void)
{
	unsigned char sc;

	sc = inb(KBD_DATA);		/* 读扫描码（读走即清控制器缓冲） */

	if (sc & 0x80) {		/* 断码（松键）：只更新 Shift 状态 */
		sc &= 0x7f;
		if (sc == 0x2a || sc == 0x36)
			shift_down = 0;
		return;
	}

	/* 通码（按下） */
	if (sc == 0x2a || sc == 0x36) {	/* Shift 按下 */
		shift_down = 1;
		return;
	}
	if (sc == 0x38 || sc == 0x1d)	/* Alt/Ctrl：本实验忽略 */
		return;
	if (sc >= 58)			/* 超出映射表（F 键/方向键/E0 前缀等） */
		return;

	{
		char c = shift_down ? keymap_shift[sc] : keymap_normal[sc];
		if (!c)
			return;
		kbd_put(c);
		con_putc(c);			/* 回显（中断上下文直写控制台） */
		tty_do_char(c);			/* 交给 tty 层攒行/唤醒读者 */
	}
}

/* kbd_init —— 解除 IRQ1 屏蔽（IRQ1 = IMR bit1） */
void kbd_init(void)
{
	outb(inb_p(0x21) & ~0x02, 0x21);
}
