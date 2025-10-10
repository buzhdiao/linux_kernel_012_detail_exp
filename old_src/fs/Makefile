GCC := gcc
LD  := ld
CCFLAG := -I../include -nostdinc -ffreestanding -Wall -fomit-frame-pointer -c -fno-stack-protector -fno-pie -fno-pic -m32
LDFLAG := -Ttext 0x0 -s --oformat binary -m elf_i386
INCDIR := ../include
OBJS   := read_write.o ioctl.o buffer.o super.o file_table.o inode.o open.o namei.o bitmap.o \
	fcntl.o char_dev.o truncate.o exec.o file_dev.o pipe.o stat.o

fs.o : $(OBJS)
	$(LD) -m elf_i386 -r -o $@ $^

read_write.o : read_write.c
	$(GCC) $(CCFLAG) -o $@ $<

ioctl.o : ioctl.c
	$(GCC) $(CCFLAG) -o $@ $<

buffer.o : buffer.c
	$(GCC) $(CCFLAG) -o $@ $<

super.o : super.c
	$(GCC) $(CCFLAG) -o $@ $<

file_table.o : file_table.c
	$(GCC) $(CCFLAG) -o $@ $<

inode.o : inode.c
	$(GCC) $(CCFLAG) -o $@ $<

bitmap.o : bitmap.c
	$(GCC) $(CCFLAG) -o $@ $<

truncate.o : truncate.c
	$(GCC) $(CCFLAG) -o $@ $<

namei.o : namei.c
	$(GCC) $(CCFLAG) -o $@ $<

open.o : open.c
	$(GCC) $(CCFLAG) -o $@ $<

bitmap.o : bitmap.c
	$(GCC) $(CCFLAG) -o $@ $<

char_dev.o : char_dev.c
	$(GCC) $(CCFLAG) -o $@ $<

fcntl.o : fcntl.c
	$(GCC) $(CCFLAG) -o $@ $<

runcate.o : truncate.c
	$(GCC) $(CCFLAG) -o $@ $<

pipe.o : pipe.c
	$(GCC) $(CCFLAG) -o $@ $<

file_dev.o : file_dev.c
	$(GCC) $(CCFLAG) -o $@ $<

exec.o : exec.c
	$(GCC) $(CCFLAG) -o $@ $<

stat.o : stat.c
	$(GCC) $(CCFLAG) -o $@ $<

clean : 
	rm -f *.o

