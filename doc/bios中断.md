# BIOS中断

bios提供了一组服务，可以帮助我们操纵硬件，避免我们直接与硬件细节打交道.
![bios中断](./bios中断示意图.png)
当触发软中断时，会自动从中断向量表中取出想用的中断程序的首地址，来执行中断程序，参数通过寄存器传递

# 0x10号中断
参考:https://www.cnblogs.com/kakafa/p/18312145


# 0x13号中断
作用是从磁盘中读数据到内存中。
对于int 0x13, 注意，intel CPU是 little endian的。
ah: 功能号 0x02--读磁盘数据到内存
al: 需要读出的扇区的数量
ch: 磁道（柱面）号的低8位
cl: 开始扇区（0-5bit），磁道号的高两位（6-7bit）
dh: 磁头号， dl: 驱动器号（if it's hard-disk, set the 7th bit)
es:bx: 目的地址。指向数据缓冲区；如果出错则CF标志置位。

load_setup:
 mov dx,#0x0000  ! drive 0, head 0
 mov cx,#0x0002  ! sector 2, track 0
 mov bx,#0x0200  ! address = 512, in INITSEG
 mov ax,#0x0200+SETUPLEN ! service 2, nr of sectors
 int 0x13   ! read it
 jnc ok_load_setup  ! ok - continue
 mov dx,#0x0000
 mov ax,#0x0000  ! reset the diskette
 int 0x13
 j load_setup
ok_load_setup:
！ 下面是取磁盘驱动器参数，特别是每道的扇区数量。
mov dl, #0x00
mov ax,#0x0800 ! ah=8 is get drive parameters
int 0x13
mov ch,#0x00
seg cs
mov sectors, cx
mov ax, #INITSET
mov es, ax
ah=8，功能号--读取磁盘参数
bl=0， 驱动器号（如果时硬盘，则要置位7为1）

返回：
ax=0, bl=驱动器类型
ch=最大磁道号的低8位， cl=每磁道最大扇区数（0-5bit），最大磁道号高2位（6-7bit）
dh=最大磁头数， dl=驱动器数量
es:di， 软驱磁盘参数表。 因此有最后一句 mov es, ax恢复堆栈栈低