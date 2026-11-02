# 7P3 管理普通文件（open/read/close 完整链路）实验的构建脚本
# 在 3P2 基础上新增：traps.c + asm.S（异常桩）、memory.c（mem_map）、syscall.c

AS := as
LD := ld -m elf_x86_64
AS32 := as --32
LD32 := ld -m elf_i386
LDFLAG := -Ttext 0x0 -s --oformat binary

KCFLAGS := -m32 -ffreestanding -fno-pic -fno-stack-protector -fno-builtin -fno-reorder-functions -O2

OBJS := head.o asm.o page.o main.o console.o vsprintf.o printk.o traps.o memory.o fork.o keyboard.o tty.o hd.o floppy.o fs.o syscall.o sched.o

all: linux.img

linux.img: tools/build bootsect setup system
	./tools/build bootsect setup system > $@

# 用 kernel.lds 保证 head.o 在最前，startup_32 位于 0x0
system: $(OBJS) kernel.lds
	$(LD32) -T kernel.lds -e startup_32 -s --oformat binary -o $@ $(OBJS)

head.o: head.S
	$(AS32) -o $@ $<

page.o: page.S
	$(AS32) -o $@ $<

asm.o: asm.S
	$(AS32) -o $@ $<

%.o: %.c kernel.h sched.h
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
