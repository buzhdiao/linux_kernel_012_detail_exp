#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/fs.h>
#include <unistd.h>
#include <fcntl.h>

#define MINIX_HEADER 32
#define GCC_HEADER 1024

#define SYS_SIZE 0x2000

#define DEFAULT_MAJOR_ROOT 2
#define DEFAULT_MINOR_ROOT 29

#define SETUP_SECTS 4

#define STRINGIFY(x) #x

#define ARG_LEN 3

void die(char * str)
{
	fprintf(stderr, "%s\n", str);
	exit(1);
}

void usage(void)
{
	die("Usage: build bootsect setup system [rootdev] [> image]");
}

int main(int argc, char **argv)
{
	int i, c, id;
	char buf[1024];
	char major_root, minor_root;
	struct stat sb;

	if ((argc != ARG_LEN) && (argc != ARG_LEN + 1))
		usage();

	if (argc == ARG_LEN + 1)
	{
		if (strcmp(argv[ARG_LEN], "FLOPPY"))
		{
			if (stat(argv[ARG_LEN], &sb))
			{
				perror(argv[ARG_LEN]);
				die("Couldn't stat root device");
			}
			major_root = sb.st_rdev & 0xff00;
			minor_root = sb.st_rdev & 0x00ff;
		}
		else
		{
			major_root = 0;
			minor_root = 0;
		}
	}
	else
	{
		major_root = DEFAULT_MAJOR_ROOT;
		minor_root = DEFAULT_MINOR_ROOT;
	}
	fprintf(stderr, "Root device is (%d, %d)\n", major_root, minor_root);
	if ((major_root != 2) && (major_root != 3) && (major_root != 0))
	{
		fprintf(stderr, "Illegal root device (major = %d)\n", major_root);
		die("Bad root device --- major #");
	}

	for (i = 0; i < sizeof(buf); i++)
		buf[i] = 0;

	if ((id = open(argv[1], O_RDONLY, 0)) < 0)
		die("Unable to open 'boot'");

	i = read(id, buf, sizeof(buf));
	fprintf(stderr, "Boot sector %d bytes.\n", i);
	if (i != 512)
		die("Boot block must be exactly 512 bytes");

	buf[508] = (char) minor_root;
	buf[509] = (char) major_root;
	i = write(1, buf, 512);
	if (i != 512)
		die("Write call failed");
	close(id);	

	if ((id = open(argv[2], O_RDONLY, 0)) < 0)
		die("Unable to open 'setup'");

	for (i = 0; (c = read(id, buf, sizeof(buf))) > 0; i += c)
		if (write(1, buf, c) != c)
			die("Write call failed");

	close(id);

	if (i > SETUP_SECTS * 512)
		die("Setup exceeds" STRINGIFY(SETUP_SECTS) " sectors - rewrite build/boot/setup");
	fprintf(stderr, "Setup is %d bytes.\n", i);
	for (c = 0; c < sizeof(buf); c++)
		buf[c] ='\0';
	while (i < SETUP_SECTS * 512)
	{
		c = SETUP_SECTS * 512 - i;
		if (c > sizeof(buf))
			c = sizeof(buf);

		if (write(1, buf, c) != c)
			die("Write call failed");
		i+=c;
	}

	/* 把整个镜像补齐到标准 1.44MB 软盘大小。
	 * 注意：此时 i 只包含 setup 部分的字节数（引导扇区的 512 字节不在内），
	 * 所以补齐目标是 1474560 - 512。
	 * qemu 会按镜像文件大小猜测软盘几何参数，小镜像可能被猜成单磁头格式，
	 * 导致 bootsect 读完一个磁道后翻磁头失败而死循环；补齐到 1.44MB 后
	 * qemu 按 2 磁头、18 扇区/磁道识别，与 bochs 的默认行为一致 */
	while (i < 1474560 - 512)
	{
		c = 1474560 - 512 - i;
		if (c > sizeof(buf))
			c = sizeof(buf);

		if (write(1, buf, c) != c)
			die("Write call failed");
		i += c;
	}

	return 0;
}