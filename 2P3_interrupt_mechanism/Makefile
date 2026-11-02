# 定义汇编器和链接器（与其他实验保持一致）
AS := as
LD := ld -m elf_x86_64

# -Ttext 0x0: 代码段从 0x0 开始
# -s: 去除符号表
# --oformat binary: 输出纯二进制
LDFLAG := -Ttext 0x0 -s --oformat binary

OBJ_FILES := bootsect.o
TARGET := linux.img

all: $(TARGET)

$(TARGET): $(OBJ_FILES)
	$(LD) $(LDFLAG) -o $@ $<

%.o: %.S
	$(AS) -o $@ $<

clean:
	rm -f $(OBJ_FILES)
	rm -f $(TARGET)

.PHONY: all clean
