# 8P2 管理进程：exit/wait/kill/管道（✅ 已修复，qemu 运行验证通过）

对应书中 **8.2 管理进程**（信号 / 管道 / 同步 / 退出）。
来源：官方仓 `kernel/exit.c` + `fs/pipe.c` 的教学精简版。

## 已实现的部分（代码完成，构建通过）

| 系统调用 | nr | 语义 |
| -------- | -- | ---- |
| exit(code) | 16 | 置 ZOMBIE、存退出码、唤醒父、调度走（不返回） |
| wait(&status) | 17 | 找 ZOMBIE 子进程收尸：取退出码、回收任务槽 |
| kill(pid, sig) | 18 | 信号位图置位（任务被调度时检查处理） |
| pipe_write/read | 19/20 | 单槽同步管道：写者灌入唤醒读者，读者睡眠等数据 |

## 当前状态（✅ 已修复，qemu 运行验证通过）

- exit/wait/kill/pipe 的内核代码与系统调用接线完成（构建通过）
- 演示段已跑通：`process_demo`（fork → 子 exit(42) → 父 wait 收尸）
  输出 `[exit] task 1 exited with code 42` → `[wait] reaped pid 1` →
  `[parent] child pid=1 exit_code=42` → `[8P2] done`，随后 uptime 循环
  正常存活（qemu 截屏验证）
- **修复的三个根因**（原 WIP 的"唤醒条件"推测只是表象之一）：
  1. **内核态 fork 的栈现场（真正元凶）**：task0 在内核态发起 int 0x80
     fork 时，CPU 不压 SS/ESP（特权级不变只压 eip/cs/eflags），
     `regs->ss` 读到的是栈上残留垃圾——子进程 TSS.ss 被填成垃圾
     选择子，首次切换进子任务即 #TS，子进程永远跑不起来 → 父进程
     wait 空转挂起（`-d int` 里"系统还在跑"的假象来自时钟中断照常）。
     修复：内核态 fork 时把父栈"当前栈顶以上"的存活调用链整段拷到
     子进程自己的栈页顶部（帧内返回地址指向内核代码，两份都有效），
     见 fork.c 的 cs==0x08 分支
  2. **wait 的 status 落在 pt_regs 槽位上**：内核态 int 0x80 的现场
     帧就压在调用者 SP 下方的保留区里，栈上局部变量 `&status` 实测
     正好落在 eax 槽，wait 写入的退出码随即被返回值覆盖（恒打印 0）。
     修复：`process_demo` 的 status 改为 static 存储
  3. **do_exit 唤醒条件只覆盖 UNINTERRUPTIBLE**：对照官方
     do_exit/notify_parent，"通知 + 唤醒"不应挑剔父进程的睡眠类型，
     现已覆盖 UNINTERRUPTIBLE 与 INTERRUPTIBLE 两种（见 exit_wait.c）
- 已验证部分：挂载/目录/链接/POSIX 段照常工作（前序实验全部复现）

## 与官方仓代码的对应关系

| 本实验 | 官方仓 linux_kernel_012 |
| ------ | ----------------------- |
| do_exit | `kernel/exit.c` 的 do_exit（官方含页释放、会话/进程组通知） |
| sys_waitpid | `kernel/exit.c` 的 sys_waitpid（官方支持 WNOHANG 等 flag） |
| kill/信号 | `kernel/signal.c`（本版只有位图置位，投递在调度点检查） |
| 管道 | `fs/pipe.c`（官方用 inode 的管道读写队列 + 双向睡眠） |
