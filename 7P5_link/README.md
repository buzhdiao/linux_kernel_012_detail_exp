# 7P5 文件链接：link/unlink（硬链接 + nlinks 引用计数）

对应书中 **7.5 文件链接**（硬链接 / 通过链接访问文件 / 删除链接）。
来源：官方仓 `fs/namei.c` 的 link/unlink 精简版（软链接 0.12 未实现）。

## 实验目的

1. **硬链接的本质**：目录项只是"名字→inode 号"映射——多一个名字就是
   多一条映射，数据零拷贝；inode 的 i_nlinks 记录名字数
2. **link**：add_entry 加新名字 + i_nlinks++ 写回
3. **unlink**：目录项 inode 清 0 + i_nlinks--；减到 0 释放 inode 位图号
   （数据 zone 回收需要遍历块表，本版从略并注明）

## 运行效果

![运行效果](./result.png)

```
link /hello2 -> /hello OK (inode 2 nlinks=2)
[link] /hello2: inode 2 nlinks=2 (same inode as /hello)
unlink /hello OK (nlinks=1)
[link] /hello after unlink: gone
[link] /hello2 still readable: inode 2 nlinks=1
```

删掉原名 /hello 后，**/hello2 依然能访问同一份数据**——这正是引用
计数的意义：数据属于 inode，名字只是引用。

## 运行步骤

```bash
python3 ../../tools/mkfs.py /tmp/minix.img
make
sh run.sh
```

## 与官方仓代码的对应关系

| 本实验 | 官方仓 linux_kernel_012 |
| ------ | ----------------------- |
| fs_link | `fs/namei.c` 的 sys_link（官方经 do_link 校验目录权限） |
| fs_unlink | `fs/namei.c` 的 sys_unlink（官方 free_block 递归释放全部数据块） |
| nlinks=0 释放 | 官方 free_inode + free_zone 链式回收 |
