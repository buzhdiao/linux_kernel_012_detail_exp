/*
 * user_hello.c —— 第一个从文件系统加载执行的用户程序
 * 编译（宿主机）：gcc -m32 -ffreestanding -fno-pic -fno-stack-protector -c user_hello.c
 *                ld -m elf_i386 -Ttext=0 -s --oformat binary -o user_hello.bin user_hello.o
 * 入口即镜像偏移 0（execve 把它放进任务空间的 0 处，do_execve 的 tss.eip=0）。
 * ★ flat binary 没有 ELF 头可解析，入口就是 .text 的第一个字节——所以 _start
 *   必须是 .text 的第一个函数（曾踩坑：_start 放在文件末尾，偏移 0 处变成
 *   sys_print，exec 一进来就按调用惯例从栈上读参数，读到未映射页直接缺页）。
 *   实现顺序 = 输出顺序：先声明内联助手 → 定义 _start → 再给出助手函数体。
 * 输出用 int 0x80（nr=0 打印 EBX 字符串；nr=7 idle）——系统调用约定同前几章
 */
typedef unsigned int u32;

static inline void sys_print(const char *s);
static inline void sys_idle(int n);

void _start(void)
{
	sys_print("[user] hello from an exec'd program!\n\r");
	for (;;) {
		sys_idle(100);
		sys_print("[user] still alive\n\r");
	}
}

static inline void sys_print(const char *s)
{
	__asm__ volatile("int $0x80" :: "a"(0), "b"((int)s));
}

static inline void sys_idle(int n)
{
	__asm__ volatile("int $0x80" :: "a"(7), "b"(n));
}
