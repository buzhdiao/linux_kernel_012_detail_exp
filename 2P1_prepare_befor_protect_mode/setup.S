# 定义段地址常量
# INITSEG 定义为 0x9000，这通常是初始化代码段的段地址
INITSEG = 0x9000
# SYSSEG 定义为 0x1000，可能用于系统代码段
SYSSEG  = 0x1000
# SETUPSEG 定义为 0x9020，可能用于设置代码段
SETUPSEG = 0x9020

# 指定使用 16 位代码模式
.code16
# 声明代码段
.text
# 声明全局符号 _start_setup，使得该符号可以被外部引用
.globl _start_setup

# 程序入口点
_start_setup:
	# 将代码段寄存器 CS 的值复制到 AX 寄存器
	movw %cs, %ax
	# 将 AX 的值复制到数据段寄存器 DS，使 DS 和 CS 指向相同的段
	movw %ax, %ds
	# 将 AX 的值复制到附加段寄存器 ES，使 ES 和 CS 指向相同的段
	movw %ax, %es

	# 将字符串 "setup is running" 的地址加载到 AX 寄存器
	movw $setup_msg, %ax
	# 将 AX 的值复制到基址指针寄存器 BP，用于后续显示字符串
	movw %ax, %bp
	# 设置显示模式为 0x01301，这是一种图形显示模式
	movw $0x01301, %ax
	# 设置文本颜色为 0x0c（红色）
	movw $0x0c, %bx
	# 设置字符串长度为 16 个字符
	movw $16, %cx
	# 设置显示的行号为 3
	movb $3, %dh
	# 设置显示的列号为 0
	movb $0, %dl
	# 调用 BIOS 中断 0x10 来显示字符串
	int  $0x010

	# 将 INITSEG 的值加载到 AX 寄存器
	movw $INITSEG, %ax
	# 将 AX 的值复制到数据段寄存器 DS，使 DS 指向 INITSEG 段
	movw %ax, %ds
	# 设置 AH 寄存器为 0x03，用于获取光标位置
	movb $0x03, %ah
	# 将 BH 寄存器清零
	xor  %bh,  %bh
	# 调用 BIOS 中断 0x10 来获取光标位置
	int  $0x10
	# 将 DX 寄存器的值（光标位置）存储到内存地址 0 处
	movw %dx, (0)
	# 设置 AH 寄存器为 0x88，用于获取扩展内存大小
	movb $0x88, %ah
	# 调用 BIOS 中断 0x15 来获取扩展内存大小
	int  $0x15
	# 将 AX 寄存器的值（扩展内存大小）存储到内存地址 2 处
	movw %ax, (2)

	# 设置 AH 寄存器为 0x0f，用于获取当前视频模式信息
	movb $0x0f, %ah
	# 调用 BIOS 中断 0x10 来获取当前视频模式信息
	int  $0x10
	# 将 BX 寄存器的值（视频模式相关信息）存储到内存地址 4 处
	movw %bx, (4)
	# 将 AX 寄存器的值（视频模式相关信息）存储到内存地址 6 处
	movw %ax, (6)
	# 设置 AH 寄存器为 0x12，用于获取视频适配器信息
	movb $0x12, %ah
	# 设置 BL 寄存器为 0x10，用于指定获取信息的类型
	movb $0x10, %bl
	# 调用 BIOS 中断 0x10 来获取视频适配器信息
	int  $0x10
	# 将 AX 寄存器的值（视频适配器信息）存储到内存地址 8 处
	movw %ax, (8)
	# 将 BX 寄存器的值（视频适配器信息）存储到内存地址 10 处
	movw %bx, (10)
	# 将 CX 寄存器的值（视频适配器信息）存储到内存地址 12 处
	movw %cx, (12)
	# 将 0x5019 存储到内存地址 14 处
	movw $0x5019, (14)

	# 将 0x0000 加载到 AX 寄存器
	movw $0x0000, %ax
	# 将 AX 的值复制到数据段寄存器 DS，使 DS 指向 0x0000 段
	movw %ax, %ds
	# 从内存地址 4 * 0x41 处加载一个字到源变址寄存器 SI
	ldsw (4 * 0x41), %si
	# 将 INITSEG 的值加载到 AX 寄存器
	movw $INITSEG, %ax
	# 将 AX 的值复制到附加段寄存器 ES，使 ES 指向 INITSEG 段
	movw %ax, %es
	# 将 0x0080 加载到目的变址寄存器 DI
	movw $0x0080, %di
	# 将 0x10 加载到 CX 寄存器，作为循环计数器
	movw $0x10, %cx
	# 重复执行 movsb 指令，将数据从 DS:SI 复制到 ES:DI
	rep
	movsb

	# get hd1 data 
	# 将 0x0000 加载到 AX 寄存器
	movw $0x0000, %ax
	# 将 AX 的值复制到数据段寄存器 DS，使 DS 指向 0x0000 段
	movw %ax, %ds
	# 从内存地址 4 * 0x46 处加载一个字到源变址寄存器 SI
	ldsw (4 * 0x46), %si
	# 将 INITSEG 的值加载到 AX 寄存器
	movw $INITSEG, %ax
	# 将 AX 的值复制到附加段寄存器 ES，使 ES 指向 INITSEG 段
	movw %ax, %es
	# 将 0x0090 加载到目的变址寄存器 DI
	movw $0x0090, %di
	# 将 0x10 加载到 CX 寄存器，作为循环计数器
	movw $0x10, %cx
	# 重复执行 movsb 指令，将数据从 DS:SI 复制到 ES:DI
	rep
	movsb

	# 将 0x1500 加载到 AX 寄存器
	movw $0x1500, %ax
	# 设置 DL 寄存器为 0x81，可能用于指定磁盘设备
	movb $0x81, %dl
	# 调用 BIOS 中断 0x13 来检查磁盘设备
	int  $0x13
	# 如果进位标志 CF 被设置（表示有错误），跳转到 no_disk1 标签处
	jc   no_disk1
	# 比较 AH 寄存器的值是否等于 3
	cmpb $3, %ah
	# 如果相等，跳转到 is_disk1 标签处
	je   is_disk1
no_disk1:
	# 将 INITSEG 的值加载到 AX 寄存器
	movw $INITSEG, %ax
	# 将 AX 的值复制到附加段寄存器 ES，使 ES 指向 INITSEG 段
	movw %ax, %es
	# 将 0x0090 加载到目的变址寄存器 DI
	movw $0x0090, %di
	# 将 0x10 加载到 CX 寄存器，作为循环计数器
	movw $0x10, %cx
	# 将 0x00 加载到 AX 寄存器
	movw $0x00, %ax
	# 重复执行 stosb 指令，将 AX 的值存储到 ES:DI 指向的内存位置
	rep
	stosb

is_disk1:
	# prepare for protection mode 
	# 关闭中断，防止在进入保护模式过程中被中断干扰
	cli

	# 将 0x0000 加载到 AX 寄存器
	movw $0x0000, %ax
	# 清除方向标志 DF，使串操作按递增方向进行
	cld
do_move:
	# 将 AX 的值复制到附加段寄存器 ES
	movw %ax, %es
	# 将 AX 的值加上 0x1000
	addw $0x1000, %ax
	# 比较 AX 的值是否等于 0x9000
	cmpw $0x9000, %ax
	# 如果相等，跳转到 end_move 标签处
	jz   end_move
	# 将 AX 的值复制到数据段寄存器 DS
	movw %ax, %ds
	# 将 DI 寄存器清零
	subw %di, %di
	# 将 SI 寄存器清零
	subw %si, %si
	# 将 0x8000 加载到 CX 寄存器，作为循环计数器
	movw $0x8000, %cx
	# 重复执行 movsw 指令，将数据从 DS:SI 复制到 ES:DI
	rep
	movsw
	# 跳转到 do_move 标签处，继续循环
	jmp  do_move

end_move:
	# 将 0xb800 加载到 AX 寄存器，这是文本显示缓冲区的段地址
	movw $0xb800, %ax
	# 将 AX 的值复制到段寄存器 GS，使 GS 指向文本显示缓冲区
	movw %ax, %gs
	# 设置 AH 寄存器为 0xf，作为文本颜色
	movb $0xf, %ah
	# 设置 AL 寄存器为 0x41，可能是要显示的字符
	movb $0x41, %al
	# 将 0x100 加载到目的变址寄存器 EDI
	movl $0x100, %edi
	# 将 AX 的值存储到 GS:DI 指向的文本显示缓冲区位置
	movw %ax, %gs:(%di)

	# 将 SETUPSEG 的值加载到 AX 寄存器
	movw $SETUPSEG, %ax
	# 将 AX 的值复制到数据段寄存器 DS，使 DS 指向 SETUPSEG 段
	movw %ax, %ds
	# 加载全局描述符表（GDT）的 48 位指针
	lgdt gdt_48

	# 调用 empty_8042 子程序，用于清空 8042 键盘控制器缓冲区
	call empty_8042
	# 设置 AL 寄存器为 0xD1，用于向 8042 键盘控制器发送命令
	movb $0xD1, %al
	# 将 AL 的值输出到端口 0x64
	outb %al,  $0x64
	# 调用 empty_8042 子程序，用于清空 8042 键盘控制器缓冲区
	call empty_8042
	# 设置 AL 寄存器为 0xDF，用于向 8042 键盘控制器发送命令
	movb $0xDF, %al
	# 将 AL 的值输出到端口 0x60
	outb %al,  $0x60
	# 调用 empty_8042 子程序，用于清空 8042 键盘控制器缓冲区
	call empty_8042

	# 设置 AL 寄存器为 0x11，用于初始化 8259A 可编程中断控制器（PIC）
	movb $0x11, %al
	# 将 AL 的值输出到端口 0x20
	outb %al,  $0x20
	# 插入两个空操作指令，用于延时
	.word 0x00eb, 0x00eb
	# 将 AL 的值输出到端口 0xA0
	outb %al,  $0xA0
	# 插入两个空操作指令，用于延时
	.word 0x00eb, 0x00eb
	# 设置 AL 寄存器为 0x20，用于设置主 8259A PIC 的中断向量偏移
	movb $0x20, %al
	# 将 AL 的值输出到端口 0x21
	outb %al, $0x21
	# 插入两个空操作指令，用于延时
	.word 0x00eb, 0x00eb
	# 设置 AL 寄存器为 0x28，用于设置从 8259A PIC 的中断向量偏移
	movb $0x28, %al
	# 将 AL 的值输出到端口 0xA1
	outb %al,   $0xA1
	# 插入两个空操作指令，用于延时
	.word 0x00eb, 0x00eb
	# 设置 AL 寄存器为 0x04，用于设置主 8259A PIC 的级联方式
	movb $0x04, %al
	# 将 AL 的值输出到端口 0x21
	outb %al, $0x21
	# 插入两个空操作指令，用于延时
	.word 0x00eb, 0x00eb
	# 设置 AL 寄存器为 0x02，用于设置从 8259A PIC 的级联方式
	movb $0x02, %al
	# 将 AL 的值输出到端口 0xA1
	outb %al, $0xA1

	# 插入两个空操作指令，用于延时
	.word 0x00eb, 0x00eb
	# 设置 AL 寄存器为 0x01，用于设置 8259A PIC 的工作模式
	movb $0x01, %al
	# 将 AL 的值输出到端口 0x21
	outb %al, $0x21
	# 插入两个空操作指令，用于延时
	.word 0x00eb, 0x00eb
	# 将 AL 的值输出到端口 0xA1
	outb %al, $0xA1
	# 插入两个空操作指令，用于延时
	.word 0x00eb, 0x00eb
	# 设置 AL 寄存器为 0xff，用于屏蔽所有中断
	movb $0xff, %al
	# 将 AL 的值输出到端口 0x21
	outb %al, $0x21
	# 插入两个空操作指令，用于延时
	.word 0x00eb, 0x00eb
	# 将 AL 的值输出到端口 0xA1
	outb %al, $0xA1

	# 将控制寄存器 CR0 的值加载到 EAX 寄存器
	movl %cr0, %eax
	# 将 AL 寄存器的最低位取反，用于设置保护模式标志
	xorb $1, %al
	# 将 EAX 的值写回到控制寄存器 CR0，进入保护模式
	movl %eax, %cr0

	# 以下是一个远跳转指令，用于跳转到保护模式下的代码段
	.byte 0x66, 0xea
	.long 0x0
	.word 0x8

# 清空 8042 键盘控制器缓冲区的子程序
empty_8042:
	# 插入两个空操作指令，用于延时
	.word 0x00eb, 0x00eb
	# 从端口 0x64 读取一个字节到 AL 寄存器
	inb $0x64, %al
	# 测试 AL 寄存器的第 2 位是否为 1
	testb $2, %al
	# 如果第 2 位为 1，跳转到 empty_8042 标签处，继续等待
	jnz empty_8042
	# 返回主程序
	ret

# 全局描述符表（GDT）的定义
gdt:
	# 第一个描述符通常为 NULL 描述符
	.word 0,0,0,0

	# 代码段描述符
	.word 0x07ff
	.word 0x0000
	.word 0x9A00
	.word 0x00C0

	# 数据段描述符
	.word 0x07ff
	.word 0x0000
	.word 0x9200
	.word 0x00c0

	# 系统段描述符
	.word 0xffff
	.word 0x8000
	.word 0x920b
	.word 0x00c0

# GDT 的 48 位指针定义
gdt_48:
	# GDT 的界限
	.word 0x800
	# GDT 的基地址
	.word 512+gdt, 0x9

# 要显示的字符串
setup_msg:
	.ascii "setup is running"