# -boot a 从软盘 A 启动内核；-hda 附加测试硬盘（含 MBR 分区表）
# 测试盘用 doc 里的说明或下列 python 构造（见 README）
qemu-system-i386 -boot a -fda linux.img -hda /tmp/testhd.img
