AR  := ar
LD  := ld
GCC := gcc
CCFLAG := -m32 -I../include -nostdinc -ffreestanding -fno-pic -Wall -fomit-frame-pointer -fno-stack-protector -c
OBJS   := string.o ctype.o file.o open.o dup.o execve.o close.o wait.o _exit.o setsid.o

lib.a : $(OBJS)
	$(AR) rcs $@ $^
	sync

string.o : string.c
	$(GCC) $(CCFLAG) -o $@ $<

ctype.o : ctype.c
	$(GCC) $(CCFLAG) -o $@ $<

file.o : file.c
	$(GCC) $(CCFLAG) -o $@ $<

open.o : open.c
	$(GCC) $(CCFLAG) -o $@ $<

dup.o : dup.c
	$(GCC) $(CCFLAG) -o $@ $<

close.o : close.c
	$(GCC) $(CCFLAG) -o $@ $<

execve.o : execve.c
	$(GCC) $(CCFLAG) -o $@ $<

wait.o : wait.c
	$(GCC) $(CCFLAG) -o $@ $<

_exit.o : _exit.c
	$(GCC) $(CCFLAG) -o $@ $<

setsid.o : setsid.c
	$(GCC) $(CCFLAG) -o $@ $<

clean : 
	rm -f *.o
	rm -f lib.a

