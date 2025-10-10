static char c = 'A';
static int bss = 0;
int compareStr(char *str, int len) {
    for (int i = 0; i < len; ++i) {
        if (str[i] == c) {
            return 1;
        }
    }
    return bss;
}
int _start() {
    if (compareStr("Hello World", 11)) {
        return 1;
    }
    return 0;
}
