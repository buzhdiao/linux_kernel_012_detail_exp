/*
 * lib/hello_lib.c —— 用 C 库写的用户程序（8.3 的目标形态）：
 * 与 7P6 的 user_hello.c 相比，程序不再直接碰 int 0x80，
 * 而是像普通 C 程序一样 printf/getpid/uptime
 */
#include "clib.h"

void lib_main(void)
{
	printf("[libc] hello from the C library!\n\r");
	printf("[libc] pid=%d uptime=%d\n\r", getpid(), uptime());
	for (;;)
		;
}
