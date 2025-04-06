# -boot是选择启动的设备，a表示软区，通常情况下，boot会跟参数，比如order等
# 这里没有写，默认就是order，含义是启动顺序
# -fda参数的含义是指定软驱A的映像文件
qemu-system-i386 -boot a -fda linux.img