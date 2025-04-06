# 定义引导段地址常量，引导扇区通常加载到物理地址 0x7c00，
# 这里 0x7c0 乘以 16 得到 0x7c00
BOOTSEG=0x7c0

# 声明接下来的代码是 16 位模式的代码
.code16
# 指定代码段
.text

# 声明 _start 为全局符号，作为程序入口点
.global _start
_start:
    # 执行远跳转，跳转到段地址cs为 BOOTSEG，偏移地址为 start2 的位置
    jmpl $BOOTSEG,$start2

start2:
    # 将 BOOTSEG 的值（0x7c0）传送到寄存器 ax
    movw $BOOTSEG,%ax
    # 将 ax 的值赋给数据段寄存器 ds，设置数据段的段地址
    movw %ax,%ds
    # 将 ax 的值赋给附加段寄存器 es
    movw %ax,%es
    # 将 ax 的值赋给 fs 段寄存器
    movw %ax,%fs
    # 将 ax 的值赋给 gs 段寄存器
    movw %ax,%gs

    # 将字符串 msg 的偏移地址传送到 ax 寄存器
    movw $msg,%ax
    # 将 ax 的值传送到 bp 寄存器，bp 作为显示字符串的地址指针
    movw %ax,%bp
    # 调用 BIOS 的 0x10 中断的 0x13 号功能，0x13表示在teletype模式下显示字符串
    movw $0x01301,%ax
    # 设置 bh 为页码 0，bl 为字符颜色（0x0c 通常是红色）
    movw $0x0c,%bx
    # 设置 cx 为要显示的字符串长度，这里是 12 个字符
    movw $12,%cx
    # 设置 dl 为显示字符串的列位置，这里设为 0
    movb $0,%dl
    # 调用 BIOS 的 0x10 号中断，进行屏幕显示操作
    int $0x010

loop:
    # 无限循环，程序执行到这里后会一直循环，防止程序继续执行不可预期的代码
    jmp loop

# 定义要显示的字符串
msg:
    .ascii "Hello World!"
# 填充字节，使代码段长度达到 510 字节
.org 510
# 引导扇区的标志，必须是 0xaa55 才能被 BIOS 识别为有效的引导扇区
boot_flag:
    .word 0xaa55
