# 7P3 管理普通文件

> 状态：**待整理**（骨架占位，代码尚未拆分）

对应书中 **7.3 管理普通文件**（通过路径查询inode / 打开文件 / 读操作 / 读写）

## 本实验要做什么

在 7P2 基础上：完整的 open/read/write/close 链路，
用户程序第一次从文件系统读数据。

## 官方仓源码映射（linux_kernel_012）

| 来源文件 | 引入内容 |
| --- | --- |
| fs/namei.c（部分） | namei 路径解析 |
| fs/open.c | sys_open/close |
| fs/read_write.c | sys_read/write |
| fs/file_dev.c | file_read/file_write |
| fs/file_table.c | 进程文件表 |

## 起手方式

cp -r ../7P2_inode/* . 开始扩展

整理完成后本 README 按既有实验格式补全：运行效果（result.png）、运行步骤、
与官方仓的对应关系、常见问题。
