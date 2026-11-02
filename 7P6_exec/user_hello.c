/*
 * user_hello.c —— 第一个从文件系统加载执行的用户程序
 * 编译（宿主机）：gcc -m32 -nostdlib -static -Ttext=0 -o user_hello.elf user_hello.c
 *                objcopy -O binary user_hello.elf user_hello.bin
 * 入口即 ELF 的 text 起始（链接到线性 0，execve 把它放进任务空间的 0 处）
 * 输出用 int 0x80（nr=0 打印 EBX 字符串；nr=7 idle）——系统调用约定同前几章
 */
typedef unsigned int u32;

static inline void sys_print(const char *s)
{
	__asm__ volatile("int $0x80" :: "a"(0), "b"((int)s));
}

static inline void sys_idle(int n)
{
	__asm__ volatile("int $0x80" :: "a"(7), "b"(n));
}

void _start(void)
{
	sys_print("[user] hello from an exec'd program!\n\r");
	for (;;) {
		sys_idle(100);
		sys_print("[user] still alive\n\r");
	}
}
