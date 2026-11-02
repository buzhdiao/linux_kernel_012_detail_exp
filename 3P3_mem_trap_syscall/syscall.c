/*
 * syscall.c —— 系统调用的最小演示
 * 对应书中 3.3.2 "初始化系统调用"
 *
 * 完整的系统调用框架（sys_call_table 分发表、参数传递、errno、
 * 信号检查、调度点）在官方仓 kernel/sys_call.S + kernel/sys.c，
 * 第 4 章实现 fork 时逐步长成。本实验只验证：
 *   int 0x80 → IDT 0x80 号门 → system_call 桩 → 进入 C 函数
 */

#include "kernel.h"

/* 系统调用桩调用的 C 函数：打印拿到的调用号 */
void sys_demo(int nr)
{
	printk("system call entered, nr = %d\n\r", nr);
}
