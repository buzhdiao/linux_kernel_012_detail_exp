# 4P3 第一个系统调用fork

> 状态：**待整理**（骨架占位，代码尚未拆分）

对应书中 **4.3 第一个系统调用fork**（系统调用和中断 / 定义fork / 写时复制 / 页保护中断 / 完善调度器）

## 本实验要做什么

在 4P2 基础上：用户态调用 fork()（int 0x80 + eax=2），父子进程各打印自己的
pid 验证复制成功；验证写时复制——子进程写共享页触发 14 号缺页 +
写保护异常，do_wp_page 复制物理页后继续运行。

## 官方仓源码映射（linux_kernel_012）

| 来源文件 | 引入内容 |
| --- | --- |
| kernel/sys_call.S（完整版） | system_call 完整逻辑、sys_fork 桩 |
| kernel/fork.c（完整版） | sys_fork、find_empty_process、copy_process、verify_area |
| mm/memory.c（un_wp_page / write_verify / do_wp_page） | 写时复制 |
| mm/page.S | 缺页中断入口（14 号异常，有错误码） |
| include/linux/sys.h | sys_call_table 分发表、NR_syscalls |

## 起手方式

cp -r ../4P2_second_process/* . 开始扩展

整理完成后本 README 按既有实验格式补全：运行效果（result.png）、运行步骤、
与官方仓的对应关系、常见问题。
