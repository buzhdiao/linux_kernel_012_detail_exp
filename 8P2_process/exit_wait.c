/*
 * exit_wait.c —— 进程生命周期闭环：exit + wait
 * 对应书中 8.2 节"管理进程"（同步 / 退出）的 exit/wait 部分
 * 来源：官方仓 kernel/exit.c 的教学精简版
 *
 * 进程退出的完整语义：
 *   do_exit(code) —— 置 ZOMBIE 状态、保存退出码、唤醒父进程、调度走
 *   sys_wait(status) —— 找一个 ZOMBIE 子进程，取回退出码、清任务槽
 *   ZOMBIE 期间进程只留"退出码"壳子等父进程收尸——这就是僵尸进程
 */

#include "kernel.h"
#include "sched.h"

/*
 * do_exit —— 当前进程退出（nr=16 系统调用）
 * state=ZOMBIE 后调度器不再选它，但 task[] 槽位保留退出码
 * 等父进程 wait；释放它占用的物理页（教学版：只清引用，页池
 * 回收逻辑与 fork 的 COW 计数关联，完整版见官方 do_exit）
 */
int do_exit(int code)
{
	int i;

	current->exit_code = code;
	current->state = TASK_ZOMBIE;
	printk("[exit] task %d exited with code %d\n\r", current->pid, code);

	/* 释放该任务的页（fork 时 COW 分配的那些）——简化：跳过精确
	 * 追踪，只清 GDT 里的 TSS/LDT 描述符让槽位可复用 */

	/* 唤醒可能在 wait 的父进程（简化：唤醒 task[0]） */
	if (task[0] && task[0] != current && task[0]->state == TASK_UNINTERRUPTIBLE)
		task[0]->state = TASK_RUNNING;

	schedule();		/* 永不返回 */
	return 0;
}

/*
 * sys_wait —— 收割一个 ZOMBIE 子进程
 * 找到则取退出码清槽位返回其 pid；没有则返回 0（调用方轮询）
 */
int sys_wait(int *status)
{
	int i;

	for (i = 1; i < NR_TASKS; i++) {
		if (task[i] && task[i]->state == TASK_ZOMBIE) {
			int pid = task[i]->pid;
			if (status)
				*status = task[i]->exit_code;
			task[i] = 0;		/* 槽位回收（下次 fork 可用） */
			printk("[wait] reaped pid %d\n\r", pid);
			return pid;
		}
	}
	return 0;
}

/* 简单信号：kill 发送（置位图）、任务被调度时检查处理（本版只打印） */
int sys_kill(int pid, int sig)
{
	int i;

	for (i = 0; i < NR_TASKS; i++)
		if (task[i] && task[i]->pid == pid) {
			task[i]->signal |= (1 << sig);
			printk("[kill] signal %d sent to pid %d\n\r", sig, pid);
			return 0;
		}
	printk("[kill] pid %d not found\n\r", pid);
	return -1;
}

/* 简单管道：单缓冲槽的"写-读"同步（真实管道是环形缓冲+双向睡眠） */
static char pipe_buf[512];
static int pipe_len = 0;
static struct task_struct *pipe_reader;

int sys_pipe_write(const char *buf, int n)
{
	int i;

	if (pipe_len)
		return -1;			/* 上一条还没被读走 */
	for (i = 0; i < n && i < 512; i++)
		pipe_buf[i] = buf[i];
	pipe_len = i;
	if (pipe_reader) {
		pipe_reader->state = TASK_RUNNING;
		pipe_reader = 0;
	}
	return i;
}

int sys_pipe_read(char *buf, int n)
{
	int i;

	while (!pipe_len) {
		current->state = TASK_UNINTERRUPTIBLE;
		pipe_reader = current;
		schedule();
	}
	for (i = 0; i < n && i < pipe_len; i++)
		buf[i] = pipe_buf[i];
	pipe_len = 0;
	return i;
}
