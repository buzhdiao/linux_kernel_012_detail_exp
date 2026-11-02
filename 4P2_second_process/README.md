# 4P2 创建第二个进程：手工构造 task[1] + 时钟轮转调度

对应书中 **4.2 创建第二个进程**（准备进程控制块 / 复制进程控制块 / 实现进程切换）。

## 实验目的

1. **手工构造第二个进程**：task1_union（task_struct + 独立内核栈共 4KB 页），
   TSS 里预填用户态现场（cs=0x0f、eip=user_task1、用户栈 0x70000）
2. **时钟驱动调度**：8254 通道 0 设为 100Hz → IRQ0 → do_timer 减时间片 →
   时间片耗尽调 schedule() → 轮转选下一个就绪进程
3. **硬件任务切换**：ljmp 到目标 TSS 选择子，CPU 一条指令保存/恢复全部现场
   （含 CR3、LDT、段寄存器、EIP）

## 新增内容（相对 4P1）

| 文件 | 变化 |
| ---- | ---- |
| `sched.c` | task1_union 静态构造；sched_init 注册 TSS1/LDT1 到 GDT 第 7/8 项；8254 编程 + 解除 IRQ0 屏蔽；do_timer/schedule/switch_to |
| `sched.h` | LATCH、jiffies、switch_to 声明 |
| `asm.S` | timer_interrupt 入口桩（EOI 提前，见下）；system_call 传两参数 |
| `traps.c` | 挂 0x20 时钟中断门 |
| `main.c` | sti 开中断；两个用户态任务各自循环打印 |

## 运行效果

![运行效果](./result.png)

```
task 0: hello from task 0
task 1: hello from task 1
task 0: hello from task 0
task 1: hello from task 1
...（两个进程轮流打印，永续交替）
```

## 运行步骤

```bash
make
sh run.sh
```

## 本实验踩过的三个坑（每个都是经典内核课）

1. **用户态绝不能直接 call 内核函数**：第一版用户任务直接调 `sys_print()`，
   用户代码段确实"够得着"内核代码地址，调用也能进入，但一执行到 printk
   内部的特权指令（out 0x3d4 写光标）就 #GP → 三重故障。正确姿势：
   `int $0x80` 走系统调用门。这个错误的排查线索是 qemu -d int 日志里
   "v=0d e=0000 cpl=3" —— CPL=3 却在执行内核代码
2. **EOI 必须在 do_timer 之前发**：do_timer 的 schedule() 可能 ljmp 到新
   任务且"一去不回"（要等下一次时钟中断才切回来）。若 EOI 放在后面，
   切走时 8259A 的 ISR 位还挂着，后续 IRQ0 全部被阻塞——新任务的时间片
   永远走不动，调度死锁。修复后 IRQ0 才能持续驱动轮转
3. **中断状态的"隐式依赖"**： sched_init 里 8259A 的 IRQ0 屏蔽位必须解除
   （setup.S 曾写 0xFF 全屏蔽）。IDT 门、IF 标志、PIC IMR、8254 编程
   四个环节任何一个断了，症状都是"中断静默消失"，用 qemu -d int 跟踪
   Servicing INT 计数即可定位

## 与官方仓代码的对应关系

| 本实验 | 官方仓 linux_kernel_012 |
| ------ | ----------------------- |
| task1_union 手工构造 | 官方由 fork 动态创建（4P3 引入），本书 4.2 先手工造一个演示 |
| sched_init 的 8254/IRQ0 初始化 | kernel/sched.c sched_init 末尾同款代码 |
| do_timer | kernel/sched.c do_timer（官方还统计进程运行时间） |
| schedule 轮转 | kernel/sched.c schedule（官方按 counter 最大值挑选，本实验先做轮转） |
| switch_to 的 ljmp | include/linux/sched.h switch_to 宏（同一手法） |

## 常见问题

1. **为什么切换要用 ljmp TSS 而不是手动保存寄存器？** TSS 是 x86 硬件
   原生支持的任务切换机制，一条 ljmp 完成"存旧装新"；现代内核改用软件
   切换（更快），但 0.11 时代和本书都用 TSS——学习阶段的最佳选择
2. **两个任务的用户代码在哪里？** 都在内核镜像里（LDT 用户段基址 0，
   恒等映射覆盖内核镜像）。task0 的用户代码是 main 的延续；task1 的
   入口 user_task1 由 sched_init 填进 TSS.eip。真正的"从文件加载程序"
   要等第 7 章 exec
3. **两行一组交替而不是一行一行？** 打印间隔（约 0.65s 延时）大于时间片
   （150ms），一次延时内时间片重新充满后再次切换，所以每个任务连续跑
   两个时间片。把 priority 调小可看到更细的交错
