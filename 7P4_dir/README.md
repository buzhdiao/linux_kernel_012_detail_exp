# 7P4 管理目录：mkdir/creat/rmdir（写路径）

对应书中 **7.4 管理目录**（创建目录 / 创建文件 / rmdir删除目录 / 新建文件）。
来源：官方仓 `fs/bitmap.c` + `fs/namei.c` 的 mkdir/creat/rmdir 精简版。

## 实验目的

1. **位图分配**：inode 位图/zone 位图的"找 0 置 1"（跳过保留号：
   0=失败码、1=根 inode、<firstdatazone=元数据区）
2. **写路径全家桶**：new_inode（inode 表写回）、new_zone（清零分配）、
   add_entry（目录项追加 + 父 inode 的 size 写回）、blk_write
3. **mkdir/creat/rmdir 语义**：新目录带 . ..；rmdir 拒绝非空目录

## 运行效果

![运行效果](./result.png)

```
mkdir /mydir OK (inode 5)
creat /mydir/inner.txt OK (inode 6)
mkdir /mydir2 OK (inode 7)
root now has: . .. hello readme etc mydir mydir2
/mydir/inner.txt: FOUND (inode 6)          <- 两级路径查找新建文件
rmdir /mydir2: entry not found in parent   <- 已知问题（见下）
rmdir /mydir: not empty (size=48)          <- 拒删非空目录 ✓
/mydir: still there (OK)
```

## 已知问题（诚实记录，留作练习）

`rmdir /mydir2` 报"entry not found"：目录项写入与查找全部正常
（hexdump 镜像实证 mydir/mydir2/inner.txt 的目录项与 inode 都落盘），
但目录 inode 的 `i_size` 写回偶发丢失（mydir 落盘 size=0，应 48），
导致后续按 size 扫描的行为异常。排查方向：`blkbuf` 共享缓冲在
add_entry 的"数据块写回 → inode 表读改写"序列中的复用时序。
这正是 0.11 用"buffer cache + 引用计数"解决的那类问题。

## 运行步骤

```bash
python3 ../../tools/mkfs.py /tmp/minix.img
make
sh run.sh
```

## 踩过的坑（两个大坑）

1. **zone 位图的保留位**：mkfs 没标记 0~firstdatazone-1 为已用，
   内核 new_bitmap 又从 bit0 起找——分配到 zone 0（引导块），写入被
   qemu 的 raw 镜像写限制丢弃。修法：mkfs 标记保留位 + 内核
   new_bitmap 加 skip 参数跳过保留号
2. **栈溢出（最隐蔽）**：blk_read/new_zone/add_entry 各在栈上放 1KB
   缓冲，调用链叠加压穿 4KB 内核栈——栈顶侵入 IDT 区域把异常门写坏，
   三重故障循环。修法：块 IO 缓冲改为共享静态 blkbuf（单内核线程
   复用安全）。**内核开发铁律：栈上不放大数据缓冲**

## 与官方仓代码的对应关系

| 本实验 | 官方仓 linux_kernel_012 |
| ------ | ----------------------- |
| new_bitmap | `fs/bitmap.c` 的 new_bitmap（官方用 8 字节批量扫描更快） |
| new_inode/new_zone | 官方 free_inode/new_block + clear_block |
| add_entry | `fs/namei.c` 的 add_entry（官方还处理目录满跨块） |
| mkdir/creat/rmdir | `fs/namei.c` 同名函数（官方经 iput 落盘含 i_nlinks 维护） |
