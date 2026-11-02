# -fda 内核启动盘；-hda 根文件系统（Minix V1 镜像，整盘无分区表）
# 镜像先用本仓库顶层 tools/mkfs.py 生成：
#   python3 ../../tools/mkfs.py /tmp/minix.img
qemu-system-i386 -boot a -fda linux.img -hda /tmp/minix.img
