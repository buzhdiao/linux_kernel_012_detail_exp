# =====================================================================
# 2P4 时钟中断实验：保护模式 + IDT + 8259A 重映射后的第一个 32 位程序
# 对应书中 2.4 节"时钟中断"
#
# 执行流程回顾：
#   bootsect.S  把本程序（system）加载到 0x10000 处
#   setup.S     把 system 从 0x10000 搬到 0x00000，开启 A20，加载 GDT，
#               重映射 8259A（IRQ0 → 中断号 0x20），屏蔽所有中断，
#               置 CR0.PE=1，远跳转到选择子 0x08:0x0 —— 正是本程序的入口
#
# 本程序的工作：
#   1. 重新加载各段寄存器（进入保护模式后必须立刻做）
#   2. 设置 IDT：256 项全部先指向 ignore_int，再把 0x20 号（IRQ0）指向时钟处理程序
#   3. 打开 8259A 主片的 IRQ0 屏蔽位，sti 开中断
#   4. 主循环 hlt 休眠；每次时钟中断，计数 +1 并写到屏幕上，发 EOI 后返回
#
# 与官方仓的对应：
#   setup_idt 的填表手法来自 kernel/head.S；
#   时钟处理程序对应 kernel/sys_call.S 中 timer_interrupt 的雏形
# =====================================================================

# setup.S 中 GDT 定义的选择子（段选择子 = 段描述符在 GDT 中的偏移）：
# 0x08 = 第 2 项：代码段（基址 0，可执行可读）
# 0x10 = 第 3 项：数据段（基址 0，可读写）
# 0x18 = 第 4 项：显存段（基址 0xB8000，可读写）
SEL_CODE  = 0x08
SEL_DATA  = 0x10
SEL_VIDEO = 0x18

# 时钟中断号：setup.S 已把主片 8259A 重映射到 0x20 起，IRQ0 对应 0x20
IRQ0_VEC  = 0x20

# 栈顶选择 0x9B000：位于 setup(0x90200) 之上、BIOS 数据区(0x9FC00) 之下的空闲内存
# 栈向低地址增长，可用范围约 0x90000~0x9B000
STACK_TOP = 0x9B000

# VGA 文本模式的行列换算：第 r 行第 c 列的显存偏移 = (r*80+c)*2
ROW9      = 9 * 80 * 2
ROW12     = 12 * 80 * 2

.code32
.text
.global _start_clock

_start_clock:
	# ---------- 1. 重装段寄存器 ----------
	# 远跳转后 CS 已是 0x08，其余段寄存器还残留实模式的值，必须全部重装
	movl $SEL_DATA, %eax
	movw %ax, %ds			# DS = 数据段（基址 0）
	movw %ax, %es			# ES = 数据段
	movw %ax, %fs			# FS = 数据段
	movw %ax, %ss			# SS = 数据段：中断发生时 CPU 自动切到当前栈
	movl $STACK_TOP, %esp		# ESP 指向栈顶
	movl $SEL_VIDEO, %eax
	movw %ax, %gs			# GS = 显存段（基址 0xB8000）

	# ---------- 2. 在屏幕上打出横幅，证明已在保护模式下运行 ----------
	movl $ROW9, %edi
	movl $msg_banner, %esi
	call print_str32

	movl $ROW12, %edi
	movl $msg_ticks, %esi
	call print_str32

	# ---------- 3. 设置 IDT ----------
	call setup_idt			# 先把 256 项全部填成 ignore_int
	call set_clock_gate		# 再把 0x20 号门改成时钟中断处理程序

	# ---------- 4. 打开 IRQ0 并开中断 ----------
	# setup.S 里向 0x21（主片 IMR）写了 0xFF 屏蔽了全部 IRQ，
	# 现在清除 bit0 只放开 IRQ0（时钟），其余仍然屏蔽
	inb  $0x21, %al
	andb $0xfe, %al
	outb %al, $0x21
	sti				# 设置 IF 标志，CPU 开始响应可屏蔽中断

	# ---------- 5. 主循环：休眠等待中断 ----------
	# hlt 让 CPU 停机省电，下一个中断（时钟）到来时被唤醒，醒来继续 hlt
main_loop:
	hlt
	jmp main_loop

# ---------------------------------------------------------------------
# 填充 IDT：把 256 个表项全部设为指向 ignore_int 的中断门
# 每项 8 字节的布局（中断门描述符）：
#   [0..1]  处理程序偏移低 16 位
#   [2..3]  段选择子（这里用代码段 0x08）
#   [4]     保留为 0
#   [5]     类型字节 0x8E：P=1 有效，DPL=0，TYPE=0xE 386 中断门
#   [6..7]  处理程序偏移高 16 位
# 手法与官方仓 kernel/head.S 的 setup_idt 完全一致：
#   EAX = 选择子<<16 | 偏移低16位 → 写入表项前 4 字节
#   EDX = 偏移高16位 | 0x8E00     → 写入表项后 4 字节
# ---------------------------------------------------------------------
setup_idt:
	leal ignore_int, %edx		# EDX = ignore_int 的地址
	movl $0x00080000, %eax		# EAX = 选择子 0x08 << 16
	movw %dx, %ax			# EAX = 0x0008<<16 | 偏移低 16 位
	movw $0x8e00, %dx		# EDX = 偏移高 16 位 | 类型字节 0x8E00
	leal idt, %edi			# EDI 指向 IDT 表首
	movl $256, %ecx			# 共 256 项
fill_idt:
	movl %eax, (%edi)		# 写表项低 4 字节
	movl %edx, 4(%edi)		# 写表项高 4 字节
	addl $8, %edi			# 下一项
	decl %ecx
	jne fill_idt
	lidt idt_descr			# 加载 IDTR：告知 CPU 表的位置和界限
	ret

# ---------------------------------------------------------------------
# 把 IDT 第 0x20 项（IRQ0 时钟）改为指向 clock_int
# ---------------------------------------------------------------------
set_clock_gate:
	leal clock_int, %edx
	movl $0x00080000, %eax
	movw %dx, %ax
	movw $0x8e00, %dx
	leal idt, %edi
	movl %eax, IRQ0_VEC*8(%edi)	# 第 0x20 项的地址 = idt + 0x20*8
	movl %edx, IRQ0_VEC*8+4(%edi)
	ret

# ---------------------------------------------------------------------
# 时钟中断处理程序（int 0x20，经 IDT 的中断门进入）
# 注意：中断门进入时 CPU 自动清 IF（关中断），iret 时自动恢复
# ---------------------------------------------------------------------
clock_int:
	pushl %eax			# 保存现场
	pushl %ebx
	pushl %ecx
	pushl %edx
	pushl %edi
	pushl %ds

	movw $SEL_DATA, %ax		# 中断可能打断任何代码，DS 重装为本程序数据段
	movw %ax, %ds

	incl tick_count			# 计数 +1
	movl tick_count, %eax

	# ---------- 把计数的 5 位十进制写到第 12 行第 17~21 列 ----------
	# GS 仍指向显存段（处理程序不会破坏 GS，且中断门进入时不会自动改 GS）
	movl $10, %ebx			# 除数 10
	movl $5, %ecx			# 5 位数字
	movl $ROW12+42, %edi		# 最右一位数字在第 21 列
convert_digit:
	xorl %edx, %edx			# EDX:EAX / EBX → EAX=商，EDX=余数（当前最低位）
	divl %ebx
	addb $'0', %dl			# 余数转 ASCII
	movb %dl, %gs:(%edi)		# 写显存（只写 ASCII 字节，属性字节保持不变）
	subl $2, %edi			# 向左移动一格（每字符 2 字节）
	loop convert_digit

	# ---------- 向 8259A 主片发 EOI ----------
	# 重映射后主片命令口仍是 0x20；0x20 = 非指定 EOI，表示中断服务结束
	movb $0x20, %al
	outb %al, $0x20

	popl %ds			# 恢复现场
	popl %edi
	popl %edx
	popl %ecx
	popl %ebx
	popl %eax
	iret				# 中断返回：依次弹出 EIP、CS、EFLAGS

# ---------------------------------------------------------------------
# 默认中断处理程序：未设置的中断都落到这里，在屏幕角落写个 'I' 提醒
# ---------------------------------------------------------------------
ignore_int:
	pushl %eax
	pushl %ecx
	movl $0x9c, %ecx		# 第 1 行第 78 列的位置
	movb $'I', %al
	movb $0x0c, %ah			# 红色
	movw %ax, %gs:(%ecx)
	movb $0x20, %al			# 同样要发 EOI，否则后续中断会被卡住
	outb %al, $0x20
	popl %ecx
	popl %eax
	iret

# ---------------------------------------------------------------------
# 32 位模式下的字符串打印：ESI = 字符串地址（0 结尾），EDI = 显存偏移
# 每个字符占 2 字节（ASCII + 属性），属性用 0x0A 绿色
# ---------------------------------------------------------------------
print_str32:
	pushl %eax
1:
	movb (%esi), %al
	testb %al, %al
	jz 1f
	movb $0x0a, %ah
	movw %ax, %gs:(%edi)
	addl $2, %edi			# 显存中前进一格
	incl %esi
	jmp 1b
1:
	popl %eax
	ret

# ---------------------------------------------------------------------
# 数据区
# ---------------------------------------------------------------------
	.align 4
msg_banner:
	.ascii "[2P4] now in protected mode, IDT works"	# 39 字符
	.byte 0
msg_ticks:
	.ascii "IRQ0 tick count: "				# 17 字符
	.byte 0

	.align 4
tick_count:
	.long 0				# 时钟中断计数器

	.align 8
idt:
	.fill 256, 8, 0			# IDT 本体：256 项 * 8 字节，先全部清零

idt_descr:
	.word 256*8-1			# 界限 = 表长 - 1
	.long idt			# 基地址（本程序链接在 0x0，idt 就是线性地址）
