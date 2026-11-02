# 8P1 POSIX接口：getpid / uptime / sysinfo

对应书中 **8.1 POSIX接口**（时间管理 / 挂载文件系统等）。本实验演示
系统调用框架的批量扩展方式——内核功能按 POSIX 语义逐个挂上。

## 新增系统调用

| nr | 名称 | 语义 |
| -- | ---- | ---- |
| 13 | getpid | 返回 current->pid |
| 14 | uptime | jiffies/HZ 换算的运行秒数 |
| 15 | sysinfo | 用户数组返回 {空闲页数, 任务数, 任务槽总数} |

## 运行效果（qemu 实测文本）

```
[posix] getpid=0
[posix] uptime=0s
[posix] sysinfo: free_pages=3840 tasks=1/64
[8P1] done
task 0: [uptime] alive        <- 主循环每 1s 心跳（经 idle 睡眠）
```

## 运行步骤

```bash
python3 ../../tools/mkfs.py /tmp/minix.img
make
sh run.sh
```

## 与官方仓代码的对应关系

| 本实验 | 官方仓 linux_kernel_012 |
| ------ | ----------------------- |
| getpid | `kernel/sys.c` 的 sys_getpid |
| uptime | jiffies/CURRENT_TIME 机制（官方 mktime 管真实日期） |
| sysinfo | 官方无此调用（free 内存统计见 mem_init） |
