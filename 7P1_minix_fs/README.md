# 7P1 Minix文件系统

> 状态：**待整理**（骨架占位，代码尚未拆分）

对应书中 **7.1 Minix文件系统**（文件系统的基本结构 / 定义超级块 / 初始化超级块）

## 本实验要做什么

在 6P4 基础上：挂载根文件系统——读取超级块、位图、inode 表，
打印文件系统布局信息。

## 官方仓源码映射（linux_kernel_012）

| 来源文件 | 引入内容 |
| --- | --- |
| include/linux/fs.h | super_block、inode、dir_entry 结构 |
| fs/super.c | read_super、挂载逻辑 |
| tools/mkfs/ | 制作 Minix 根文件系统镜像的工具 |

## 起手方式

cp -r ../6P4_floppy/* . 开始扩展

整理完成后本 README 按既有实验格式补全：运行效果（result.png）、运行步骤、
与官方仓的对应关系、常见问题。
