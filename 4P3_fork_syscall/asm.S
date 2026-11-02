# =====================================================================
# 3P3 异常处理入口桩（汇编部分）
# 对应书中 3.3.3 "处理CPU异常" 和 3.3.2 "初始化系统调用"
#
# 官方仓里这些桩在 kernel/asm.S（异常/系统调用/fork 的入口），
# 本实验取其中最小集合：除零错误、非法操作码、双重故障、系统调用
#
# 为什么需要汇编桩？CPU 通过 IDT 进来时压入的现场（EIP/CS/EFLAGS，
# 有错误码的异常还多压一个 error code）不满足 C 调用约定，
# 必须用汇编调整栈布局后再调用 C 函数处理
# =====================================================================

.code32
.text

# ---------------------------------------------------------------------
# 无错误码的异常桩（divide_error、invalid_op、system_call 等）：
# CPU 只压了 EIP/CS/EFLAGS，没有 error code，统一补压一个 0，
# 让所有异常的栈布局一致，方便 C 侧统一读取
# ---------------------------------------------------------------------

# 0 号异常：除零错误（fault，返回地址指向出错指令本身）
.globl divide_error
divide_error:
	pushl $0			# 补一个假的错误码
	pushl %ds
	pushl %es
	pushl %fs
	pushl %eax
	pushl %ecx
	pushl %edx
	pushl %ebx
	pushl %esi
	pushl %edi
	pushl %ebp
	movl $0x10, %eax		# DS/ES/FS 装入内核数据段
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs
	pushl %esp			# 把"保存现场数组的地址"作为参数传给 C 函数
	call do_divide_error		# C 处理函数（打印现场后停机）
	addl $4, %esp			# 清掉参数
	popl %ebp
	popl %edi
	popl %esi
	popl %ebx
	popl %edx
	popl %ecx
	popl %eax
	popl %fs
	popl %es
	popl %ds
	addl $4, %esp			# 丢弃补压的假错误码
	iret

# 6 号异常：非法操作码（fault）
.globl invalid_op
invalid_op:
	pushl $0
	pushl %ds
	pushl %es
	pushl %fs
	pushl %eax
	pushl %ecx
	pushl %edx
	pushl %ebx
	pushl %esi
	pushl %edi
	pushl %ebp
	movl $0x10, %eax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs
	pushl %esp
	call do_invalid_op
	addl $4, %esp
	popl %ebp
	popl %edi
	popl %esi
	popl %ebx
	popl %edx
	popl %ecx
	popl %eax
	popl %fs
	popl %es
	popl %ds
	addl $4, %esp
	iret

# ---------------------------------------------------------------------
# 0x80 号：系统调用入口
# 约定（沿用官方）：EAX = 系统调用号，返回值也放 EAX
# 官方仓 kernel/sys_call.S 的 system_call 在此基础上还有信号处理、
# 调度判断等逻辑，第 4 章逐步引入；本实验只演示"int 0x80 能进来、
# 能取到调用号、能返回"这一最小机制
# ---------------------------------------------------------------------
.globl system_call
system_call:
	# ---- 硬件已压（用户态 int 0x80，CPL 变化）：eip,cs,eflags,esp,ss ----
	# ---- 按 pt_regs 布局补压其余寄存器：先压高地址的，最后压的落在低处 ----
	# 压栈顺序精心设计：最后压的落在最低地址，必须与 sched.h 的
	# pt_regs 布局一致（ebx 在最前，eax 在其后）——顺序压错的话
	# C 侧读到的"参数"其实是别的寄存器（本项目实际踩过：
	# sys_dispatch 收到的 ebx 是调用号 4）
	pushl %ds
	pushl %es
	pushl %fs
	pushl %gs
	pushl %ebp
	pushl %edi
	pushl %esi
	pushl %eax
	pushl %edx
	pushl %ecx
	pushl %ebx
	movl $0x10, %edx		# 内核数据段装入段寄存器
	movw %dx, %ds
	movw %dx, %es
	movw %dx, %fs
	movw %dx, %gs
	# ---- 调 C 分发器 sys_dispatch(nr, regs)（cdecl：先压 arg2 再压 arg1）----
	pushl %esp			# arg2: regs = 现场数组的地址
	pushl 16(%esp)			# arg1: nr = 保存的 EAX（因刚压了 4 字节而 +4）
	call sys_dispatch
	addl $8, %esp			# 清参数
	# ---- 按 pt_regs 相反顺序恢复（EAX 可能已被写回 fork 返回值）----
	popl %ebx
	popl %ecx
	popl %edx
	popl %eax
	popl %esi
	popl %edi
	popl %ebp
	popl %gs
	popl %fs
	popl %es
	popl %ds
	iret				# 返回用户态（子进程则从自己的 EIP 复活）

# ---------------------------------------------------------------------
# 0x20 号：时钟中断入口（IRQ0，8259A 重映射后）
# 流程与普通中断一致：存现场 → 调 C 的 do_timer（时间片--，可能调度）→
# EOI → 恢复现场 → iret
# 注意：如果 do_timer 里发生了任务切换，iret 弹出的就是"新进程"
# 进入中断前压栈保存的现场——这正是多任务交替运行的机制
# ---------------------------------------------------------------------
.globl timer_interrupt
timer_interrupt:
	pushl %ds
	pushl %es
	pushl %fs
	pushl %eax
	pushl %ecx
	pushl %edx
	pushl %ebx
	pushl %esi
	pushl %edi
	pushl %ebp
	movl $0x10, %eax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs
	# EOI 必须在 do_timer 之前发！do_timer 里的 schedule() 可能触发
	# 任务切换（ljmp 新 TSS），CPU 一去不回——若此时 ISR 位还挂着，
	# 8259A 会阻塞后续所有 IRQ0，新进程的时间片永远走不动，死锁。
	# 先 EOI 再处理，本中断被新中断嵌套的极端情况由关中断保护
	movb $0x20, %al			# EOI：8259A 主片
	outb %al, $0x20
	call do_timer
	popl %ebp
	popl %edi
	popl %esi
	popl %ebx
	popl %edx
	popl %ecx
	popl %eax
	popl %fs
	popl %es
	popl %ds
	iret
