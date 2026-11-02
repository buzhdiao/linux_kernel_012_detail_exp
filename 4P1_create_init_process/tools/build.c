/*
 * 2P4 镜像打包工具：把 bootsect、setup、system 拼成可引导的软盘镜像
 *
 * 镜像布局（与 bootsect.S 的读取逻辑严格对应）：
 *   第 0 扇区       bootsect（正好 512 字节，末尾两字节是 0xAA55）
 *   第 1~4 扇区     setup（不足 4 扇区用 0 补齐）
 *   第 5 扇区起     system（不足 SYSSIZE 用 0 补齐，这里补齐到 64KB）
 *
 * 用法：./tools/build bootsect setup system > linux.img
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

/* bootsect.S 中 SETUPLEN = 4：setup 固定占 4 个扇区 */
#define SETUP_SECTS  4

/* 整个镜像补齐到标准 1.44MB 软盘大小。
 * 这一步不可省略：qemu 按镜像文件大小猜测软盘几何参数，小尺寸镜像会被
 * 猜成 160KB 单磁头格式（8 扇区/磁道），read_it 读满一个磁道后翻到
 * head 1 就会读失败，陷入无限重试。补齐到 1.44MB 后 qemu 按
 * 双磁头、每磁道 18 扇区识别，与 bochs 的默认行为一致 */
#define IMG_TOTAL    (1474560)

void die(char *str)
{
	fprintf(stderr, "%s\n", str);
	exit(1);
}

/* 把文件 fd 的内容原样写到标准输出（镜像），返回已写字节数 */
long copy_file(int fd, const char *name)
{
	char buf[512];
	long total = 0;
	int n;

	while ((n = read(fd, buf, sizeof(buf))) > 0) {
		if (write(1, buf, n) != n)
			die("Write call failed");
		total += n;
	}
	if (n < 0) {
		perror(name);
		die("Read failed");
	}
	close(fd);
	return total;
}

/* 向标准输出补 count 个零字节 */
void pad_zero(long count)
{
	char buf[512];

	memset(buf, 0, sizeof(buf));
	while (count > 0) {
		int n = count > (long)sizeof(buf) ? (int)sizeof(buf) : (int)count;
		if (write(1, buf, n) != n)
			die("Write call failed");
		count -= n;
	}
}

/* 返回文件长度 */
long file_size(int fd)
{
	long cur = lseek(fd, 0, SEEK_CUR);
	long end = lseek(fd, 0, SEEK_END);
	lseek(fd, cur, SEEK_SET);
	return end;
}

int main(int argc, char **argv)
{
	int fd;
	long size, written = 0;

	if (argc != 4)
		die("Usage: build bootsect setup system > image");

	/* 1. bootsect：必须是正好 512 字节 */
	if ((fd = open(argv[1], O_RDONLY, 0)) < 0)
		die("Unable to open 'bootsect'");
	if (file_size(fd) != 512)
		die("Boot block must be exactly 512 bytes");
	written += copy_file(fd, argv[1]);
	fprintf(stderr, "Boot sector 512 bytes.\n");

	/* 2. setup：先写内容，再补齐到 4 个扇区 */
	if ((fd = open(argv[2], O_RDONLY, 0)) < 0)
		die("Unable to open 'setup'");
	size = file_size(fd);
	if (size > SETUP_SECTS * 512)
		die("Setup exceeds 4 sectors");
	written += copy_file(fd, argv[2]);
	written += SETUP_SECTS * 512 - size;
	pad_zero(SETUP_SECTS * 512 - size);
	fprintf(stderr, "Setup is %ld bytes.\n", size);

	/* 3. system：写内容后，把整个镜像补齐到 1.44MB 标准大小 */
	if ((fd = open(argv[3], O_RDONLY, 0)) < 0)
		die("Unable to open 'system'");
	size = file_size(fd);
	if (written + size > IMG_TOTAL)
		die("Image too big");
	written += copy_file(fd, argv[3]);
	fprintf(stderr, "System is %ld bytes.\n", size);

	pad_zero(IMG_TOTAL - written);

	return 0;
}
