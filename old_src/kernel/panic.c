#include <linux/kernel.h>
#include <linux/sched.h>

int sys_sync();

void panic(const char * s) {
    printk("Kernel panic: %s\n\r",s);
    if (current == task[0]) {
        printk("In swapper task - not syncing\n\r");
    }
    else {
        sys_sync();
    }

    for (;;);
}

