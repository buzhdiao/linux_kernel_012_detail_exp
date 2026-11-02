# 5P1 支持键盘

> 状态：**待整理**（骨架占位，代码尚未拆分）

对应书中 **5.1 支持键盘**（键盘的工作原理 / 解析扫描码 / 构建缓冲区 / 完善屏幕回显）

## 本实验要做什么

在 4P3 基础上：初始化键盘（8259A 放开 IRQ1），键盘中断解析扫描码进环形
缓冲区，回显到屏幕——内核第一次响应外部输入设备。

## 官方仓源码映射（linux_kernel_012）

| 来源文件 | 引入内容 |
| --- | --- |
| kernel/chr_drv/keyboard.S | 键盘中断入口、扫描码解析表 |
| kernel/chr_drv/kboard.c | 扫描码到 ASCII 的转换 |
| kernel/chr_drv/console.c（回显部分） | con_write 回显到屏幕 |
| include/linux/tty.h | tty_struct、键盘缓冲队列 |

## 起手方式

cp -r ../4P3_fork_syscall/* . 开始扩展

整理完成后本 README 按既有实验格式补全：运行效果（result.png）、运行步骤、
与官方仓的对应关系、常见问题。
