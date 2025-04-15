# 定义系统大小常量，SYSSIZE 表示系统代码的大小为 0x3000 字节， 12KB
SYSSIZE = 0x3000
# 定义设置部分长度常量，SETUPLEN 表示设置部分的扇区数为 4
SETUPLEN  = 4
# 定义引导段地址常量，BOOTSEG 表示引导段的段地址为 0x7c0，通常引导程序加载到该段
BOOTSEG	 = 0x7c0
# 定义初始化段地址常量，INITSEG 表示初始化段的段地址为 0x9000，用于初始化相关操作
INITSEG	 = 0x9000
# 定义设置段地址常量，SETUPSEG 表示设置段的段地址为 0x9020，用于系统设置相关操作
SETUPSEG  = 0x9020
# 定义系统段地址常量，SYSSEG 表示系统段的段地址为 0x1000，存放系统代码
SYSSEG = 0x1000
# 计算结束段地址，ENDSEG 为系统段地址加上系统大小，即系统代码的结束位置
ENDSEG  = SYSSEG + SYSSIZE
# 定义根设备号常量，ROOT_DEV 表示根设备的设备号为 0x000
ROOT_DEV  = 0x000

# 指定使用 16 位代码模式，适用于早期 8086 架构的处理器
.code16
# 声明代码段，后续代码将存储在代码段中
.text

# 声明全局符号 _start，作为程序的入口点，可被外部引用
.global _start
_start:
	# 长跳转指令，跳转到 BOOTSEG 段的 start2 位置
	jmpl $BOOTSEG, $start2

start2:
	# 将 BOOTSEG 的值赋给 AX 寄存器
	movw $BOOTSEG, %ax
	# 将 AX 的值赋给数据段寄存器 DS，设置数据段为 BOOTSEG 段
	movw %ax, %ds
	# 将 INITSEG 的值赋给 AX 寄存器
	movw $INITSEG, %ax
	# 将 AX 的值赋给附加段寄存器 ES，设置附加段为 INITSEG 段
	movw %ax, %es
	# 将 256 赋给 CX 寄存器，作为循环次数
	movw $256, %cx
	# 将源变址寄存器 SI 清零
	subw %si, %si
	# 将目的变址寄存器 DI 清零
	subw %di, %di

	# 重复执行 movsw 指令，每次移动一个字（16 位）的数据，直到 CX 为 0
	rep
	movsw

	# 长跳转指令，跳转到 INITSEG 段的 go 位置
	jmpl $INITSEG, $go

go:
	# 将代码段寄存器 CS 的值赋给 AX 寄存器
	movw %cs, %ax
	# 将 AX 的值赋给数据段寄存器 DS，设置数据段为当前代码段
	movw %ax, %ds
	# 将 AX 的值赋给附加段寄存器 ES，设置附加段为当前代码段
	movw %ax, %es
	# 将 AX 的值赋给堆栈段寄存器 SS，设置堆栈段为当前代码段
	movw %ax, %ss
	# 将 0xFF00 赋给堆栈指针寄存器 SP，设置堆栈指针
	movw $0xFF00, %sp

load_setup:
	# 将 0x0000 赋给 DX 寄存器，可能用于指定磁盘设备号等
	movw $0x0000, %dx
	# 将 0x0002 赋给 CX 寄存器，可能表示要读取的扇区数等
	movw $0x0002, %cx
	# 将 0x0200 赋给 BX 寄存器，作为数据存储的内存偏移地址
	movw $0x0200, %bx
	# 将 SETUPLEN 的值赋给 AL 寄存器，指定要读取的扇区数
	movb $SETUPLEN, %al
	# 将 0x02 赋给 AH 寄存器，指定 BIOS 中断 0x13 的功能号（读磁盘扇区）
	movb $0x02,	 %ah
	# 调用 BIOS 中断 0x13 来读取磁盘扇区数据
	int	$0x13
	# 如果没有进位（即读取成功），跳转到 ok_load_setup 标签处
	jnc	ok_load_setup
	# 如果读取失败，将 0x0000 赋给 DX 寄存器
	movw $0x0000, %dx
	# 将 0x0000 赋给 AX 寄存器
	movw $0x0000, %ax
	# 调用 BIOS 中断 0x13 来重置磁盘
	int	$0x13
	# 跳回到 load_setup 标签处，重新尝试读取
	jmp	load_setup

ok_load_setup:
	# 将 0x00 赋给 DL 寄存器，可能用于指定磁盘设备号
	movb $0x00, %dl
	# 将 0x0800 赋给 AX 寄存器，可能是磁盘操作的参数
	movw $0x0800, %ax
	# 调用 BIOS 中断 0x13 进行磁盘操作
	int	$0x13
	# 将 0x00 赋给 CH 寄存器，可能用于指定磁道号等
	movb $0x00, %ch
	# 将 CX 寄存器的值存储到当前代码段的 sectors 位置，记录扇区数等信息
	movw %cx, %cs:sectors
	# 将 INITSEG 的值赋给 AX 寄存器
	movw $INITSEG, %ax
	# 将 AX 的值赋给附加段寄存器 ES，设置附加段为 INITSEG 段
	movw %ax, %es

	# 将 SYSSEG 的值赋给 AX 寄存器
	movw $SYSSEG, %ax
	# 将 AX 的值赋给附加段寄存器 ES，设置附加段为 SYSSEG 段
	movw %ax, %es
	# 调用 read_it 子程序，用于读取磁盘数据到内存
	call read_it
	# 调用 kill_motor 子程序，用于关闭磁盘电机
	call kill_motor

	# 将当前代码段中 root_dev 位置的值赋给 AX 寄存器，获取根设备号
	movw %cs:root_dev, %ax
	# 比较 AX 寄存器的值与 0 是否相等
	cmpw $0, %ax
	# 如果不相等，跳转到 root_defined 标签处，说明根设备号已定义
	jne	root_defined
	# 将当前代码段中 sectors 位置的值赋给 BX 寄存器，获取扇区数
	movw %cs:sectors, %bx
	# 将 0x0208 赋给 AX 寄存器，可能是某种条件判断的值
	movw $0x0208, %ax
	# 比较 BX 寄存器的值与 15 是否相等
	cmpw $15, %bx
	# 如果相等，跳转到 root_defined 标签处，确定根设备号
	je	 root_defined
	# 将 0x021c 赋给 AX 寄存器，可能是另一种条件判断的值
	movw $0x021c, %ax
	# 比较 BX 寄存器的值与 18 是否相等
	cmpw $18, %bx
	# 如果相等，跳转到 root_defined 标签处，确定根设备号
	je	 root_defined
undef_root:
	# 无限循环，若根设备号未定义则陷入此循环
	jmp	undef_root
root_defined:
	# 将 AX 寄存器的值存储到当前代码段的 root_dev 位置，确定根设备号
	mov	%ax, %cs:root_dev

	# 将当前代码段寄存器 CS 的值赋给 AX 寄存器
	movw %cs, %ax
	# 将 AX 的值赋给数据段寄存器 DS，设置数据段为当前代码段
	movw %ax, %ds
	# 将 AX 的值赋给附加段寄存器 ES，设置附加段为当前代码段
	movw %ax, %es

	# 将 0x600 赋给 AX 寄存器，可能是显示操作的参数
	movw $0x600, %ax
	# 将 0x700 赋给 BX 寄存器，可能是显示操作的参数
	movw $0x700, %bx
	# 将 0x0 赋给 CX 寄存器，可能是显示操作的参数
	movw $0,	 %cx
	# 将 0x184f 赋给 DX 寄存器，可能是显示操作的参数
	movw $0x184f,%dx
	# 调用 BIOS 中断 0x10 进行显示相关操作
	int	$0x10

	# 将 msg 的地址赋给 AX 寄存器，msg 是要显示的字符串
	movw $msg, %ax
	# 将 AX 的值赋给基址指针寄存器 BP，用于显示字符串
	movw %ax, %bp
	# 将 0x01301 赋给 AX 寄存器，设置显示模式等
	movw $0x01301, %ax
	# 将 0x0c 赋给 BX 寄存器，设置文本颜色等
	movw $0x0c, %bx
	# 将 23 赋给 CX 寄存器，设置字符串长度
	movw $23, %cx
	# 将 0 赋给 DX 寄存器，可能是显示位置的参数
	movw $0, %dx
	# 调用 BIOS 中断 0x010 显示字符串
	int	$0x010

	# 长跳转指令，跳转到 SETUPSEG 段的 0 位置
	jmpl $SETUPSEG, $0

sread:
	.word 1+SETUPLEN
head:
	.word 0
track:
	.word 0
read_it:
	# 将附加段寄存器 ES 的值赋给 AX 寄存器
	movw %es, %ax
	# 测试 AX 寄存器的值与 0x0fff 是否有非零位
	testw $0x0fff, %ax
die:
	# 如果有非零位，跳转到 die 标签处，可能是错误处理
	jne die
	# 将 BX 寄存器清零
	xor %bx, %bx
rp_read:
	# 将附加段寄存器 ES 的值赋给 AX 寄存器
	movw %es, %ax
	# 比较 AX 寄存器的值与 ENDSEG 是否小于
	cmpw $ENDSEG, %ax
	# 如果小于，跳转到 ok1_read 标签处，继续读取
	jb	 ok1_read
	# 如果不小于，返回
	ret
ok1_read:
	# 将当前代码段中 sectors 位置的值赋给 AX 寄存器，获取扇区数
	movw %cs:sectors, %ax
	# 用扇区数减去 sread 的值
	subw sread, %ax
	# 将结果赋给 CX 寄存器
	movw %ax, %cx
	# 将 CX 寄存器的值左移 9 位（乘以 512）
	shlw $9,	%cx
	# 将 BX 寄存器的值加到 CX 寄存器
	addw %bx, %cx
	# 如果没有进位，跳转到 ok2_read 标签处
	jnc	ok2_read
	# 如果相等，跳转到 ok2_read 标签处
	je	 ok2_read
	# 将 AX 寄存器清零
	xorw %ax, %ax
	# 用 BX 寄存器的值减去 AX 寄存器的值
	subw %bx, %ax
	# 将 AX 寄存器的值右移 9 位（除以 512）
	shrw $9,	%ax
ok2_read:
	# 调用 read_track 子程序，读取一个磁道的数据
	call read_track
	# 将 AX 寄存器的值赋给 CX 寄存器
	movw %ax, %cx
	# 将 sread 的值加到 AX 寄存器
	addw sread, %ax
	# 比较 AX 寄存器的值与当前代码段中 sectors 位置的值是否不相等
	cmpw %cs:sectors, %ax
	# 如果不相等，跳转到 ok3_read 标签处
	jne	ok3_read
	# 将 1 赋给 AX 寄存器
	movw $1, %ax
	# 用 AX 寄存器的值减去当前代码段中 head 位置的值
	subw head, %ax
	# 如果不相等，跳转到 ok4_read 标签处
	jne	ok4_read
	# 将当前代码段中 track 位置的值加 1
	incw track
ok4_read:
	# 将 AX 寄存器的值存储到当前代码段的 head 位置
	movw %ax, head
	# 将 AX 寄存器清零
	xorw %ax, %ax
ok3_read:
	# 将 AX 寄存器的值存储到当前代码段的 sread 位置
	movw %ax, sread
	# 将 CX 寄存器的值左移 9 位（乘以 512）
	shlw $9,	%cx
	# 将 CX 寄存器的值加到 BX 寄存器
	addw %cx, %bx
	# 如果没有进位，跳转到 rp_read 标签处，继续读取
	jnc	rp_read
	# 将附加段寄存器 ES 的值赋给 AX 寄存器
	movw %es, %ax
	# 将 AX 寄存器的值加上 0x1000
	addw $0x1000, %ax
	# 将 AX 的值赋给附加段寄存器 ES，切换到下一个段
	movw %ax, %es
	# 将 BX 寄存器清零
	xorw %bx, %bx
	# 跳转到 rp_read 标签处，继续读取
	jmp	rp_read

read_track:
	# 将 AX 寄存器的值压入堆栈
	pushw %ax
	# 将 BX 寄存器的值压入堆栈
	pushw %bx
	#将 CX 寄存器的值压入堆栈
	pushw %cx
	#将 DX 寄存器的值压入堆栈
	pushw %dx
	# 将当前代码段中 track 位置的值赋给 DX 寄存器，获取磁道号
	movw	track, %dx
	# 将当前代码段中 sread 位置的值赋给 CX 寄存器，获取要读取的扇区偏移
	movw	sread, %cx
	# 将 CX 寄存器的值加 1
	incw	%cx
	# 将 DL 寄存器的值赋给 CH 寄存器，设置磁道号
	movb	%dl, %ch
	# 将当前代码段中 head 位置的值赋给 DX 寄存器，获取磁头号
	movw	head, %dx
	# 将 DL 寄存器的值赋给 DH 寄存器，设置磁头号
	movb	%dl, %dh
	# 将 0 赋给 DL 寄存器，可能用于指定磁盘设备号
	movb	$0, %dl
	# 与 0x0100 进行按位与操作，设置 DX 寄存器的某些位
	andw	$0x0100, %dx
	# 将 2 赋给 AH 寄存器，指定 BIOS 中断 0x13 的功能号（读磁盘扇区）
	movb	$2, %ah
	# 调用 BIOS 中断 0x13 读取磁盘扇区数据
	int	 $0x13
	# 如果有进位（即读取失败），跳转到 bad_rt 标签处
	jc	bad_rt
	# 将 DX 寄存器的值从堆栈弹出
	popw	%dx
	# 将 CX 寄存器的值从堆栈弹出
	popw	%cx
	# 将 BX 寄存器的值从堆栈弹出
	popw	%bx
	# 将 AX 寄存器的值从堆栈弹出
	popw	%ax
	# 返回
	ret
bad_rt:
	# 将 0x0 赋给 AX 寄存器
	movw $0x0, %ax
	# 将 0x0 赋给 DX 寄存器
	movw $0x0, %dx
	# 调用 BIOS 中断 0x13 重置0号磁盘
	int	$0x13
	# 将 DX 寄存器的值从堆栈弹出
	popw %dx
	# 将 CX 寄存器的值从堆栈弹出
	popw %cx
	# 将 BX 寄存器的值从堆栈弹出
	popw %bx
	# 将 AX 寄存器的值从堆栈弹出
	popw %ax
	# 跳转到 read_track 标签处，重新尝试读取
	jmp read_track

kill_motor:
	# 将 DX 寄存器的值压入堆栈，保存 DX 寄存器原来的值
	pushw %dx
	# 将 0x3f2 赋值给 DX 寄存器，0x3f2 是软盘控制器中控制电机的端口地址
	movw	$0x3f2, %dx
	# 将 0 赋值给 AL 寄存器，准备向端口写入数据
	movb	$0,	%al
	# 将 AL 寄存器中的值输出到 DX 寄存器指定的端口（0x3f2），通过写入 0 来关闭软盘电机
	outb	%al, %dx
	# 将之前压入堆栈的 DX 寄存器的值弹出，恢复 DX 寄存器原来的值
	popw	%dx
	# 从当前子程序返回，回到调用它的地方
	ret

sectors:
	# 定义一个 16 位（2 字节）的内存空间，名称为 sectors，初始值为 0
	# 可能用于存储与磁盘扇区相关的数量或编号等信息
	 .word 0

msg:
	# 定义一个字节，值为 13（回车符的 ASCII 码）
	 .byte 13
	# 定义一个字节，值为 10（换行符的 ASCII 码）
	 .byte 10
	# 定义一个字符串，内容为 "Linux is booting..."
	 .ascii "Linux is booting..."
	# 定义一个字节，值为 13（回车符的 ASCII 码）
	 .byte 13
	# 定义一个字节，值为 10（换行符的 ASCII 码）
	 .byte 10
	# 将当前位置调整到偏移量为 508 的地方，用于后续数据或代码的定位
	 .org 508
root_dev:
	# 定义一个 16 位（2 字节）的内存空间，名称为 root_dev
	# 并将 ROOT_DEV 常量的值（在代码开头定义为 0x000）存储到该内存空间，用于表示根设备号
	 .word ROOT_DEV
boot_flag:
	# 定义一个 16 位（2 字节）的内存空间，名称为 boot_flag
	# 并将其值设置为 0xaa55，这是引导扇区的有效标志，用于标识该扇区是一个可引导的扇区
	 .word 0xaa55