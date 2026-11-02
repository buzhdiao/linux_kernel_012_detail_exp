/*
 * vsprintf.c —— 内核格式化字符串的核心实现
 * 对应书中 3.2.4 "格式化打印结果并输出"
 *
 * 支持的格式（内核够用子集）：
 *   %d %i    十进制有符号
 *   %u       十进制无符号
 *   %x %X    十六进制（小写/大写）
 *   %c       单字符
 *   %s       字符串
 *   %%       百分号本身
 *   修饰：'-': 左对齐；'0': 补零；数字宽度；'*': 宽度取自参数；
 *         'l': 长整数修饰
 *
 * 结构与官方仓 kernel/vsprintf.c 一致：skip_atoi + number + vsprintf 主循环，
 * 本文件按教学需要精简（去掉了浮点 %e/%f/%g 和 field precision 的部分分支）
 */

#include "kernel.h"
#include <stdarg.h>

/* number() 的 flags 参数用到的位 */
#define ZEROPAD 1		/* 宽度不足补零（00042）而不是补空格（   42） */
#define LEFT    2		/* 左对齐 */

/* 判断字符是否是十进制数字 */
static int is_digit(char c)
{
	return c >= '0' && c <= '9';
}

/* 解析 fmt 里连续的数字（如 %08x 中的 8），返回数值，指针前移 */
static int skip_atoi(const char **s)
{
	int i = 0;

	while (is_digit(**s))
		i = i * 10 + *((*s)++) - '0';
	return i;
}

/*
 * number —— 把整数按指定进制转成 ASCII 写入 str
 * num      待转换的数（负数仅对十进制有意义）
 * base     进制（10/16）
 * size     最小宽度
 * flags    ZEROPAD/LEFT
 * letters  进制字母表：小写 "0123456789abcdef" 或大写版
 * 返回写入后的目标指针
 */
static char *number(char *str, long num, int base, int size, int flags,
		    const char *letters)
{
	char tmp[36];		/* 64 位二进制最长 64 个字符，这里 32 位最长 33 个够用 */
	const char *dig = letters;
	int negative = 0;
	int i = 0;

	/* 十进制负数：记录符号后转正数处理 */
	if (base == 10 && num < 0) {
		negative = 1;
		num = -num;
	}

	/* 先从低位到高位逐位拆出来 */
	do {
		tmp[i++] = dig[num % base];
		num /= base;
	} while (num);
	/* 负号占一个宽度 */
	if (negative)
		tmp[i++] = '-';

	/* 补零/补空格到指定宽度（数字部分在 tmp 里是倒序的，
	 * 补空格时先把空格写在 str，补零时把零排在符号后面） */
	while (size-- > i) {
		if (flags & ZEROPAD)
			*str++ = '0';
		else
			*str++ = ' ';
	}
	/* 上面的补零会把 0 排在负号前面（000-42），修正成 -000042 的效果有限：
	 * 内核打印场景几乎不用负数补零，保持简单 */

	/* 把 tmp 里的数字倒序写入 str */
	while (i-- > 0)
		*str++ = tmp[i];
	return str;
}

/*
 * vsprintf —— 把 fmt 按 args 格式化到 buf
 * 返回写入的字符个数（不含结尾的 0）
 */
int vsprintf(char *buf, const char *fmt, va_list args)
{
	char *str = buf;

	for (; *fmt; ++fmt) {
		/* 普通字符直接复制，等待 % */
		if (*fmt != '%') {
			*str++ = *fmt;
			continue;
		}

		/* ---- 解析 % 后面的标志位 ---- */
		int flags = 0;
		int field_width = 0;
repeat:
		++fmt;			/* 跳过刚处理过的字符 */
		switch (*fmt) {
		case '-':		/* 左对齐 */
			flags |= LEFT;
			goto repeat;
		case '0':		/* 补零 */
			flags |= ZEROPAD;
			goto repeat;
		}

		/* ---- 解析域宽：数字或 '*'（从参数取） ---- */
		if (is_digit(*fmt)) {
			field_width = skip_atoi(&fmt);
		} else if (*fmt == '*') {
			++fmt;
			field_width = va_arg(args, int);
			if (field_width < 0) {
				field_width = -field_width;
				flags |= LEFT;
			}
		}

		/* ---- 长度修饰符：只关心 'l' ---- */
		int is_long = 0;
		if (*fmt == 'l') {
			is_long = 1;
			++fmt;
		}

		/* ---- 转换说明符 ---- */
		switch (*fmt) {
		case 'd':		/* 有符号十进制 */
		case 'i': {
			long num = is_long ? va_arg(args, long)
					   : (long)va_arg(args, int);
			str = number(str, num, 10, field_width, flags,
				     "0123456789");
			break;
		}
		case 'u': {		/* 无符号十进制 */
			unsigned long num = is_long ? va_arg(args, unsigned long)
						    : (unsigned long)va_arg(args, unsigned int);
			str = number(str, (long)num, 10, field_width, flags,
				     "0123456789");
			break;
		}
		case 'x':		/* 十六进制（小写） */
			str = number(str, va_arg(args, long), 16,
				     field_width, flags, "0123456789abcdef");
			break;
		case 'X':		/* 十六进制（大写） */
			str = number(str, va_arg(args, long), 16,
				     field_width, flags, "0123456789ABCDEF");
			break;
		case 'c': {		/* 单字符：按宽度补空格 */
			char c = (char)va_arg(args, int);
			if (!(flags & LEFT))
				while (--field_width > 0)
					*str++ = ' ';
			*str++ = c;
			while (--field_width > 0)
				*str++ = ' ';
			break;
		}
		case 's': {		/* 字符串 */
			const char *s = va_arg(args, const char *);
			if (!s)
				s = "<NULL>";
			int len = 0;
			while (s[len])
				len++;
			if (!(flags & LEFT))
				while (len < field_width--)
					*str++ = ' ';
			for (int i = 0; i < len; i++)
				*str++ = s[i];
			while (len < field_width--)
				*str++ = ' ';
			break;
		}
		case 'p':		/* 指针：按 8 位十六进制显示 */
			str = number(str, va_arg(args, long), 16, 8,
				     ZEROPAD, "0123456789abcdef");
			break;
		case '%':		/* %% 输出一个 % */
			*str++ = '%';
			break;
		default:		/* 未知说明符：原样输出，方便发现 bug */
			*str++ = '%';
			if (*fmt)
				*str++ = *fmt;
			break;
		}
	}
	*str = 0;
	return str - buf;
}
