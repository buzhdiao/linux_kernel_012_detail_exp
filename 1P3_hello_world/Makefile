# 定义汇编器和链接器
AS := as
# -m 后面跟的是目标机器设备的名称
LD := ld -m elf_x86_64

# 定义链接器标志
# -Ttext 0x0: 设置代码段起始地址为 0x0
# -s: 去除符号表和调试信息
# --oformat binary: 输出二进制格式
LDFLAG := -Ttext 0x0 -s --oformat binary

# 目标文件列表
OBJ_FILES := bootsect.o

# 最终生成的镜像文件
TARGET := linux.img

# 默认目标，生成镜像文件
all: $(TARGET)

# 生成镜像文件的规则
$(TARGET): $(OBJ_FILES)
	$(LD) $(LDFLAG) -o $@ $<

# 生成目标文件的规则
%.o: %.S
	$(AS) -o $@ $<

# 清理规则，删除中间文件和目标文件
clean:
	rm -f $(OBJ_FILES)
	rm -f $(TARGET)

# 防止 Makefile 中出现与文件名冲突的情况
.PHONY: all clean
