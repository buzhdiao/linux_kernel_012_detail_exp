# 8P3 构建C语言库

> 状态：**待整理**（骨架占位，代码尚未拆分）

对应书中 **8.3 构建C语言库**

## 本实验要做什么

在 8P2 基础上：用户态 C 库成形——crt0 启动 + 系统调用封装 + printf，
配合 tcc 在目标机上编译运行 hello world，全系统闭环。

## 官方仓源码映射（linux_kernel_012）

| 来源文件 | 引入内容 |
| --- | --- |
| lib/（open/read/write/close/exit 等） | 用户态系统调用封装 |
| tools/tcc/runtime | crt0、printf/vsprintf、sys_call |
| tools/tcc/test | hello.c / pipe.c 用户程序样例 |

## 起手方式

cp -r ../8P2_process/* . 开始扩展

整理完成后本 README 按既有实验格式补全：运行效果（result.png）、运行步骤、
与官方仓的对应关系、常见问题。
