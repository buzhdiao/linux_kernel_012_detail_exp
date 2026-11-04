/*
 * main.c —— 5P4 实验：termios 标志（ICANON/ECHO）+ 方向键
 * 对应书中 5.4 节"修改终端状态"（方向键 / 控制台标志位）
 *
 * 演示流程（读者任务）：
 *   1. 规范模式读一行（5P2 行为）
 *   2. ioctl 关 ICANON → 原始模式：每字符即交付（vim/密码输入的模式）
 *   3. ioctl 关 ECHO → 输入不回显（密码模式），再开回来
 *   方向键（E0 前缀扫描码）在内核里直接移动屏幕光标
 */

#include "kernel.h"
#include "sched.h"

#define EXT_MEM_K (*(unsigned short *)0x90002)

extern void trap_init(void);
extern long mem_init(long start, long end);

static inline int sys_call(int nr, int arg, int arg2, int arg3)
{
	int ret;
	__asm__ volatile("int $0x80" : "=a"(ret) : "a"(nr), "b"(arg), "c"(arg2), "d"(arg3));
	return ret;
}

static inline int fork_u(void)
{
	int ret;
	__asm__ volatile("int $0x80" : "=a"(ret) : "a"(2));
	return ret;
}

volatile int spin_sink;
static void spin(void)
{
	for (spin_sink = 0; spin_sink < 4000000; spin_sink++)
		;
}

void main(void)
{
	long memory_end;

	memory_end = (1 << 20) + (EXT_MEM_K << 10);
	if (memory_end > 16 * 1024 * 1024)
		memory_end = 16 * 1024 * 1024;
	mem_init(1 * 1024 * 1024, memory_end);

	trap_init();
	con_init();
	sched_init();

	printk("\n[6P3] hd driver demo\n\r");
	__asm__ volatile("sti");

	/* 块设备驱动用"睡眠等中断完成"模型，必须在开中断之后跑 */
	hd_init();		/* 输出已精简，详见 6P1-6P4 */
	floppy_init();
	floppy_demo();

	/* 7.1~7.4：挂载根文件系统 + 目录操作（写路径） */
	{
		extern void mount_root(void);
		extern void dir_demo(void);
		extern void link_demo(void);
		extern void posix_demo(void);
		extern void process_demo(void);
		mount_root();
		posix_demo();
		process_demo();
	}

	printk("\n[8P2] done\n\r");

	for (;;) {
		sys_call(0, (int)"[uptime] alive", 0, 0);
		sys_call(7, 100, 0, 0);
	}
}

/* 8.1 演示：getpid/uptime/sysinfo */
static long info[3];
void posix_demo(void)
{
	int pid = sys_call(13, 0, 0, 0);
	int up;

	sys_call(15, (int)info, 0, 0);
	printk("[posix] getpid=%d\n\r", pid);
	up = sys_call(14, 0, 0, 0);
	printk("[posix] uptime=%ds\n\r", up);
	printk("[posix] sysinfo: free_pages=%d tasks=%d/%d\n\r",
	       info[0], info[1], info[2]);
}

/* 8.2 演示：fork 子进程 → 子 exit(42) → 父 wait 收尸；管道 */
void child_exit_task(void)
{
	sys_call(0, (int)"[child] I will exit with code 42", 0, 0);
	sys_call(16, 42, 0, 0);			/* exit(42)：永不返回 */
}

void process_demo(void)
{
	/* ★ status 用静态存储：内核态发起的 int 0x80，CPU/中断桩的现场帧
	 * 就压在调用者 SP 下方的保留区里，栈上局部变量的地址会与桩帧的
	 * pt_regs 槽位重叠（实测 &status 正好落在 eax 槽），wait 写入的
	 * 退出码随即被返回值覆盖。用户态调用无此问题（帧在用户栈）。
	 * 曾踩坑：exit_code 打印恒为 0 */
	static int status = 0;
	int pid;

	pid = fork_u();
	if (pid == 0) {
		child_exit_task();
	}
	{
		int got = 0;
		while (!got) {
			got = sys_call(17, (int)&status, 0, 0);
			sys_call(7, 10, 0, 0);
		}
		printk("[parent] child pid=%d exit_code=%d\n\r", got, status);
	}
	{
		extern int sys_pipe_write(const char *, int);
		sys_pipe_write("via pipe", 8);
	}
}
