#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

int main() {
    char buf[16];
    int fds[2];
    int pid;
    int n = pipe(fds);

    if (n < 0) {
        printf("unable to create pipe: %d\n\r", n);
        return 0;
    }

    if ((pid = fork()) == 0) {
        n = write(fds[1], "hello pipe!", 11);
        if (n < 0) {
            printf("unable to write:%d\n\r", n);
        }
        else {
            printf("write done\n\r");
        }

        wait(pid);
    }
    else {
        n = read(fds[0], buf, 16);
        if (n < 0) {
            printf("unable to read:%d\n\r", n);
            return 0;
        }
        else {
            printf("receive: %s\n\r", buf);
            return 0;
        }
    }

    return 0;
}

