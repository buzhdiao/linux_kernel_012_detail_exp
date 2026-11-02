# 3P1 开启分页实验的构建脚本
#
# 四个组件：
#   bootsect.S —— 16 位引导扇区（沿用 2P4）
#   setup.S    —— 16 位 setup，进入保护模式（沿用 2P4）
#   head.S     —— 32 位内核头：重装段、设 IDT/GDT、开分页（本章主角）
#   main.c     —— 第一个 C 函数：验证分页后停机

# 16 位部分
AS := as
LD := ld -m elf_x86_64
# 32 位部分
AS32 := as --32
LD32 := ld -m elf_i386
LDFLAG := -Ttext 0x0 -s --oformat binary

# 内核 C 代码的编译 flags（与官方仓 kernel/Makefile 一致的思路）：
#   -m32           生成 32 位代码
#   -ffreestanding 独立环境（没有标准库、没有 main 之外的运行时假定）
#   -fno-pic       内核不用位置无关代码
#   -fno-stack-protector  关闭栈保护（内核里没有 TLS 支持）
#   -fno-builtin   不让 gcc 用内置函数替换我们的代码
#   -fno-reorder-functions  禁止把函数拆到 .text.startup 等独立段
KCFLAGS := -m32 -ffreestanding -fno-pic -fno-stack-protector -fno-builtin -fno-reorder-functions -O2

all: linux.img

linux.img: tools/build bootsect setup system
	./tools/build bootsect setup system > $@

# head.S 和 main.c 链接成 system：用 kernel.lds 保证 head.o 在最前，
# startup_32 位于 0x0、pg_dir/pg0~pg3 的 .org 布局生效
system: head.o main.o kernel.lds
	$(LD32) -T kernel.lds -e startup_32 -s --oformat binary -o $@ head.o main.o

head.o: head.S
	$(AS32) -o $@ $<

main.o: main.c
	gcc $(KCFLAGS) -c -o $@ $<

tools/build: tools/build.c
	gcc -o $@ $<

bootsect: bootsect.o
	$(LD) $(LDFLAG) -o $@ $<

bootsect.o: bootsect.S
	$(AS) -o $@ $<

setup: setup.o
	$(LD) $(LDFLAG) -e _start_setup -o $@ $<

setup.o: setup.S
	$(AS) -o $@ $<

clean:
	rm -f *.o bootsect setup system linux.img tools/build

.PHONY: all clean
