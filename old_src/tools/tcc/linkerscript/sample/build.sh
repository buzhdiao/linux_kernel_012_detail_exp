#!/bin/bash
export LDEMULATION=elf_i386
gcc foo.c -c -fno-pic -m32
ld -T ../a.out.lds ../a.out.header foo.o -o a.out
