
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
| 4P2_second_process | 4.2 创建第二个进程 | ✅ | 双进程轮转调度（ljmp 任务切换），qemu 验证交替输出 |
| 4P3_fork_syscall | 4.3 第一个系统调用fork | ✅ | fork + 写时复制 + 缺页中断（父子数据分化验证） |
| 5P1_keyboard | 5.1 支持键盘 | ✅ | IRQ1 + 扫描码 + 环形缓冲 + 睡眠唤醒（sendkey 验证） |
| 5P2_input | 5.2 优化输入功能 | ✅ | tty 层 + 规范模式阻塞 read |
| 5P3_output | 5.3 优化输出功能 | ✅ | write + ANSI 转义序列状态机（彩色验证） |
| 5P4_terminal_state | 5.4 修改终端状态 | ✅ | termios 标志（ICANON/ECHO）+ 方向键 |
| 6P1_harddisk | 6.1 硬盘的基本结构 | ✅ | ATA IDENTIFY + MBR 分区表解析 |
| 6P2_buffer | 6.2 管理缓冲区 | ✅ | buffer_head 池 + 哈希链 + LRU（MISS/HIT 验证） |
| 6P3_hd_driver | 6.3 硬盘驱动 | ✅ | 请求队列 + IRQ14 中断驱动读写（写读回校验） |
| 6P4_floppy | 6.4 软盘驱动 | ✅ | FDC 编程 + 8237 DMA（内容级校验） |
| 7P1_minix_fs | 7.1 Minix文件系统 | ✅ | 超级块 + 挂载 + 根目录（mkfs.py 配套工具） |
| 7P2_inode | 7.2 管理inode | ✅ | inode 缓存 + namei 路径查找 |
| 7P3_file | 7.3 管理普通文件 | ✅ | open/read/close，用户程序首次读文件成功 |
| 7P4_dir | 7.4 管理目录 | ✅ | mkdir/creat/rmdir 写路径（已知 i_size 写回问题见 README） |
| 7P5_link | 7.5 文件链接 | ✅ | 硬链接 + nlinks 引用计数 |
| 7P6_exec | 7.6 执行程序 | 🟡 WIP | execve 代码完成；首次任务切换 #GP 留作练习（README 有调试线索） |
| 8P1_posix | 8.1 POSIX接口 | ✅ | getpid/uptime/sysinfo |
| 8P2_process | 8.2 管理进程 | 🟡 WIP | exit/wait/kill/管道代码完成；演示段唤醒条件待修 |
| 8P3_clib | 8.3 构建C语言库 | ✅ | syscall 封装 + 用户态 printf，编译链验证（4556B 可执行） |

全部 28 个实验通过全量构建验证。骨架 README 含对应书节、源码映射、踩坑记录。

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
1. 镜像必须补齐到 1.44MB（qemu 软盘几何参数）；head.S 的 std 必须配对 cld（DF=1 时 C 结构体拷贝反向执行）
2. 内核 C 代码必须用链接脚本固定 head.o 在 0x0；GDT 下标 8 字节/项的指针运算陷阱
3. 系统调用里绝不能忙等/睡眠忘开中断（int 0x80 门 IF=0 饿死中断和调度器）
4. EOI 时序（先 EOI 再可能切任务）；级联中断两个 IMR 位都要开
5. 按 1KB 块读写缓冲必须给足 1KB（栈炸教训）；内核栈 4KB 内不放大数据缓冲
6. IDE 数据口必须 inw；`extern void *idt` 与 `extern unsigned long idt[]` 完全不同

# 学习建议

1， 善于利用大模型，比如豆包，是一个很不错的工具
2， 每个实验先跑起来看现象，再对照书中该节内容读注释
3， 改一改做实验：注释掉 EOI、换掉页表属性、故意触发异常，观察行为差异
