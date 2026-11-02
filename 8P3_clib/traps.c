/*
 * traps.c —— CPU 异常处理 + IDT 门的设置
 * 对应书中 3.3.3 "处理CPU异常"
 *
 * head.S 已经把 256 项 IDT 全部指向 ignore_int（什么都不做的兜底），
 * 本文件用 set_gate 把关键向量改挂到真正的处理桩上：
 *   0x00 divide_error  除零错误
 *   0x06 invalid_op    非法操作码
 *   0x80 system_call   系统调用（DPL=3，用户态可用 int 0x80 进入）
 *
 * set_gate 的做法与官方仓 kernel/traps.c 的 _set_gate 一致：
 * 直接按 8 字节门描述符的内存布局逐字段写入 head.S 的 idt[] 数组
 */

#include "kernel.h"

/*
 * head.S 中的 IDT 数组（.globl idt）。
 * 必须声明成数组：extern void *idt 会把 idt 当"指针变量"，
 * 读到的是 IDT 头 8 个字节的值而不是表地址，set_gate 就写飞了
 */
extern unsigned long idt[];

/* 汇编桩（asm.S） */
extern void divide_error(void);
extern void invalid_op(void);
extern void system_call(void);
extern void timer_interrupt(void);
extern void page_fault(void);
extern void keyboard_interrupt(void);
extern void hd_interrupt(void);
extern void floppy_interrupt(void);

/*
 * set_gate —— 往 IDT 的第 vector 项写一个门描述符
 * type       0xE = 中断门（进入自动关中断），0xF = 陷阱门（不关中断）
 * dpl        特权级：异常/中断用 0（内核级）；系统调用门用 3（用户可 int）
 * handler    处理程序入口（选择子固定用内核代码段 0x08）
 */
#define set_gate(vector, type, dpl, handler) \
do { \
	unsigned long h = (unsigned long)handler; \
	unsigned long *p = idt + vector * 2;	/* 每项 8 字节 = 2 个 long */ \
	/* 低 4 字节：偏移低 16 位 | 选择子(0x08) << 16 */ \
	p[0] = ((0x08 << 16) & 0xffff0000) | (h & 0xffff); \
	/* 高 4 字节：偏移高 16 位 | P=1,DPL,TYPE 字节 */ \
	p[1] = (h & 0xffff0000) | ((0x80 | (dpl << 5) | type) << 8); \
} while (0)

/*
 * die —— 通用异常现场打印
 * str   异常名称
 * esp   指向汇编桩压栈后的现场数组，布局（下标从栈顶往高地址数）：
 *       [0..9]=EBP,EDI,ESI,EBX,EDX,ECX,EAX,FS,ES,DS
 *       [10]=错误码（无错误码异常由桩补 0），[11]=EIP,[12]=CS,[13]=EFLAGS
 * nr    异常向量号
 *
 * 注意：除零是 fault，iret 会回到出错指令本身再触发一次，
 * 所以本实验打印完现场直接停机，避免无限循环（真实内核会
 * 修复现场或终止出错的进程）
 */
static void die(const char *str, long *esp, int nr)
{
	int i;

	printk("%s: 0x%02x\n\r", str, nr);
	printk("error code: 0x%lx, EIP: 0x%08lx, CS: 0x%lx, EFLAGS: 0x%08lx\n\r",
	       esp[10], esp[11], esp[12], esp[13]);
	printk("saved registers:");
	for (i = 0; i < 10; i++)
		printk(" %lx", esp[i]);
	printk("\n\r");
	printk("system halted.\n\r");
	__asm__ volatile("cli; hlt");
	for (;;)
		;
}

/* 除零错误：0 号异常，无错误码（桩补了 0） */
void do_divide_error(long *esp)
{
	die("divide error", esp, 0);
}

/* 非法操作码：6 号异常，无错误码 */
void do_invalid_op(long *esp)
{
	die("invalid opcode", esp, 6);
}

/*
 * trap_init —— 挂接异常门
 * 在 head.S 的 ignore_int 兜底之上，把教学需要的最小集合挂上
 */
void trap_init(void)
{
	set_gate(0x00, 0xe, 0, divide_error);
	set_gate(0x06, 0xe, 0, invalid_op);
	/* 系统调用门：类型 0xE、DPL=3 —— 用户态程序 int 0x80 不触发保护异常 */
	set_gate(0x80, 0xe, 3, system_call);
	/* 时钟中断门：IRQ0 → 0x20，时间片轮转的驱动力 */
	set_gate(0x20, 0xe, 0, timer_interrupt);
	/* 缺页门：14 号异常（有硬件错误码），写时复制的入口 */
	set_gate(0x0e, 0xe, 0, page_fault);
	/* 键盘中断门：IRQ1 → 0x21 */
	set_gate(0x21, 0xe, 0, keyboard_interrupt);
	/* 硬盘中断门：IRQ14 → 0x2e（从片级联） */
	set_gate(0x2e, 0xe, 0, hd_interrupt);
	/* 软盘中断门：IRQ6 → 0x26 */
	set_gate(0x26, 0xe, 0, floppy_interrupt);
}
