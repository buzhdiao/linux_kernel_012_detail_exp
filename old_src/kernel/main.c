#define __LIBRARY__

#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <time.h>

inline _syscall0(int, fork);
inline _syscall1(int,setup,void *,BIOS)
inline _syscall2(int, kill, int, pid, int, sig)

int errno;

#include <asm/system.h>
#include <asm/io.h>

#include <linux/tty.h>
#include <linux/kernel.h>
#include <linux/sched.h>

extern void mem_init(long start, long end);
extern void hd_init(void);
extern void blk_dev_init(void);
extern void floppy_init();
extern long kernel_mktime(struct tm * tm);

void init();

#define EXT_MEM_K (*(unsigned short *)0x90002)
#define DRIVE_INFO (*(struct drive_info *)0x90080)
#define ORIG_ROOT_DEV (*(unsigned short *)0x901FC)
#define ORIG_SWAP_DEV (*(unsigned short *)0x901FA)

static long memory_end = 0;
static long buffer_memory_end = 0;
static long main_memory_start = 0;

struct drive_info { char dummy[32]; } drive_info;

#define CMOS_READ(addr) ({ \
    outb_p(0x80|addr,0x70); \
    inb_p(0x71); \
})

#define BCD_TO_BIN(val) ((val)=((val)&15) + ((val)>>4)*10)

static void time_init() {
    struct tm time;
    do {
        time.tm_sec = CMOS_READ(0);
        time.tm_min = CMOS_READ(2);
        time.tm_hour = CMOS_READ(4);
        time.tm_mday = CMOS_READ(7);
        time.tm_mon = CMOS_READ(8);
        time.tm_year = CMOS_READ(9);
    } while (time.tm_sec != CMOS_READ(0));

    BCD_TO_BIN(time.tm_sec);
    BCD_TO_BIN(time.tm_min);
    BCD_TO_BIN(time.tm_hour);
    BCD_TO_BIN(time.tm_mday);
    BCD_TO_BIN(time.tm_mon);
    BCD_TO_BIN(time.tm_year);
    time.tm_mon--;
    startup_time = kernel_mktime(&time);
}

static char * argv_rc[] = { "/bin/sh", NULL };
static char * envp_rc[] = { "HOME=/", NULL ,NULL };

static char * argv[] = { "-/bin/sh",NULL };
static char * envp[] = { "HOME=/usr/root", NULL, NULL };

void main(void) {
    ROOT_DEV = ORIG_ROOT_DEV;

    drive_info = DRIVE_INFO;

    memory_end = (1<<20) + (EXT_MEM_K<<10);
    memory_end &= 0xfffff000;
    if (memory_end > 16*1024*1024)
        memory_end = 16*1024*1024;
    if (memory_end > 12*1024*1024)
        buffer_memory_end = 4*1024*1024;
    else if (memory_end > 6*1024*1024)
        buffer_memory_end = 2*1024*1024;
    else
        buffer_memory_end = 1*1024*1024;

    main_memory_start = buffer_memory_end;
    mem_init(main_memory_start, memory_end);

    trap_init();

    tty_init();

    time_init();
    sched_init();

    printk("\n\rmemory start: %d, end: %d\n\r", main_memory_start, memory_end);

    buffer_init(buffer_memory_end);
    blk_dev_init();
    hd_init();
    floppy_init();
    move_to_user_mode();
    printf("\x1b[31m In user mode!\n\r\x1b[0m");

    struct termios tms;

     if (fork() == 0) {
        init();
    }

    while (1) {}
}

void init() {
    int pid = 0;
    setup((void *) &drive_info);
    (void)open("/dev/tty0", O_RDWR, 0);
    dup(0);
    dup(0);

/*
    if ((pid = fork()) == 0) {
        test_b();
    }
    else {
        test_c(pid);
    }
*/
    if (!fork()) {
        printf("read to start shell\n");
        execve("/hello", argv,envp);
    }
}

