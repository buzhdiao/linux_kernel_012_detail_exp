# 4P2 创建第二个进程

> 状态：**待整理**（骨架占位，代码尚未拆分）

对应书中 **4.2 创建第二个进程**（准备进程控制块 / 复制进程控制块 / 实现进程切换）

## 本实验要做什么

在 4P1 基础上：手工构造 task[1]（复制 task[0]），写 schedule() 轮转调度，
用 ljmp TSS 选择子完成第一次真正的进程切换，两个进程轮流打印。

## 官方仓源码映射（linux_kernel_012）

| 来源文件 | 引入内容 |
| --- | --- |
| kernel/fork.c（copy_process 前半） | 复制 task_struct、复制页目录（copy_mem） |
| kernel/sched.c（schedule 等） | 就绪队列、调度器 |
| kernel/asm.S（system_call 后半） | 进程切换点 |
| include/linux/sched.h | switch_to 宏 |

## 起手方式

cp -r ../4P1_create_init_process/* . 开始扩展

整理完成后本 README 按既有实验格式补全：运行效果（result.png）、运行步骤、
与官方仓的对应关系、常见问题。
