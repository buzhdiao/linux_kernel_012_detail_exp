# 定义汇编器变量 AS，指定使用 as 作为汇编器
AS := as
# 定义链接器变量 LD，指定使用 ld 链接器，并设置链接的目标架构为 x86_64 的 ELF 格式
LD := ld -m elf_x86_64

# 定义链接器标志 LDFLAG
# -Ttext 0x0 表示将代码段的起始地址设置为 0x0
# -s 表示去掉符号表，减少生成文件的大小
# --oformat binary 表示输出二进制格式的文件
LDFLAG := -Ttext 0x0 -s --oformat binary

# 定义最终目标 image，依赖于 linux.img
# 当执行 make image 时，会先检查 linux.img 是否需要更新
image : linux.img

# 定义 linux.img 的生成规则，它依赖于 tools/build、bootsect 和 setup
# 使用 tools/build 工具处理 bootsect 和 setup 文件，并将结果输出到 linux.img 文件中
linux.img : tools/build bootsect setup
	./tools/build bootsect setup > $@

# 定义 tools/build 的生成规则，它依赖于 tools/build.c
# 使用 gcc 编译器将 tools/build.c 编译成可执行文件 tools/build
tools/build : tools/build.c
	gcc -o $@ $<

# 定义 bootsect 的生成规则，它依赖于 bootsect.o
# 使用链接器 LD 并结合链接器标志 LDFLAG 将 bootsect.o 链接成 bootsect 文件
bootsect : bootsect.o
	$(LD) $(LDFLAG) -o $@ $<

# 定义 bootsect.o 的生成规则，它依赖于 bootsect.S
# 使用汇编器 AS 将 bootsect.S 汇编成 bootsect.o 目标文件
bootsect.o : bootsect.S
	$(AS) -o $@ $<

# 定义 setup 的生成规则，它依赖于 setup.o
# 使用链接器 LD 并结合链接器标志 LDFLAG，同时指定入口点为 _start_setup，将 setup.o 链接成 setup 文件
setup : setup.o
	$(LD) $(LDFLAG) -e _start_setup -o $@ $<

# 定义 setup.o 的生成规则，它依赖于 setup.S
# 使用汇编器 AS 将 setup.S 汇编成 setup.o 目标文件
setup.o : setup.S
	$(AS) -o $@ $<

# 定义清理规则 clean
# 用于删除生成的目标文件、可执行文件和镜像文件
# 执行 make clean 时，会删除所有以 .o 结尾的文件、bootsect、setup、tools/build 和 linux.img
clean:
	rm -f *.o
	rm -f bootsect
	rm -f setup
	rm -f tools/build
	rm -f linux.img