# =====================================================================
# 2P3 中断机制实验：实模式中断向量表 IVT + 8259A 的 EOI
# 对应书中 2.3 节"中断机制"（2.3.1 中断描述符表 IDT / 2.3.2 8259A 中断控制芯片 / 2.3.3 特权级）
#
# 实验内容：
#   1. 替换 IVT（中断向量表）中 0x08 号表项——实模式下 8259A 默认把
#      IRQ0（时钟）映射到中断号 0x08
#   2. 打开中断后，每次时钟中断到来，我们的处理程序让计数 +1
#      并直接写 VGA 显存显示，最后向 8259A 发 EOI 结束中断
#   3. 屏幕上可以看到一个约 18.2 次/秒不断增长的计数器
#
# 本实验仍在实模式下运行；保护模式下的 IDT 要到 2P4 才登场，
# 但"查表 → 进处理程序 → 发 EOI → iret"的流程两者完全一致
# =====================================================================

BOOTSEG  = 0x7c0

# IRQ0 时钟中断对应的中断号是 0x08，IVT 中每个表项 4 字节（偏移 2 字节 + 段值 2 字节）
IVT_IRQ0 = 0x08 * 4

# VGA 文本模式显存段地址，每字符 2 字节（ASCII + 属性）
VGA_SEG  = 0xb800
# 第 12 行行首的显存偏移：12 行 * 每行 80 字符 * 每字符 2 字节
ROW12    = 12 * 80 * 2

.code16
.text
.global _start

_start:
	# 规范化 CS:IP 为 0x7C0:0
	jmpl $BOOTSEG, $start

start:
	# DS、ES 指向本程序所在段
	movw %cs, %ax
	movw %ax, %ds
	movw %ax, %es
	# 栈设置在 0x7C00 以下
	movw $0, %ax
	movw %ax, %ss
	movw $0x7c00, %sp

	# 打印标题（电传方式逐字符输出）
	movw $msg_title, %si
	call print_str
	movw $msg_hint, %si
	call print_str

	# 在第 12 行打印计数器标签（用 int 0x10 的 13 号功能，指定行列显示）
	movw $msg_ticks, %ax
	movw %ax, %bp			# BP = 字符串地址
	movw $0x1301, %ax		# AH=0x13 显示字符串，AL=1 光标跟随移动
	movw $0x000c, %bx		# BH=页 0，BL=0x0C 红色
	movw $17, %cx			# 字符串长度 "IRQ0 tick count: " 共 17 字符
	movw $0x0c00, %dx		# DH=12 行，DL=0 列
	int $0x10

	# ---------- 安装中断处理程序：改写 IVT 的 0x08 号表项 ----------
	xorw %ax, %ax
	movw %ax, %ds			# DS = 0，IVT 位于内存最前端 0x0000:0x0000
	movw IVT_IRQ0, %ax		# 保存原来的偏移
	movw %ax, old_vec_off
	movw IVT_IRQ0+2, %ax		# 保存原来的段值
	movw %ax, old_vec_seg
	movw $clock_handler, %ax	# 写入新偏移（当前段内偏移）
	movw %ax, IVT_IRQ0
	movw %cs, %ax			# 写入新段值（本程序的段）
	movw %ax, IVT_IRQ0+2

	# DS 恢复为本程序数据段
	movw %cs, %ax
	movw %ax, %ds

	# ---------- 确保 8259A 主片没有屏蔽 IRQ0 ----------
	# 0x21 是主片的 IMR（中断屏蔽寄存器），某位为 1 表示对应 IRQ 被屏蔽
	inb  $0x21, %al
	andb $0xfe, %al			# 清除 bit0（IRQ0 时钟），其余保持不变
	outb %al, $0x21

	# ---------- 打开 CPU 中断标志 ----------
	sti

	# ---------- 主循环：停机等待中断 ----------
	# hlt 让 CPU 休眠，下一个中断到来时被唤醒；唤醒后继续 hlt
main_loop:
	hlt
	jmp main_loop

# ---------------------------------------------------------------------
# IRQ0 时钟中断处理程序（实模式下经 IVT 进入）
# 流程：保存现场 → 计数 +1 → 写显存显示 → 向 8259A 发 EOI → 恢复现场 → iret
# ---------------------------------------------------------------------
clock_handler:
	# 保存处理程序中用到的所有寄存器（中断随时可能打断任何代码）
	pushw %ax
	pushw %bx
	pushw %cx
	pushw %dx
	pushw %di
	pushw %ds
	pushw %es

	# DS 恢复为本程序数据段（中断打断别的代码时 DS 不确定）
	movw %cs, %ax
	movw %ax, %ds
	# ES 指向 VGA 显存段
	movw $VGA_SEG, %ax
	movw %ax, %es

	# 计数 +1
	incw tick_count
	movw tick_count, %ax

	# ---------- 把计数的 5 位十进制写到第 12 行第 17~21 列 ----------
	movw $10, %bx			# 除数 10
	movw $5, %cx			# 共 5 位数字
	movw $ROW12+42, %di		# 标签占 0~16 列，最右一位数字在第 21 列，从右往左填
convert_digit:
	xorw %dx, %dx			# DX:AX / BX → AX=商，DX=余数（当前的最低位）
	divw %bx
	addb $'0', %dl			# 余数转 ASCII 字符
	movb %dl, %es:(%di)		# 只写字符字节，保留原有属性字节
	subw $2, %di			# 显存中每个字符占 2 字节，向左移动一格
	loop convert_digit

	# ---------- 向 8259A 主片发送 EOI（End Of Interrupt） ----------
	# 不发 EOI 的话，8259A 认为中断还没处理完，不会再送出同级或更低级的中断
	movb $0x20, %al			# 0x20 = 非指定 EOI 命令
	outb %al, $0x20			# 0x20 是主片的命令端口

	# 恢复现场并中断返回（iret 会恢复 IP、CS、FLAGS）
	popw %es
	popw %ds
	popw %di
	popw %dx
	popw %cx
	popw %bx
	popw %ax
	iret

# ---------------------------------------------------------------------
# 打印字符串：SI = 字符串地址（0 结尾）
# ---------------------------------------------------------------------
print_str:
	pushw %ax
	pushw %bx
	pushw %si
	movw $0x0007, %bx
1:
	movb (%si), %al
	testb %al, %al
	jz 1f
	movb $0x0e, %ah
	int $0x10
	incw %si
	jmp 1b
1:
	popw %si
	popw %bx
	popw %ax
	ret

# ---------------------------------------------------------------------
# 数据区
# ---------------------------------------------------------------------
msg_title:
	.ascii "\r\n[2P3] interrupt mechanism: IVT + 8259A EOI\r\n"
	.byte 0
msg_hint:
	.ascii "IRQ0 timer -> int 0x08, counter runs at ~18.2 Hz\r\n"
	.byte 0
msg_ticks:
	.ascii "IRQ0 tick count: "	# 14 个字符，显示在第 12 行行首
	.byte 0

	.align 2
tick_count:
	.word 0				# 时钟中断计数器
old_vec_off:
	.word 0				# 原 IVT[0x08] 的偏移
old_vec_seg:
	.word 0				# 原 IVT[0x08] 的段值

.org 510
	.word 0xaa55
