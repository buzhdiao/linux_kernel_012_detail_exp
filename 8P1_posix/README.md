# 8P1 POSIX接口

> 状态：**待整理**（骨架占位，代码尚未拆分）

对应书中 **8.1 POSIX接口**（时间管理 / 挂载文件系统）

## 本实验要做什么

在 7P6 基础上：补齐常用 POSIX 系统调用，
date 命令能显示真实时间，mount 挂载额外文件系统。

## 官方仓源码映射（linux_kernel_012）

| 来源文件 | 引入内容 |
| --- | --- |
| kernel/sys.c | sys_time/settimeofday/mount 等 |
| kernel/mktime.c | 开机时间换算 |

## 起手方式

cp -r ../7P6_exec/* . 开始扩展

整理完成后本 README 按既有实验格式补全：运行效果（result.png）、运行步骤、
与官方仓的对应关系、常见问题。
