# 8P2 管理进程：exit/wait/kill/管道（WIP：演示段有挂起问题）

对应书中 **8.2 管理进程**（信号 / 管道 / 同步 / 退出）。
来源：官方仓 `kernel/exit.c` + `fs/pipe.c` 的教学精简版。

## 已实现的部分（代码完成，构建通过）

| 系统调用 | nr | 语义 |
| -------- | -- | ---- |
| exit(code) | 16 | 置 ZOMBIE、存退出码、唤醒父、调度走（不返回） |
| wait(&status) | 17 | 找 ZOMBIE 子进程收尸：取退出码、回收任务槽 |
| kill(pid, sig) | 18 | 信号位图置位（任务被调度时检查处理） |
| pipe_write/read | 19/20 | 单槽同步管道：写者灌入唤醒读者，读者睡眠等数据 |

## 当前状态（WIP）

- exit/wait/kill/pipe 的内核代码与系统调用接线完成（构建通过）
- **遗留问题**：process_demo（fork → 子 exit(42) → 父 wait 收尸）运行
  时挂起。排查线索：`-d int` 显示系统仍在跑（时钟/缺页/盘中断正常），
  卡点在 do_exit 的唤醒条件与父进程 idle 睡眠状态的交互——
  **留作练习**：对照官方 do_exit/notify_parent 检查"父进程睡眠状态
  的准确唤醒"（我们的唤醒条件只覆盖 UNINTERRUPTIBLE 一种）
- 已验证部分：挂载/目录/链接/POSIX 段照常工作（前序实验全部复现）

## 与官方仓代码的对应关系

| 本实验 | 官方仓 linux_kernel_012 |
| ------ | ----------------------- |
| do_exit | `kernel/exit.c` 的 do_exit（官方含页释放、会话/进程组通知） |
| sys_waitpid | `kernel/exit.c` 的 sys_waitpid（官方支持 WNOHANG 等 flag） |
| kill/信号 | `kernel/signal.c`（本版只有位图置位，投递在调度点检查） |
| 管道 | `fs/pipe.c`（官方用 inode 的管道读写队列 + 双向睡眠） |
