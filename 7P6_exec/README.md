# 7P6 执行程序：execve 加载用户程序（✅ 已完成：#GP 风暴已修复）

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

## 当前状态（✅ 已修复，qemu 运行验证通过）

- execve 全流程工作：装载 → 建 task → GDT 登记 → 时钟中断驱动的
  任务切换 → 用户程序在 CPL=3 跑系统调用，持续输出
  `[user] hello from an exec'd program!` + 每秒 `[user] still alive`
- **修复的三个根因**（原 WIP 的"#GP 风暴"实为连锁反应）：
  1. **调度器可见性窗口（真正的 #GP 元凶）**：原代码在装载循环之前
     就 `task[nr] = p`，而 p 是未清零页、state 恰好为 0（=RUNNING）。
     装载要走 hd I/O（毫秒级），期间时钟中断照常触发，schedule()
     扫到这个"看似就绪"的半成品 → switch_to(1) ljmp 一个还没写进
     GDT 的 TSS → #GP e=0038；#GP 处理返回后 current 已被 xchgl
     换成 task1 而 TR 仍是 task0，状态永久错乱 → 每 tick 风暴 →
     三重故障。修复：分配后立即 `p->state = TASK_UNINTERRUPTIBLE`
     再挂 task[]（fork.c 同款纪律），收尾才 state=RUNNING
  2. **跨页拷贝越界 + 内核栈放大**：原逐 1KB 中转的 `phy[loaded+got+j]`
     把全局累计 loaded 又加在本页偏移上（第 2 页起写越界），且
     do_execve 的 tmp[1024] 与 file_read 的 blk[1024] 叠加压栈。
     修复：去掉 tmp 中转，file_read 直接读进目标物理页
     （file_read 内部自带按 1KB 块的循环，支持任意长度）
  3. **用户栈差一页**：压栈写的是 esp-4，第一条 push 落在
     0x5fffc，栈页须映射 [0x5f000,0x60000) 而非从 0x60000 起
- **配套修复**：
  - user_hello.c：_start 必须是 .text 第一个函数（flat binary 无头
    可解析，入口 = 偏移 0；原先偏移 0 是 sys_print，exec 一进来就
    按调用惯例读栈上参数 → 未映射页缺页）
  - syscall.c default 分支：EBX 是调用者段内偏移，内核侧要加
    current->start_code 翻译（task0 start_code=0，旧实验兼容）
- 运行方式：`sh run.sh`（先 `python3 ../tools/mkfs.py /tmp/minix.img`
  生成根文件系统，会自动打包 /tmp/user_hello.bin 为 /hello.exe；
  user_hello.bin 编译命令见 user_hello.c 头注释）

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
