# 5P2 优化输入功能

> 状态：**待整理**（骨架占位，代码尚未拆分）

对应书中 **5.2 优化输入功能**（控制台和远程终端 / read系统调用）

## 本实验要做什么

在 5P1 基础上：实现 sys_read，用户程序 read() 阻塞等待键盘输入；
区分控制台 tty 和串行终端的数据结构。

## 官方仓源码映射（linux_kernel_012）

| 来源文件 | 引入内容 |
| --- | --- |
| kernel/chr_drv/tty_io.c | tty 读队列、read_wait 等待 |
| kernel/chr_drv/tty_ioctl.c | 终端属性设置 |
| kernel/sys_call.S | sys_read |

## 起手方式

cp -r ../5P1_keyboard/* . 开始扩展

整理完成后本 README 按既有实验格式补全：运行效果（result.png）、运行步骤、
与官方仓的对应关系、常见问题。
