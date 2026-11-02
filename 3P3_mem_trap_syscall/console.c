/*
 * console.c —— VGA 文本模式控制台
 * 对应书中 3.2 节：初始化终端、操作显示控制器、支持换行回车
 *
 * 简化说明（与官方仓 kernel/chr_drv/console.c 的差异）：
 *   - 本实验只做"内核打印"所需的最小功能：光标定位、字符输出、
 *     回车/换行/制表符、行尾自动换行、整屏上滚
 *   - tty 队列、终端转义序列（ANSI escape）、键盘回显等特性属于第 5 章，
 *     官方版本的 console.c 是围绕 tty 写队列展开的，届时再引入
 *
 * 硬件知识：
 *   - VGA 文本模式显存在 0xB8000，25 行 * 80 列，每字符 2 字节：
 *     低字节是 ASCII，高字节是属性（高 4 位背景色，低 4 位前景色）
 *   - 光标位置由 6845 CRT 控制器的寄存器 14/15（索引口 0x3D4，数据口 0x3D5）
 *     控制，写入的是"字符单元序号"（从 0 数起的第几个格子，不是字节偏移）
 */

#include "kernel.h"

/* setup.S 开机时采集并保存到 0x90000 处的光标位置（存的是"列,行"） */
#define ORIG_X (*(u16 *)0x90000)
#define ORIG_Y (*(u16 *)0x90002)

#define COLS     80			/* 每行字符数 */
#define LINES    25			/* 屏幕行数 */
#define ROW_BYTES (COLS * 2)		/* 每行在显存中占的字节数 */
#define ATTR     0x07			/* 属性字节：黑底白字 */

/* 当前输出位置在显存中的字节偏移，范围 [0, COLS*LINES*2) */
static u32 pos;

static u16 *const video = (u16 *)0xb8000;

/* ---------- 操作显示控制器：把硬件光标移到 pos 对应的位置 ---------- */
static void set_cursor(void)
{
	u16 cell = pos / 2;		/* CRT 寄存器按"字符格子"计数，不是字节 */

	outb_p(14, 0x3d4);		/* 索引 14：光标位置高 8 位 */
	outb_p(cell >> 8, 0x3d5);
	outb_p(15, 0x3d4);		/* 索引 15：光标位置低 8 位 */
	outb_p(cell & 0xff, 0x3d5);
}

/* ---------- 整屏上滚一行：1~24 行上移，末行清空 ---------- */
static void scroll(void)
{
	int i;

	/* 把第 1~24 行搬到第 0~23 行（按 16 位字搬运正好是格子数） */
	for (i = 0; i < (LINES - 1) * COLS; i++)
		video[i] = video[i + COLS];
	/* 最后一行清成空白 */
	for (i = (LINES - 1) * COLS; i < LINES * COLS; i++)
		video[i] = ATTR << 8 | ' ';
	/* 输出位置回到最后一行的行首 */
	pos = (LINES - 1) * ROW_BYTES;
}

/* ---------- 换行：到下一行行首（含滚屏） ---------- */
static void lf(void)
{
	if (pos < (LINES - 1) * ROW_BYTES)
		pos += ROW_BYTES;
	else
		scroll();
}

/* ---------- 回车：回到本行行首 ---------- */
static void cr(void)
{
	pos -= pos % ROW_BYTES;
}

/*
 * con_putc —— 输出一个字符（内核打印的最终出口）
 * '\n' 换行、'\r' 回车、'\t' 跳到下一个 8 列对齐处、普通字符直接写显存
 */
void con_putc(char c)
{
	switch (c) {
	case '\n':			/* LF 换行 */
		lf();
		break;
	case '\r':			/* CR 回车 */
		cr();
		break;
	case '\t': {
		int col = (pos % ROW_BYTES) / 2;	/* 当前列号 */
		int n = 8 - col % 8;			/* 补到 8 列对齐 */
		while (n--)
			con_putc(' ');
		break;
	}
	default: {
		/* 普通字符：写入 ASCII + 属性，位置前移一格 */
		video[pos / 2] = ((u16)ATTR << 8) | (u8)c;
		pos += 2;
		/* 写到屏幕末尾：回车换行（会触发滚屏） */
		if (pos >= LINES * ROW_BYTES) {
			pos -= 2;
			cr();
			lf();
		}
		break;
	}
	}
	set_cursor();
}

/* con_puts —— 输出以 0 结尾的字符串 */
void con_puts(const char *s)
{
	while (*s)
		con_putc(*s++);
}

/*
 * con_init —— 控制台初始化
 * 从 setup.S 保存的光标位置接着往下打印（开机信息不会被清掉）
 */
void con_init(void)
{
	pos = (ORIG_Y * COLS + ORIG_X) * 2;
	set_cursor();
}
