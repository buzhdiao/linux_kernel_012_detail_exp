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
	case 6: {			/* getchar：读键盘缓冲（见 keyboard.c） */
		extern int kbd_getchar(void);
		regs->eax = kbd_getchar();
		break;
	}
	case 7: {			/* idle：hlt 睡 n 个时钟滴答
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
