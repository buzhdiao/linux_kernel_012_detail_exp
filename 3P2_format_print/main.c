/*
 * main.c —— 3P2 实验：验证 printk 格式化打印
 *
 * 在 3P1 的基础上（分页已开启），main() 先初始化控制台，
 * 再用 printk 打印各种格式验证 vsprintf，最后打印 30 行验证滚屏
 */

#include "kernel.h"

void main(void)
{
	int i;

	con_init();

	printk("\n[3P2] format print works\n\r");
	printk("dec : %d %d %d\n\r", 0, 123, -456);
	printk("hex : 0x%x 0x%08x\n\r", 0xdeadbeef, 42);
	printk("str : %s, char: %c%c%c\n\r", "hello kernel", 'L', 'X', '!');
	printk("width: [%8d][%-8d][%08d]\n\r", 42, 42, 42);
	printk("ptr : %p\n\r", (void *)0xb8000);

	/* 打印 30 行触发滚屏：前几行（含开机信息）会被顶出屏幕 */
	for (i = 0; i < 30; i++)
		printk("scroll test line %2d: "
		       "AAAAAAAAAA BBBBBBBBBB CCCCCCCCCC DDDDDDDDDD\n\r", i);

	printk("done, halt here.\n\r");

	__asm__ volatile("hlt");
	for (;;)
		;
}
