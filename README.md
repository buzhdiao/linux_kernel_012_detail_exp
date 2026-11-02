
#《从零开始写linux内核》实验代码及详解

参考源码：https://gitee.com/hinus/linux_kernel_012.git

# 写作目的


  《从零开始写linux内核》是一本很好的学习linux内核的书，它提供了一个从零搭建linux内核的实验舞台，可以详细了解linux内核各个关键特性的关键设计思路。值得好好学习，并动手实践。
  美中不足的是，这本书的作者海纳大佬，没有在书中直接标明源码的地址。笔者在学习的时候，经过多方查找，最终从知乎上，获得了作者配套的源码。下载阅读后，发现作者的源码，是一套最终版本的代码，而不是针对具体章节的。如果读者没有一定的基础，无法直接参考源码做实验。并且，书上摘录的源码和作者代码仓中的源码，有一些不一样的地方。
  在此背景下，笔者考虑，基于作者的源码仓和学习总结，建立一个对新手更加友好的代码仓。希望对大家的学习有一点用处。

# 实验平台

linux平台：基于WSL搭建的ubuntu 22.04版本
IDE： VS CODE
仿真工具： qemu

# 实验索引（按书的章节推进）

完整章节映射见 [doc/书籍目录与实验映射.md](./doc/书籍目录与实验映射.md)。
每个实验目录自成一体：`make` 生成 linux.img，`sh run.sh` 启动 qemu。

| 实验 | 对应书节 | 状态 | 一句话说明 |
| ---- | -------- | ---- | ---------- |
| 1P3_hello_world | 1.3 第一个内核程序 | ✅ | 引导扇区显示 hello world |
| 2P1_prepare_befor_protect_mode | 2.1 进入保护模式前的准备 | ✅ | bootsect 加载 setup、采集硬件信息（build.c 已修复 qemu 兼容） |
| 2P2_memory_manage | 2.2 内存管理 | ✅ | A20 地址线检测与开启（8042） |
| 2P3_interrupt_mechanism | 2.3 中断机制 | ✅ | 实模式 IVT + 8259A EOI，时钟计数器 |
| 2P4_clock_interrupt | 2.4 时钟中断 | ✅ | 完整进入保护模式，IDT + 时钟中断（★第一个32位实验） |
| 3P1_enable_paging | 3.1 开启分页管理 | ✅ | head.S：页目录/页表/栈，开启 CR0.PG 进入 main() |
| 3P2_format_print | 3.2 实现格式化打印 | ✅ | console + vsprintf + printk，换行回车滚屏 |
| 3P3_mem_trap_syscall | 3.3 设置内存和陷阱处理 | ✅ | mem_map 内存管理 + 除零异常现场 + int 0x80 骨架 |
| 4P1_create_init_process | 4.1 创建INIT进程 | ✅ | task_struct、TSS/LDT、move_to_user_mode（nr=15 验证通过） |
| 4P2_second_process | 4.2 创建第二个进程 | 🚧 骨架 | 复制 PCB、进程切换 |
| 4P3_fork_syscall | 4.3 第一个系统调用fork | 🚧 骨架 | fork + 写时复制 + 缺页中断 |
| 5P1_keyboard ~ 5P4_terminal_state | 第5章 支持字符设备 | 🚧 骨架 | 键盘扫描码、read/write、ANSI、termios |
| 6P1_harddisk ~ 6P4_floppy | 第6章 支持块设备 | 🚧 骨架 | 硬盘/分区表、缓冲区、软盘 DMA |
| 7P1_minix_fs ~ 7P6_exec | 第7章 文件系统 | 🚧 骨架 | Minix 超级块、inode、文件/目录/链接、execve |
| 8P1_posix ~ 8P3_clib | 第8章 系统服务接口 | 🚧 骨架 | POSIX 接口、信号/管道、用户态 C 库 |

骨架目录的 README 里写好了：对应书节、要做什么、官方仓源码映射、从哪个实验起手。

# 代码仓结构说明
每一个目录代表一个实验，命名遵循“章节_名称”的命名方式
 ## 1P3_hello_word
这个实验对应1.3章节的hello world实验。

第2章之后的实验共用一套骨架（bootsect.S / setup.S / head.S / kernel.lds / tools/build.c），
每个实验在其上叠加该章节新增的源码文件，全部带逐行中文注释。

## 已完成实验的通用构建链（以 3P3 为例）

```
bootsect.S ──as──> bootsect.o ──ld──> bootsect ┐
setup.S    ──as──> setup.o    ──ld──> setup    ├─ tools/build ──> linux.img（补齐 1.44MB）
head.S     ──as --32──┐                       │
asm.S     ──as --32───┼─ ld -T kernel.lds ──> system
*.c       ──gcc -m32──┘（-ffreestanding 等 flags）
```

关键经验（详见各实验 README 的“常见问题”）：
1. 镜像必须补齐到 1.44MB，否则 qemu 猜错软盘几何参数导致读盘死循环
2. 内核 C 代码必须用链接脚本固定 head.o 在 0x0（gcc -O2 的 .text.startup 会乱序）
3. 链接 C 与汇编符号时，`extern void *idt` 与 `extern unsigned long idt[]` 完全不同

# 学习建议

1， 善于利用大模型，比如豆包，是一个很不错的工具
2， 每个实验先跑起来看现象，再对照书中该节内容读注释
3， 改一改做实验：注释掉 EOI、换掉页表属性、故意触发异常，观察行为差异
