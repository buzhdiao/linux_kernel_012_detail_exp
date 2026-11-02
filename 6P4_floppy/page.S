# =====================================================================
# 4P3 缺页中断入口（14 号异常，有硬件错误码）
# 对应书中 4.3 节"页保护中断"，来源：官方仓 mm/page.S（逐行注释版）
#
# 14 号异常（Page Fault）的三个信息来源：
#   错误码 —— CPU 自动压栈：bit0=1 页存在(写保护错) / bit0=0 页不存在
#            bit1=1 是写操作引发，bit2=1 用户态引发
#   CR2    —— 故障的线性地址（CPU 专设寄存器）
#   压栈的 EIP —— 引发缺页的指令地址
#
# 汇编桩的职责：把非标准的"错误码栈帧"整理成 C 函数喜欢的参数布局，
# 然后按错误码 bit0 分发：写保护 → do_wp_page（fork 的写时复制）；
# 页不存在 → do_no_page（按需调页，7P6 实现）
#
# 技巧：xchgl %eax,(%esp) 把"栈顶的错误码"和"eax 里可能有用的值"交换，
# 一条指令完成"取错误码 + 保存原 eax"，避免多压一次栈
# =====================================================================

.globl page_fault

page_fault:
	xchgl	%eax, (%esp)		# EAX = 错误码，栈上原错误码位置存原 EAX
	pushl	%ecx
	pushl	%edx
	pushl	%ds
	pushl	%es
	pushl	%fs
	movl	$0x10, %edx		# 装内核数据段
	movw	%dx, %ds
	movw	%dx, %es
	movw	%dx, %fs
	movl	%cr2, %edx		# EDX = 故障线性地址
	pushl	%edx			# 第 2 参数：address
	pushl	%eax			# 第 1 参数：error_code
	testl	$1, %eax		# bit0 = 1？页存在（写保护）
	jne	1f
	call	do_no_page		# 页不存在：按需调页（7P6 实现）
	jmp	2f
1:	call	do_wp_page		# 写保护：写时复制
2:	addl	$8, %esp		# 清掉两个参数
	popl	%fs
	popl	%es
	popl	%ds
	popl	%edx
	popl	%ecx
	popl	%eax			# 恢复原 EAX（在栈上换过位置）
	iret				# 返回引发缺页的指令重试（COW 后页已可写）
