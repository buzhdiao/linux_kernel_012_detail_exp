AS := as
LD := ld -m elf_x86_64

LDFLAG := -Ttext 0x0 -s --oformat binary

linux.img : bootsect.o
	$(LD) $(LDFLAG) -o $@ $<

bootsect.o : bootsect.S
	$(AS) -o $@ $<

clean:
	rm -f *.o
	rm -f bootsect
	rm -f linux.img