/*
 * 3P1 main.c —— 内核的第一个 C 函数
 *
 * head.S 的 setup_paging 开启分页后，用 ret 技巧跳到这里。
 * 此时还没有 printk（那是 3P2 的事），只能直接写 VGA 显存。
 *
 * 本函数做三件事：
 *   1. 打横幅证明已进入 main()
 *   2. 读 CR0 验证 PG 位（bit31）确实为 1——分页真正打开的铁证
 *   3. 再通过页表访问 8MB 处的内存（4MB 以上必须经第 2 个页表转换），
 *      读写一致说明页表填充正确
 */

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned long  u32;

/* VGA 文本模式显存：每字符 2 字节（ASCII + 属性），80 列 * 25 行 */
static volatile u16 *const vga = (u16 *)0xb8000;

/* 在第 row 行第 col 列起显示一个字符串，attr 为颜色属性 */
static void put_str(int row, int col, const char *s, u8 attr)
{
	volatile u16 *p = vga + row * 80 + col;
	while (*s) {
		*p++ = (u16)(*s++) | ((u16)attr << 8);
	}
}

/* 在第 row 行第 col 列起显示 8 位十六进制数 */
static void put_hex32(int row, int col, u32 v, u8 attr)
{
	volatile u16 *p = vga + row * 80 + col;
	int i;
	for (i = 7; i >= 0; i--) {
		int d = (v >> (i * 4)) & 0xf;
		char c = d < 10 ? '0' + d : 'A' + d - 10;
		*p++ = (u16)c | ((u16)attr << 8);
	}
}

/* 属性字节：高 4 位背景（0=黑），低 4 位前景（A=绿，F=白） */
#define GREEN 0x0a
#define WHITE 0x0f

void main(void)
{
	u32 cr0;
	volatile u32 *high_mem = (u32 *)0x800000;	/* 8MB 处，落在 pg2 页表 */

	put_str(9, 0, "[3P1] paging enabled, welcome to main()", GREEN);
	put_str(10, 0, "CR0 = 0x", WHITE);

	/* 读控制寄存器 CR0：bit31 是 PG（分页使能）位 */
	__asm__ volatile("movl %%cr0, %0" : "=r"(cr0));
	put_hex32(10, 7, cr0, WHITE);

	/* 经页表访问 8MB 处：写进去再读出来，一致则 pg2 填写正确 */
	*high_mem = 0x1234abcd;
	if (*high_mem == 0x1234abcd)
		put_str(11, 0, "access 8MB via page table: OK", GREEN);
	else
		put_str(11, 0, "access 8MB via page table: FAIL", 0x0c);

	/* 分页检查全部通过，在这里停机；
	 * 后续章节将从这里继续扩展 main() 的初始化流程 */
	__asm__ volatile("hlt");
	for (;;)
		;
}
