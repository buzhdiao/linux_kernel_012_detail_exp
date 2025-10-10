#include <errno.h>
#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/kernel.h>
#include <linux/config.h>
#include <sys/param.h>
#include <asm/segment.h>
#include <string.h>

int sys_time(long * tloc) {
    int i;
    i = CURRENT_TIME;
    if (tloc) {
        put_fs_long(i,(unsigned long *)tloc);
    }
    return i;
}

int sys_setsid(void) {
    if (current->leader && !suser())
        return -EPERM;
    current->leader = 1;
    current->session = current->pgrp = current->pid;
    current->tty = -1;
    return current->pgrp;
}

int in_group_p(gid_t grp) {
    int i;

    if (grp == current->egid)
        return 1;

    for (i = 0; i < NGROUPS; i++) {
        if (current->groups[i] == NOGROUP)
            break;
        if (current->groups[i] == grp)
            return 1;
    }
    return 0;
}

int sys_setuid(int uid) {
    if (suser())
        current->uid = current->euid = current->suid = uid;
    else if ((uid == current->uid) || (uid == current->suid))
        current->euid = uid;
    else
        return -EPERM;
    return(0);
}

