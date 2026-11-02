/*
 * syscall.c —— 系统调用的最小演示（4P2 版）
 *
 * 4P1 只验证了"能进能出"；本实验给两个用户进程一个共同的打印通道：
 * sys_print(编号, 字符串) —— 内核 printk 把用户态字符串打出来。
 * 字符串指针直接解引用之所以可行：恒等映射下用户段基址 0，
 * 内核能直接访问用户数据（真正的 verify_area 检查在第 5 章 read/write 引入）
 */

#include "kernel.h"

void sys_print(int nr, const char *s)
{
	printk("task %d: %s\n\r", nr, s);
}
