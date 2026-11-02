#!/usr/bin/env python3
"""最小 Minix V1 文件系统镜像生成器（1.44MB 软盘，512B/1KB 混合块）

布局（1KB 块，Minix V1 magic=0x137F）：
  block 0     引导块（空）
  block 1     超级块
  block 2..   inode 位图（1 块）
  ..          zone 位图（1 块）
  ..          inode 表（每个 32 字节，32 个/块）
  ..          数据区（zones，1KB 每个）

inode7 种模式位 + 直接块 7 个 + 一级间接 1 个。
支持：建目录（. .. 项）、写普通文件。足够内核 7~8 章实验使用。
"""
import struct, sys

BLOCK = 1024
ZONES_PER_BLOCK = 1          # 1KB zone 与块同大小
TOTAL_BLOCKS = 1440          # 1.44MB / 1KB
NR_INODES = 64
IMAP_BLOCKS = 1
ZMAP_BLOCKS = 1
INODE_TABLE_BLOCKS = 2       # 64 inode × 32B = 2KB
FIRST_DATA_ZONE = 2 + IMAP_BLOCKS + ZMAP_BLOCKS + INODE_TABLE_BLOCKS  # = 6

S_IFDIR, S_IFREG = 0x4000, 0x8000

class MinixFS:
    def __init__(self):
        self.img = bytearray(TOTAL_BLOCKS * BLOCK)
        self.inodes = {}      # nr -> (mode, size, zones[9])
        self.next_inode = 2   # 1 号留给根目录
        self.next_zone = FIRST_DATA_ZONE

    def alloc_inode(self, mode, size, zones):
        nr = self.next_inode; self.next_inode += 1
        self.inodes[nr] = (mode, size, zones)
        return nr

    def alloc_zone(self, data=None):
        z = self.next_zone; self.next_zone += 1
        if data:
            d = data if isinstance(data, bytes) else data.encode('latin1')
            self.img[z*BLOCK:(z+1)*BLOCK] = d.ljust(BLOCK, b'\0')[:BLOCK]
        return z

    def write_inode(self, nr, mode, size, zones):
        """Minix V1 inode（32B）：mode(H) uid(H) size(I) mtime(I)
        gid(B) nlinks(B) zone[9](H×9)——注意 V1 的 zone 号是 16 位"""
        off = (2 + IMAP_BLOCKS + ZMAP_BLOCKS) * BLOCK + (nr - 1) * 32
        z = list(zones) + [0] * (9 - len(zones))
        struct.pack_into('<HHIIBB9H', self.img, off,
                         mode, 0, size, 0, 0, 1, *z)

    def add_dir_entry(self, dir_inode_nr, name, inode_nr):
        mode, size, zones = self.inodes[dir_inode_nr]
        data = bytearray(self.img[zones[0]*BLOCK:(zones[0]+1)*BLOCK])
        for off in range(0, BLOCK, 16):
            if struct.unpack_from('<H', data, off)[0] == 0:
                entry = struct.pack('<H', inode_nr) + name.encode().ljust(14, b'\0')
                data[off:off+16] = entry
                self.img[zones[0]*BLOCK:(zones[0]+1)*BLOCK] = data
                self.inodes[dir_inode_nr] = (mode, size + 16, zones)
                return

    def mkdir(self, parent_nr, name):
        """建目录：申请一个 zone 放 . 和 .. 两个目录项"""
        data = struct.pack('<H', 0) + b'.'.ljust(14, b'\0') \
             + struct.pack('<H', 0) + b'..'.ljust(14, b'\0')
        z = self.alloc_zone(data)
        nr = self.alloc_inode(S_IFDIR | 0o755, 32, [z])
        # 回填 . 和 .. 指向自身/父
        blk = bytearray(self.img[z*BLOCK:(z+1)*BLOCK])
        struct.pack_into('<H', blk, 0, nr)
        struct.pack_into('<H', blk, 16, parent_nr)
        self.img[z*BLOCK:(z+1)*BLOCK] = blk
        self.inodes[nr] = (S_IFDIR | 0o755, 32, [z])
        self.add_dir_entry(parent_nr, name, nr)
        return nr

    def create_file(self, parent_nr, name, content):
        """普通文件：内容按 1KB 分块，直接块最多 7 个（7KB 上限）"""
        zones = []
        for i in range(0, len(content), BLOCK):
            zones.append(self.alloc_zone(content[i:i+BLOCK]))
        if not zones:
            zones.append(self.alloc_zone())
        nr = self.alloc_inode(S_IFREG | 0o644, len(content), zones)
        self.add_dir_entry(parent_nr, name, nr)
        return nr

    def finish(self):
        # ---- 超级块（block 1，偏移 0x400）----
        nzones = self.next_zone - FIRST_DATA_ZONE
        sb = struct.pack('<HHHHHHHIIIHH',
                         NR_INODES, TOTAL_BLOCKS, 0, 0, FIRST_DATA_ZONE,
                         0, 0, 0x137F, 0, 0, 0, 0)
        # 字段序（Minix V1 super_block）：s_ninodes s_nzones s_imap_blocks
        #  s_zmap_blocks s_firstdatazone s_log_zone_size s_max_size s_magic
        #  s_state s_time
        sb = struct.pack('<HHHHHHIIIHH',
                         NR_INODES, TOTAL_BLOCKS, IMAP_BLOCKS, ZMAP_BLOCKS,
                         FIRST_DATA_ZONE, 0, 0x7fffffff, 0x137F, 1, 0, 0)
        self.img[BLOCK:BLOCK+len(sb)] = sb
        # ---- inode 位图：1..next_inode-1 置 1 ----
        imap = bytearray(BLOCK)
        for i in range(1, self.next_inode):
            imap[i >> 3] |= 1 << (i & 7)
        self.img[2*BLOCK:3*BLOCK] = imap
        # ---- zone 位图：数据区已用 zone 置 1 ----
        zmap = bytearray(BLOCK)
        for z in range(FIRST_DATA_ZONE, self.next_zone):
            zmap[z >> 3] |= 1 << (z & 7)
        self.img[(2+IMAP_BLOCKS)*BLOCK:(3+IMAP_BLOCKS)*BLOCK] = zmap
        # ---- 写各 inode ----
        for nr, (mode, size, zones) in self.inodes.items():
            self.write_inode(nr, mode, size, zones)
        return bytes(self.img)

def build(out):
    fs = MinixFS()
    root = 1
    fs.inodes[root] = (S_IFDIR | 0o755, 32, [])
    z = fs.alloc_zone(struct.pack('<H', 1) + b'.'.ljust(14, b'\0')
                      + struct.pack('<H', 1) + b'..'.ljust(14, b'\0'))
    fs.inodes[root] = (S_IFDIR | 0o755, 32, [z])

    hello = b"Hello from Minix filesystem on floppy!\n\r"
    fs.create_file(root, 'hello', hello)
    readme = ("This is the root filesystem of linux_kernel_012_detail_exp\n\r"
              "chapter 7 experiments (Minix V1, 1KB blocks).\n\r")
    fs.create_file(root, 'readme', readme)
    fs.mkdir(root, 'etc')

    open(out, 'wb').write(fs.finish())
    print(f"minix fs: {TOTAL_BLOCKS} blocks, inodes={fs.next_inode-1}, "
          f"zones used={fs.next_zone-FIRST_DATA_ZONE}, out={out}")

if __name__ == '__main__':
    build(sys.argv[1] if len(sys.argv) > 1 else '/tmp/minix.img')
