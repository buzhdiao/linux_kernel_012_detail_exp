/*
 * 内核链接脚本：保证 head.o 的代码从地址 0x0 开始
 *
 * 不用它的话，gcc -O2 会把只调用一次的函数放进 .text.startup 段，
 * 链接器默认脚本会把它排在普通 .text 之前——main.o 的代码就会覆盖
 * head.S 的 startup_32 入口。本脚本让所有代码段按命令行顺序合并，
 * head.o 在前，startup_32 稳稳落在 0x0。
 */

SECTIONS
{
	. = 0x0;
	.text : {
		*(.text .text.*)
	}
	.data : {
		*(.data .data.*)
	}
	/DISCARD/ : {
		*(.comment)
		*(.note*)
		*(.eh_frame*)
	}
}
