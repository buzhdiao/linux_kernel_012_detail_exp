AR  	:= ar
GCC		:= gcc
CCFLAG 	:= -m32 -I./include -ffreestanding -fno-pic -Wall -fomit-frame-pointer -fno-stack-protector -c

runtime/libc.a: runtime/*.c
	cd runtime; make; cd ..;

et: tcc runtime/libc.a test/test.c
	./tcc test/test.c et
	cp et ../mkfs/

clean:
	-rm *.o
	-rm et
	cd runtime; make clean; cd ..;

