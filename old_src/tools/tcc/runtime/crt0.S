.code32
.text

.globl _environ, _start

_start:
 movl 8(%esp),%eax
 movl %eax,_environ
 call main
 pushl %eax
1: call exit
 jmp 1b

.data

_environ:
 .long 0
