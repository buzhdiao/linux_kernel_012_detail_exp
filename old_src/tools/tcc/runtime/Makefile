AR  	:= ar
GCC		:= gcc
CCFLAG 	:= -m32 -I../include -ffreestanding -fno-pic -Wall -fomit-frame-pointer -fno-stack-protector -c
OBJS	:= sys_call.o errno.o printf.o vsprintf.o string.o crt0.o

libc.a: $(OBJS)
	$(AR) rcs $@ $^
	sync

sys_call.o: sys_call.c
	$(GCC) $(CCFLAG) -o $@ $<

errno.o: errno.c
	$(GCC) $(CCFLAG) -o $@ $<

printf.o: printf.c
	$(GCC) $(CCFLAG) -o $@ $<

vsprintf.o: vsprintf.c
	$(GCC) $(CCFLAG) -o $@ $<

string.o: string.c
	$(GCC) $(CCFLAG) -o $@ $<

crt0.o: crt0.S
	$(GCC) $(CCFLAG) -o $@ $<

clean:
	-rm *.o
	-rm libc.a

