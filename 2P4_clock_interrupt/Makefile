# 2P4 时钟中断实验的构建脚本
#
# 三个组件：
#   bootsect.S —— 16 位引导扇区，负责加载 setup 和 system
#   setup.S    —— 16 位 setup，负责进入保护模式（A20/GDT/8259A/PE）
#   clock.S    —— 32 位系统程序，设置 IDT 并处理时钟中断

# 16 位部分用宿主机默认 as 即可（.code16 指令控制生成 16 位编码）
AS := as
# 32 位部分用 as --32 生成 32 位目标文件
AS32 := as --32
# 链接 bootsect/setup 时输出纯二进制
LD := ld -m elf_x86_64
# 链接 clock 时按 32 位 ELF 链接（代码段定在 0x0，因为 setup 会把 system 搬到 0x0）
LD32 := ld -m elf_i386
LDFLAG := -Ttext 0x0 -s --oformat binary

all: linux.img

# 用打包工具把三个部分拼成软盘镜像
linux.img: tools/build bootsect setup clock
	./tools/build bootsect setup clock > $@

# 32 位系统程序：链接到 0x0 地址
clock: clock.o
	$(LD32) $(LDFLAG) -o $@ $<

clock.o: clock.S
	$(AS32) -o $@ $<

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
	rm -f *.o bootsect setup clock linux.img tools/build

.PHONY: all clean
