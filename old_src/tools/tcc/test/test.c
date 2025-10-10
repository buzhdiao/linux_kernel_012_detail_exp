#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define MAX_LEN 128

char buf[MAX_LEN];
char* argv[] = {"/bin", NULL};
char* envp[] = {"HOME=/usr/root", NULL};

int main(int argc, char** argv, char** envp) {
    int i;
    pid_t pid;

    while (1) {
        printf(">>>");
        i = read(0, buf, MAX_LEN);
        buf[i-1] = '\0';
        if (strcmp(buf, "exit") == 0) {
            break;
        }
        else if (strcmp(buf, "sh") == 0) {
            printf("got sh\n");
            if (!(pid = fork())) {
                execve("/bin/sh", argv, envp);
            }
            wait(pid);
        }
        else if (strcmp(buf, "ls") == 0) {
            printf("got ls\n");
            if (!(pid = fork())) {
                execve("/bin/ls", argv, envp);
            }
            wait(pid);
        }
        else if (strcmp(buf, "cat") == 0) {
            printf("got cat\n");
            buf[0] = '/';
            buf[1] = 'r';
            buf[2] = 'o';
            buf[3] = 'o';
            buf[4] = 't';
            buf[5] = '/';
            i = read(0, buf + 6, MAX_LEN);
            buf[i + 6] = '\0';
            argv[0] = buf;

            if (!(pid = fork())) {
                printf("%s\n\r", buf);
                execve("/bin/cat", argv, envp);
            }
            wait(pid);
        }
    }

    return 0;
}
