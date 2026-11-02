# =====================================================================
# 3P1 开启分页管理：head.S —— 内核真正的起点
# 对应书中 3.1 节"开启分页管理"（3.1.1 设置页表 / 3.1.2 设置栈指针）
#
# 执行到这里时机器的状态（由 setup.S 准备好）：
#   - 已在保护模式，CS = 0x08（setup 的 GDT：基址 0、限长 8MB 的代码段）
#   - system 已被搬到物理地址 0x0，本程序从 0x0 开始执行
#   - 8259A 已重映射到 0x20 起，所有中断仍被屏蔽
#
# 本程序的任务（按执行顺序）：
#   1. 重装段寄存器，加载内核自己的栈（stack_start）
#   2. 设置 IDT 和 GDT（内核要用自己的表，不依赖 setup 的）
#   3. 检查 A20 是否真的开启（0 与 1MB 地址是否重叠）
#   4. 设置页目录 + 4 个页表，开启 CR0.PG，从此运行在分页机制之上
#   5. 用"伪造返回地址"的技巧跳进 C 语言写的 main()
#
# 内存布局（本文件用 .org 精确定位）：
#   0x0000 ~ 0x0FFF  pg_dir  页目录（4KB，运行时清零后填充）
#                    注意 startup_32 的代码也从 0x0 开始——这一段代码
#                    执行完就"死去"，内存被回收成页目录
#   0x1000 ~ 0x4FFF  pg0~pg3 四个页表（16MB 恒等映射）
#   0x5000 ~ 0x53FF  tmp_floppy_area 软盘临时缓冲
#   0x5400 起         真正长命的代码：after_page_tables、setup_paging、
#                    IDT、GDT 都在这一段——所以 setup_paging 把
#                    0x0~0x5000 清零时不会踩到自己
#   0x6000 ~ 0x6FFF  内核栈（4KB），栈顶 0x7000
# =====================================================================

.code32
.text
.globl startup_32, idt, gdt, pg_dir, tmp_floppy_area

# 页目录从 0x0 开始；startup_32 的入口偏移也是 0
pg_dir:
startup_32:
	# ---------- 1. 重装段寄存器 ----------
	# setup 的 GDT 里数据段选择子是 0x10（基址 0、限长 8MB）
	# 远跳转只更新了 CS，其余段寄存器必须全部重装
	movl $0x10, %eax
	movw %ax, %ds			# DS = 数据段
	movw %ax, %es			# ES = 数据段
	movw %ax, %fs			# FS = 数据段
	movw %ax, %gs			# GS = 数据段（显存段稍后不再需要，先用数据段占位）

	# ---------- 2. 加载内核栈 ----------
	# lss 从内存读 6 字节：低 4 字节 → ESP（栈顶指针），高 2 字节 → SS（栈段选择子）
	# stack_start 定义在本文件末尾：指向 0x7000，段选择子 0x10
	lss stack_start, %esp

	# 设置内核自己的 IDT 和 GDT（此时先全部指向"未用中断"处理程序）
	call setup_idt
	call setup_gdt

	# 加载新 GDT 后段基址可能变化，段寄存器按新 GDT 再装一遍
	movl $0x10, %eax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs

	# GS 改用新 GDT 的显存段（第 4 项，基址 0xB8000），便于直接写屏
	movl $0x18, %eax
	movw %ax, %gs

	# ---------- 3. 检查 A20 是否真的开启 ----------
	# 思路：往 0x0 写一个递增的值，然后比较 1MB 处（0x100000）的值：
	#   若两处始终相等，说明地址回卷（A20 没开成），死循环卡在这里
	#   若两处出现差异，说明 1MB 以上是独立内存，检查通过
	xorl %eax, %eax
1:
	incl %eax			# EAX 从 1 开始递增
	movl %eax, 0x000000		# 写到物理地址 0
	cmpl %eax, 0x100000		# 和 1MB 处比较
	je 1b				# 相等就继续循环等待（A20 未生效）

	# ---------- 4. 准备进入 main ----------
	# 伪造 main() 的返回地址和参数，见 after_page_tables 处的说明
	jmp after_page_tables

# ---------------------------------------------------------------------
# 设置 IDT：把 256 项全部填成指向 ignore_int 的中断门后 lidt
# 与 2P4 clock.S 里的手法完全一致（都源自这份代码）
# ---------------------------------------------------------------------
setup_idt:
	leal ignore_int, %edx		# EDX = ignore_int 的地址
	movl $0x00080000, %eax		# EAX = 选择子 0x08 << 16
	movw %dx, %ax			# EAX = 0x0008<<16 | 偏移低 16 位
	movw $0x8e00, %dx		# EDX = 偏移高 16 位 | 类型 0x8E（中断门）
	leal idt, %edi			# EDI = IDT 表首
	movl $256, %ecx			# 填 256 项
rp_sidt:
	movl %eax, (%edi)		# 表项低 4 字节
	movl %edx, 4(%edi)		# 表项高 4 字节
	addl $8, %edi			# 每项 8 字节
	decl %ecx
	jne rp_sidt
	lidt idt_descr			# 加载 IDTR
	ret

# ---------------------------------------------------------------------
# 设置 GDT：加载内核自己的 GDT
# ---------------------------------------------------------------------
setup_gdt:
	lgdt gdt_descr
	ret

# ---------- 页表区域（运行时填充，见 setup_paging） ----------
.org 0x1000
pg0:
.org 0x2000
pg1:
.org 0x3000
pg2:
.org 0x4000
pg3:

.org 0x5000
# 软盘 DMA 临时缓冲区（后续章节的软盘驱动使用）
tmp_floppy_area:
	.fill 1024, 1, 0

# ---------------------------------------------------------------------
# 进入 main 前的准备工作
#
# pushl $main 让 setup_paging 结尾的 ret 把 main 当成"返回地址"弹给 EIP，
# 这是一个不写 jmp 就跳转的小技巧；main 前压入的 4 个 0 是给它的参数
# （envp、argv、argc，内核不用，纯占位）；L6 是 main 若"返回"的死循环
# ---------------------------------------------------------------------
after_page_tables:
	pushl $0			# main 的第 3 个参数（占位）
	pushl $0			# main 的第 2 个参数（占位）
	pushl $0			# main 的第 1 个参数（占位）
	pushl $L6			# main 的"返回地址"（实际返回就死循环）
	pushl $main			# 真正的跳转目标
	jmp setup_paging		# 设置分页，其结尾的 ret 会弹出 main

L6:
	jmp L6

# ---------------------------------------------------------------------
# 未用中断的兜底处理程序：在屏幕第 1 行写个 'I'，发 EOI 后返回
# （官方仓的版本注释了 call _printk 的位置——那要等 3P2 实现 printk）
# ---------------------------------------------------------------------
ignore_int:
	pushl %eax
	pushl %ecx
	pushl %edx
	pushw %ds
	pushw %es
	pushw %fs
	movl $0x10, %eax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs
	movl $0x96, %edi		# 第 1 行第 75 列附近
	movb $'I', %al
	movb $0x0c, %ah			# 红色
	movw %ax, %gs:(%edi)
	popw %fs
	popw %es
	popw %ds
	popl %edx
	popl %ecx
	popl %eax
	iret

# ---------------------------------------------------------------------
# 开启分页：全书到此为止最重要的一段初始化代码
#
# 两级页表（i386）：
#   页目录（4KB，1024 项）→ 每项指向一个页表
#   页表  （4KB，1024 项）→ 每项指向一个 4KB 物理页
#   合计一个页目录可管理 1024*1024*4KB = 4GB 线性空间
#
# 本内核只用 4 个页表，把线性地址 0~16MB 恒等映射（线性地址 == 物理地址）
# 到物理内存 0~16MB，内核代码数据的地址在开分页前后保持不变
# ---------------------------------------------------------------------
.align 4
setup_paging:
	# 第一步：把页目录 + 4 个页表（共 5 页，20KB）全部清零
	# 清零范围 0x0~0x5000，正在执行的代码在 0x5400 以上，不会被破坏
	movl $1024*5, %ecx		# 5 页 * 每页 1024 个长字
	xorl %eax, %eax			# 写入的值：0
	xorl %edi, %edi			# 从地址 0 开始（pg_dir 在 0x0）
	cld				# 串操作方向：向高地址递增
	rep
	stosl				# ES:[EDI] ← EAX，重复 ECX 次

	# 第二步：填页目录。低 3 位是页属性：111 = 页存在(P) + 可写(RW) + 用户态(US)
	# 页目录的第 0~3 项分别指向 pg0~pg3 这四个页表
	movl $pg0 + 7, pg_dir		# 第 0 项 → pg0（管线性 0~4MB）
	movl $pg1 + 7, pg_dir + 4	# 第 1 项 → pg1（管线性 4~8MB）
	movl $pg2 + 7, pg_dir + 8	# 第 2 项 → pg2（管线性 8~12MB）
	movl $pg3 + 7, pg_dir + 12	# 第 3 项 → pg3（管线性 12~16MB）

	# 第三步：填 4 个页表共 4096 项，从 16MB 处的最后一项往前填
	# 最后一项（pg3 的第 1023 项）映射物理页 0xFFF000（16MB-4KB），
	# 属性 0x007 = 存在 + 可写 + 用户；每往前一项，物理页号减 1
	movl $pg3 + 4092, %edi		# EDI = pg3 的最后一项地址
	movl $0xfff007, %eax		# 页表项：物理页号 0xFFF | 属性 7
	std				# 串操作方向：向低地址递减
1:
	stosl				# *EDI = EAX；EDI -= 4
	subl $0x1000, %eax		# 物理页号减 1（页号在高位，每项差 0x1000）
	jge 1b				# 直到 EAX 的页号部分减到 0，共写 4096 项
	xorl %eax, %eax

	# 第四步：让 CR3 指向页目录（0x0），再置 CR0 的 PG 位（bit31）打开分页
	movl %eax, %cr3			# CR3 = 页目录基址
	movl %cr0, %eax
	orl $0x80000000, %eax		# 置 PG 位
	movl %eax, %cr0			# 从下一条指令起，所有地址都经页表转换

	ret				# 弹出栈顶的 main 地址，进入 main()

# ---------- IDT/GDT 的位置和界限（lidt/lgdt 的操作数） ----------
.align 4
	.word 0
idt_descr:
	.word 256*8-1			# 界限 = 256 项 * 8 字节 - 1
	.long idt			# 基地址

.align 4
	.word 0
gdt_descr:
	.word 256 * 8 - 1
	.long gdt

.align 8
idt:
	.fill 256, 8, 0			# IDT 本体：先清零，运行时填

# 内核自己的 GDT：
#   0x00 空 descriptor（CPU 规定第 0 项必须为空）
#   0x08 代码段：基址 0，限长 0xFFF 个 4KB 页 = 16MB，可执行
#   0x10 数据段：基址 0，限长 16MB，可读写
#   0x18 显存段：基址 0xB8000，DPL=3（将来用户程序也能写屏）
gdt:
	.quad 0x0000000000000000
	.quad 0x00c09a0000000fff	# 代码段：P=1 DPL=0 S=1 Type=0xA（代码），G=1 D=1
	.quad 0x00c0920000000fff	# 数据段：Type=0x2（可读写）
	.quad 0x00c0f20b8000ffff	# 显存段：基址 0xB8000，限长 0xFFF 页（G=1），DPL=3
	.quad 0x0000000000000000
	.fill 251, 8, 0			# 预留：后续进程的 LDT/TSS 描述符用

# ---------- 内核栈（紧跟 GDT 之后顺序排布，约 0x65C0~0x75C0） ----------
# 栈向低地址增长，stack_top 是栈底（最高地址）；4KB 对内核初始化阶段够用
kernel_stack:
	.fill 4096, 1, 0
stack_top:

# lss stack_start,%esp 的操作数：4 字节栈顶偏移 + 2 字节栈段选择子
.align 4
stack_start:
	.long stack_top
	.word 0x10
