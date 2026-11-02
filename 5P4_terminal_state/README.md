# 5P4 修改终端状态

> 状态：**待整理**（骨架占位，代码尚未拆分）

对应书中 **5.4 修改终端状态**（方向键 / 控制台标志位）

## 本实验要做什么

在 5P3 基础上：termios 控制——规范模式/原始模式切换、
方向键在命令行里移动光标。

## 官方仓源码映射（linux_kernel_012）

| 来源文件 | 引入内容 |
| --- | --- |
| kernel/chr_drv/tty_ioctl.c | termios 标志位（ICANON/ECHO 等） |
| kernel/chr_drv/keyboard.S | 方向键等扩展扫描码 |

## 起手方式

cp -r ../5P3_output/* . 开始扩展

整理完成后本 README 按既有实验格式补全：运行效果（result.png）、运行步骤、
与官方仓的对应关系、常见问题。
