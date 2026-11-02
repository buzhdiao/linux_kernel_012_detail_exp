/*
 * syscall.c —— 系统调用分发器（4P3 版）
 * 对应书中 4.3 节"系统调用和中断"：int 0x80 → system_call 桩 →
 * 按调用号分发。官方仓的 sys_call_table 函数表在 4P2 的 README 里
 * 讨论过，本实验任务少先用 if 链，第 8 章换成真正的分发表
 *
 * 调用约定：EAX = 调用号，EBX = 参数；返回值写回 pt_regs->eax
 */

#include "kernel.h"
#include "sched.h"

/* fork.c */
extern int sys_fork(struct pt_regs *regs);

void sys_dispatch(int nr, struct pt_regs *regs)
{
	switch (nr) {
	case 2:				/* fork：无参数，返回值写 eax */
		regs->eax = sys_fork(regs);
		break;
	case 4:				/* 打印"调用者看到的 cow_var 值"：
					 * EBX 是用户代码用自己线性视图读出的值，
					 * 父子传来的值不同即证明写时复制生效 */
		printk("task %d: cow_var = %d\n\r", current->pid, regs->ebx);
		break;
	case 5:				/* 打印 fork 的返回值 */
		printk("task %d: fork() returned %d\n\r", current->pid, regs->ebx);
		break;
	case 6: {			/* read(0, buf, n)：tty 规范模式读一行 */
		extern int tty_read(char *, int);
		regs->eax = tty_read((char *)regs->ebx, (int)regs->ecx);
		break;
	}
	case 8: {			/* write(1, buf, n)：用户输出路径 */
		const char *p = (const char *)regs->ebx;
		int n = (int)regs->ecx, i;
		for (i = 0; i < n && p[i]; i++)
			con_putc(p[i]);
		regs->eax = i;
		break;
	}
	case 10: {			/* open(path, mode) */
		extern int file_open(const char *, int);
		regs->eax = file_open((const char *)regs->ebx, (int)regs->ecx);
		break;
	}
	case 11: {			/* read(fd, buf, n) */
		extern int file_read(int, char *, int);
		regs->eax = file_read((int)regs->ebx, (char *)regs->ecx, (int)regs->edx);
		break;
	}
	case 12: {			/* close(fd) */
		extern int file_close(int);
		regs->eax = file_close((int)regs->ebx);
		break;
	}
	case 9: {			/* ioctl(0, cmd)：修改终端状态 */
		extern void tty_ioctl(int);
		tty_ioctl((int)regs->ebx);
		break;
	}
	case 7: {			/* idle：hlt 睡 n 个时钟滴答（对内也叫 sys_idle_ticks）
					 * （忙转会饿死 qemu TCG 的事件循环；
					 *   真实内核的 idle 也是 hlt） */
		int n = (int)regs->ebx;
		while (n--) {
			/* hlt 被中断唤醒后 IF 已被中断门清零，必须重开再睡 */
			__asm__ volatile("sti; hlt");
		}
		break;
	}
	default:			/* 其余：直接打印字符串参数 */
		printk("task %d: %s\n\r", current->pid, (char *)regs->ebx);
		break;
	}
}

/* floppy.c 等驱动用的睡眠原语：hlt n 个时钟滴答 */
void sys_idle_ticks(int n)
{
	while (n--) {
		__asm__ volatile("sti; hlt");
	}
}

/* 7.3 演示辅助：打印用户态读到的文件内容（控制字符转义显示） */
void print_file_content(const char *s, int n)
{
	printk("[file] read %d bytes: ", n);
	while (n--) {
		char c = *s++;
		if (c == '\n')
			printk("\\n");
		else if (c >= 32 && c < 127)
			con_putc(c);
	}
	printk("\n\r");
}
