# 7P6 执行程序：execve 加载用户程序（WIP：调度切换残留问题）

对应书中 **7.6 执行程序**（a.out格式 / 执行execve / 缺页中断 / 实验）。
来源：官方仓 `fs/exec.c` 的教学精简版。

## 已实现的部分

1. **用户程序构建链**：`user_hello.c`（int 0x80 系统调用）→
   `gcc -m32 -ffreestanding -fno-pic` → `ld -Ttext=0 --oformat binary`
   → flat binary（4280 字节）→ mkfs.py 自动放入镜像 `/hello.exe`
2. **do_execve**（新任务版）：
   - namei 找文件、file 层读出镜像
   - 每 4KB：get_free_page + 读入 + `put_page` 挂到新任务的 64MB 区页表
   - 手工构造 task_struct：LDT 基址=64MB×nr、TSS eip=0（镜像入口）、
     用户栈页映射、GDT 登记、state=RUNNING
3. **用户栈页**：exec 时预映射（本版 do_no_page 未实现按需调页）

## 当前状态（WIP）

- 内核侧 execve 全流程代码就绪（exec.c，编译通过）
- **遗留问题**：exec 后第一次时钟中断触发的任务切换引发 #GP 风暴
  （故障点在 sys_idle_ticks 附近，疑似 exec 任务的 TSS/现场保存与
  中断嵌套的交互问题）。**留作练习**：用 `-d int` 跟踪第一次
  switch_to(n) 的 ljmp 前后状态，对照 4P2 的手工 task1 检查 TSS
  各字段（重点：eflags、esp0、ldt 选择子、GDT 描述符类型位）
- 修复后运行方式：`sh run.sh`，期望屏幕出现
  `[user] hello from an exec'd program!`

## 运行步骤

```bash
python3 ../../tools/mkfs.py /tmp/minix.img   # 会自动打包 /tmp/user_hello.bin
make
sh run.sh
```

## 与官方仓代码的对应关系

| 本实验 | 官方仓 linux_kernel_012 |
| ------ | ----------------------- |
| put_page | `mm/memory.c` 的 put_page（官方还含 write_verify） |
| 镜像整块加载 | 官方 do_execve 按需调页（bread_page + do_no_page），本版整块 |
| a.out 头 | 官方解析 ZMAGIC 头（magic/text/data/bss/entry），本版 flat |
| 用户栈布局 | 官方 execve 把 argc/argv/envp 压入用户栈顶，本版从略 |
