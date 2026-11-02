# =====================================================================
# 2P2 内存管理实验：A20 地址线的开启与验证
# 对应书中 2.2 节"内存管理"
#
# 实验内容：
#   1. 检测 A20 地址线当前状态（关闭时 1MB 以上地址会"回卷"）
#   2. 通过键盘控制器 8042 开启 A20（与官方仓 setup.S 做法一致）
#   3. 再次检测，在屏幕上打印开启前后的对比结果
#
# 本实验只有引导扇区一个程序，不加载 setup，独立运行
# =====================================================================

# 引导扇区被 BIOS 加载到 0x7C00 处，段地址为 0x7C0
BOOTSEG = 0x7c0

# 使用 16 位实模式代码
.code16
.text

# 声明全局入口符号
.global _start

_start:
	# 长跳转到 BOOTSEG 段的 start 标签
	# BIOS 跳转到引导程序时，有的机器 CS=0 IP=0x7C00，有的 CS=0x7C0 IP=0
	# 先做一次段间跳转，把 CS:IP 规范化为 0x7C0:0，后面的偏移地址才正确
	jmpl $BOOTSEG, $start

start:
	# 让 DS、ES 与 CS 指向同一段，便于访问本程序内的数据（字符串等）
	movw %cs, %ax
	movw %ax, %ds
	movw %ax, %es
	# 设置栈：SS=0，SP=0x7C00，栈从 0x7C00 向低地址增长，不会覆盖引导程序
	movw $0, %ax
	movw %ax, %ss
	movw $0x7c00, %sp

	# 打印实验标题
	movw $msg_title, %si
	call print_str

	# ---------- 第一步：检测 A20 初始状态 ----------
	call test_a20
	# 返回值放在 AX 中：1 表示 A20 开启，0 表示关闭
	movw %ax, %bx

	movw $msg_before, %si
	call print_str
	movw %bx, %ax
	call print_on_off

	# ---------- 第二步：若未开启，则通过 8042 开启 ----------
	cmpw $0, %bx
	jne already_on

	movw $msg_enabling, %si
	call print_str
	call enable_a20

	# 8042 处理需要一点时间，用空循环短暂延时
	movw $0x2000, %cx
delay:
	decw %cx
	jnz delay

	# ---------- 第三步：再次检测，确认开启结果 ----------
	call test_a20

	movw $msg_after, %si
	call print_str
	call print_on_off
	jmp done

already_on:
	# A20 本来就是开启的，无需处理
	movw $msg_skip, %si
	call print_str

done:
	# 关中断并停机，屏幕停留在最终结果上
	cli
1:
	hlt
	jmp 1b

# ---------------------------------------------------------------------
# 检测 A20 地址线是否开启
# 原理：实模式下地址 = 段值*16 + 偏移。
#   地址 0x0000:0x0500   的物理地址是 0x00500
#   地址 0xFFFF:0x0510   的物理地址是 0x100510（超过 1MB）
# 若 A20 关闭，第 21 根地址线恒为 0，超过 1MB 的地址会回卷（mod 1MB），
# 0x100510 就回卷到 0x000500，两个地址指向同一块内存。
# 写不同的特征值到低地址，再读高地址：
#   每次读到的都恰好是刚写入的值 → 两地址重叠 → A20 关闭
# 返回：AX = 1 表示开启，AX = 0 表示关闭
# ---------------------------------------------------------------------
test_a20:
	pushw %ds
	pushw %es
	pushw %bx
	pushw %cx

	# 测试期间关中断，防止中断处理程序破坏我们做标记的内存单元
	cli

	# 先保存低地址单元的原始内容，测试结束后恢复
	xorw %ax, %ax
	movw %ax, %ds			# DS = 0x0000
	movw $0x0500, %bx
	movb (%bx), %cl			# CL 保存 0x0500 处原值

	# 第一次：低地址写 0x55，读高地址
	movb $0x55, (%bx)
	movw $0xffff, %ax
	movw %ax, %es			# ES = 0xFFFF
	movb %es:0x0510, %al		# 读"1MB 以上"的地址
	movb %al, %ch			# CH = 第一次读到的值

	# 第二次：低地址改写 0xAA，再读高地址
	movb $0xaa, (%bx)
	movb %es:0x0510, %al		# AL = 第二次读到的值

	# 恢复 0x0500 处的原值
	movb %cl, (%bx)
	sti

	# 判断：两次读到的值分别是 0x55 和 0xAA，
	# 说明高地址读到的永远是低地址刚写入的值，地址发生了回卷
	cmpb $0x55, %ch
	jne a20_is_on
	cmpb $0xaa, %al
	jne a20_is_on
	xorw %ax, %ax			# A20 关闭
	jmp a20_done
a20_is_on:
	movw $1, %ax			# A20 开启
a20_done:
	popw %cx
	popw %bx
	popw %es
	popw %ds
	ret

# ---------------------------------------------------------------------
# 通过键盘控制器 8042 开启 A20 地址线
# 8042 的状态口是 0x64，数据口是 0x60
# 向 8042 输出端口（PS/2 系统控制端口）写命令：
#   D1 —— 表示"下一个写到 0x60 的字节要写到输出端口"
#   DF —— 输出端口的值：bit1 置 1 打开 A20 闸门（bit0 是 CPU 复位线，必须为 1）
# 这段逻辑与官方仓 setup.S 中开启 A20 的部分完全一致
# ---------------------------------------------------------------------
enable_a20:
	call empty_8042			# 等输入缓冲区空
	movb $0xd1, %al			# 命令字节 D1：准备写输出端口
	outb %al, $0x64
	call empty_8042
	movb $0xdf, %al			# 数据字节 DF：A20 闸门打开
	outb %al, $0x60
	call empty_8042
	ret

# ---------------------------------------------------------------------
# 等待 8042 输入缓冲区为空
# 读状态口 0x64，bit1 为 1 表示输入缓冲区满（8042 还没处理完），需要等待
# ---------------------------------------------------------------------
empty_8042:
	.word 0x00eb, 0x00eb		# jmp $+2 两条，跳到下一条指令本身，作短暂延时
	inb $0x64, %al
	testb $2, %al
	jnz empty_8042			# 缓冲区满则继续等待
	ret

# ---------------------------------------------------------------------
# 打印字符串
# 入口：SI = 字符串地址（以 0 结尾）
# ---------------------------------------------------------------------
print_str:
	pushw %ax
	pushw %bx
	pushw %si
	movw $0x0007, %bx		# BH=页号0，BL=前景色7（白色）
1:
	movb (%si), %al			# 取一个字符
	testb %al, %al			# 遇到 0 结束
	jz 1f
	movb $0x0e, %ah			# int 0x10 功能号 0x0E：电传方式输出字符并自动前移光标
	int $0x10
	incw %si
	jmp 1b
1:
	popw %si
	popw %bx
	popw %ax
	ret

# ---------------------------------------------------------------------
# 根据 AX（0/1）打印 "OFF" 或 "ON"
# ---------------------------------------------------------------------
print_on_off:
	pushw %si
	cmpw $0, %ax
	je 2f
	movw $msg_on, %si
	call print_str
	jmp 3f
2:
	movw $msg_off, %si
	call print_str
3:
	popw %si
	ret

# ---------------------------------------------------------------------
# 数据区：各种提示字符串，均以回车换行(13,10)开头、0 结尾
# ---------------------------------------------------------------------
msg_title:
	.ascii "\r\n"
	.ascii "[2P2] A20 test experiment\r\n"
	.byte 0
msg_before:
	.ascii "before enable, A20 is: "
	.byte 0
msg_enabling:
	.ascii "enabling A20 via 8042 port 0x64/0x60...\r\n"
	.byte 0
msg_after:
	.ascii "after enable,  A20 is: "
	.byte 0
msg_skip:
	.ascii "A20 is already enabled, skip.\r\n"
	.byte 0
msg_on:
	.ascii "ON \r\n"
	.byte 0
msg_off:
	.ascii "OFF\r\n"
	.byte 0

# 填充到 510 字节，再写入引导扇区有效标志 0xAA55
.org 510
	.word 0xaa55
