# 8P3 构建C语言库：系统调用封装 + 用户态 printf

对应书中 **8.3 构建C语言库**。来源：官方仓 `lib/` + `tools/tcc/runtime/`
的教学精简版。

## 库的三层结构

```
用户程序（hello_lib.c：printf/getpid/uptime 像普通 C 一样用）
    ↓ 库的接口（clib.h）
C 库（lib/clib.c：syscall 封装 + 用户态 printf 格式化）
    ↓ int 0x80（寄存器约定集中在一处）
内核系统调用（nr=8 write / 13 getpid / 14 uptime / 16 exit……）
```

**关键分层思想**：格式化在用户态做（库的 printf 把 %d/%s 展开到
缓冲区），只有最终的字符串经 write 系统调用进内核——真实 libc
正是这个分层。寄存器约定（EAX=号 EBX/ECX/EDX=参数）只出现在
库的 syscall() 一处，程序永不直接碰。

## 编译验证

```bash
cd lib
gcc -m32 -ffreestanding -fno-pic -fno-stack-protector -nostdlib -c clib.c hello_lib.c
ld -m elf_i386 -Ttext=0 --oformat binary -o hello_lib.bin hello_lib.o clib.o -e lib_main
# → hello_lib.bin (4556B)：可被 7P6 的 do_execve 加载执行
```

配合 7P6 的 exec：mkfs.py 把 hello_lib.bin 放进镜像即可
`do_execve("/hello_lib.bin")` —— 完整闭环：
**文件系统 → execve 加载 → C 库 printf → write 系统调用 → 控制台输出**。

## 与官方仓代码的对应关系

| 本实验 | 官方仓 linux_kernel_012 |
| ------ | ----------------------- |
| syscall() 封装 | `lib/` 里的 *_syscall 宏族 |
| printf | `tools/tcc/runtime/printf.c`（官方基于 vsprintf 完整版） |
| crt0/入口 | `tools/tcc/runtime/crt0.S`（本版 _start 由 ld 入口替代） |
