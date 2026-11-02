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
	case 13: {			/* getpid */
		regs->eax = current->pid;
		break;
	}
	case 14: {			/* uptime（秒） */
		extern long sys_uptime(void);
		regs->eax = sys_uptime();
		break;
	}
	case 15: {			/* sysinfo(EBX=用户数组) */
		extern void sys_sysinfo(long *);
		long info[3];
		int i;
		sys_sysinfo(info);
		for (i = 0; i < 3; i++)
			((long *)regs->ebx)[i] = info[i];
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

/* =====================================================================
 * 8.1 补充：常用 POSIX 系统调用
 * 对应书中 8.1 节"POSIX接口"（时间管理 / 挂载文件系统等）
 * getpid(20) time(13) sysinfo(自定) —— 展示系统调用的批量扩展方式
 * ===================================================================== */

/* 内核启动时间基准（秒）：jiffies/HZ 从开机累计 */
long sys_uptime(void)
{
	extern long volatile jiffies;
	return jiffies / 100;		/* HZ=100：滴答换秒 */
}

/* 简化的系统信息：空闲页数/任务数 */
void sys_sysinfo(long out[3])
{
	extern long count_free_pages(void);
	int i, tasks = 0;
	long free;

	free = count_free_pages();
	for (i = 0; i < NR_TASKS; i++)
		if (task[i])
			tasks++;
	out[0] = free;			/* 空闲页 */
	out[1] = tasks;			/* 任务数 */
	out[2] = NR_TASKS;		/* 任务槽总数 */
}

/* =====================================================================
 * 8.3 补充：用户态 C 库的内核侧支撑
 * 对应书中 8.3 节：库的 printf/write 依赖的 write 系统调用已就位
 * （nr=8），libc 的 getpid/uptime/exit 也都是前几章实现的系统调用——
 * 库本身就是"系统调用层的用户态封装"，内核侧无需新增功能。
 * ===================================================================== */
