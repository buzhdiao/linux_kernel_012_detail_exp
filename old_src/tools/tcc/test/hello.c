#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

int main() {
    char buf[128];

    int fid = open("/root/txt", O_RDONLY);
    if (fid < 0) {
        printf("unable to open file\n\r");
        return 0;
    }

    int n = read(fid, buf, 128);
    if (n > 0) {
        write(1, buf, n);
        printf("\n\r");
    }
    else
        printf("unable to read file\n\r");

    return 0;
}

