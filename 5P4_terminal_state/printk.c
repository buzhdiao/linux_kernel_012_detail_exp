/*
 * printk.c —— 内核格式化打印接口
 * 对应书中 3.2.4：printk = vsprintf 格式化 + console 输出
 *
 * 从此内核有了像 printf 一样的输出能力，后面所有章节的调试信息
 * 都靠它打印。官方仓的 printk 位于 kernel/printk.c。
 */

#include "kernel.h"
#include <stdarg.h>

/* 格式化结果的临时缓冲区：一次 printk 最长 1024 字符 */
static char buf[1024];

int printk(const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i = vsprintf(buf, fmt, args);
	va_end(args);

	con_puts(buf);
	return i;
}
