#include <errno.h>
#include <signal.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <asm/segment.h>
#include <sys/wait.h>

void release(struct task_struct * p) {
    int i;

    if (!p)
        return;
    if (p == current) {
        printk("task releasing itself\n\r");
        return;
    }
    for (i=1 ; i<NR_TASKS ; i++) {
        if (task[i]==p) {
            task[i]=NULL;
            /* Update links */
            if (p->p_osptr)
                p->p_osptr->p_ysptr = p->p_ysptr;
            if (p->p_ysptr)
                p->p_ysptr->p_osptr = p->p_osptr;
            else
                p->p_pptr->p_cptr = p->p_osptr;
            free_page((long)p);
            schedule();
            return;
        }
    }
    panic("trying to release non-existent task");
}

static inline int send_sig(long sig,struct task_struct * p,int priv) {
    if (!p)
        return -EINVAL;
    if (!priv && (current->euid != p->euid) && !suser())
        return -EPERM;
    if ((sig == SIGKILL) || (sig == SIGCONT)) {
        if (p->state == TASK_STOPPED)
            p->state = TASK_RUNNING;
        p->exit_code = 0;
        p->signal &= ~( (1<<(SIGSTOP-1)) | (1<<(SIGTSTP-1)) |
                (1<<(SIGTTIN-1)) | (1<<(SIGTTOU-1)) );
    } 
    /* If the signal will be ignored, don't even post it */
    if ((int) p->sigaction[sig-1].sa_handler == 1)
        return 0;
    /* Depends on order SIGSTOP, SIGTSTP, SIGTTIN, SIGTTOU */
    if ((sig >= SIGSTOP) && (sig <= SIGTTOU)) 
        p->signal &= ~(1<<(SIGCONT-1));
    /* Actually deliver the signal */
    p->signal |= (1<<(sig-1));
    return 0;
}

int kill_pg(int pgrp, int sig, int priv) {
    struct task_struct **p;
    int err,retval = -ESRCH;
    int found = 0;

    if (sig<1 || sig>32 || pgrp <= 0)
        return -EINVAL;
    for (p = &LAST_TASK ; p > &FIRST_TASK ; --p) {
        if ((*p)->pgrp == pgrp) {
            if (sig && (err = send_sig(sig,*p,priv)))
                retval = err;
            else
                found++;
        }
    }

    return(found ? 0 : retval);
}

int kill_proc(int pid, int sig, int priv)
{
     struct task_struct **p;

    if (sig<1 || sig>32)
        return -EINVAL;
    for (p = &LAST_TASK ; p > &FIRST_TASK ; --p)
        if ((*p)->pid == pid)
            return(sig ? send_sig(sig,*p,priv) : 0);
    return(-ESRCH);
}

int sys_kill(int pid,int sig) {
    struct task_struct **p = NR_TASKS + task;
    int err, retval = 0;

    if (!pid)
        return(kill_pg(current->pid,sig,0));
    if (pid == -1) {
        while (--p > &FIRST_TASK)
            if (err = send_sig(sig,*p,0))
                retval = err;
        return(retval);
    }
    if (pid < 0) 
        return(kill_pg(-pid,sig,0));
    /* Normal kill */
    return(kill_proc(pid,sig,0));
}

int is_orphaned_pgrp(int pgrp) {
    struct task_struct **p;

    for (p = &LAST_TASK ; p > &FIRST_TASK ; --p) {
        if (!(*p) ||
            ((*p)->pgrp != pgrp) || 
            ((*p)->state == TASK_ZOMBIE) ||
            ((*p)->p_pptr->pid == 1))
            continue;
        if (((*p)->p_pptr->pgrp != pgrp) &&
            ((*p)->p_pptr->session == (*p)->session))
            return 0;
    }
    return(1);    /* (sighing) "Often!" */
}

static int has_stopped_jobs(int pgrp) {
    struct task_struct ** p;

    for (p = &LAST_TASK ; p > &FIRST_TASK ; --p) {
        if ((*p)->pgrp != pgrp)
            continue;
        if ((*p)->state == TASK_STOPPED)
            return(1);
    }
    return(0);
}

volatile void do_exit(long code) {
    struct task_struct *p;
    int i;

    free_page_tables(get_base(current->ldt[1]),get_limit(0x0f));
    free_page_tables(get_base(current->ldt[2]),get_limit(0x17));
    for (i=0 ; i<NR_OPEN ; i++)
        if (current->filp[i])
            sys_close(i);
    iput(current->pwd);
    current->pwd = NULL;
    iput(current->root);
    current->root = NULL;
    iput(current->executable);
    current->executable = NULL;
    iput(current->library);
    current->library = NULL;
    current->state = TASK_ZOMBIE;
    current->exit_code = code;
    /* 
     * Check to see if any process groups have become orphaned
     * as a result of our exiting, and if they have any stopped
     * jobs, send them a SIGUP and then a SIGCONT.  (POSIX 3.2.2.2)
     *
     * Case i: Our father is in a different pgrp than we are
     * and we were the only connection outside, so our pgrp
     * is about to become orphaned.
      */
    if ((current->p_pptr->pgrp != current->pgrp) &&
        (current->p_pptr->session == current->session) &&
        is_orphaned_pgrp(current->pgrp) &&
        has_stopped_jobs(current->pgrp)) {
        kill_pg(current->pgrp,SIGHUP,1);
        kill_pg(current->pgrp,SIGCONT,1);
    }
    /* Let father know we died */
    current->p_pptr->signal |= (1<<(SIGCHLD-1));
    
    /*
     * This loop does two things:
     * 
       * A.  Make init inherit all the child processes
     * B.  Check to see if any process groups have become orphaned
     *    as a result of our exiting, and if they have any stopped
     *    jons, send them a SIGUP and then a SIGCONT.  (POSIX 3.2.2.2)
     */
    if (p = current->p_cptr) {
        while (1) {
            p->p_pptr = task[1];
            if (p->state == TASK_ZOMBIE)
                task[1]->signal |= (1<<(SIGCHLD-1));
            /*
             * process group orphan check
             * Case ii: Our child is in a different pgrp 
             * than we are, and it was the only connection
             * outside, so the child pgrp is now orphaned.
             */
            if ((p->pgrp != current->pgrp) &&
                (p->session == current->session) &&
                is_orphaned_pgrp(p->pgrp) &&
                has_stopped_jobs(p->pgrp)) {
                kill_pg(p->pgrp,SIGHUP,1);
                kill_pg(p->pgrp,SIGCONT,1);
            }
            if (p->p_osptr) {
                p = p->p_osptr;
                continue;
            }
            /*
             * This is it; link everything into init's children 
             * and leave 
             */
            p->p_osptr = task[1]->p_cptr;
            task[1]->p_cptr->p_ysptr = p;
            task[1]->p_cptr = current->p_cptr;
            current->p_cptr = 0;
            break;
        }
    }
    if (current->leader) {
        struct task_struct **p;
        struct tty_struct *tty;

        if (current->tty >= 0) {
            tty = tty_table + current->tty;
            if (tty->pgrp>0)
                kill_pg(tty->pgrp, SIGHUP, 1);
            tty->pgrp = 0;
            tty->session = 0;
        }
         for (p = &LAST_TASK ; p > &FIRST_TASK ; --p)
            if ((*p)->session == current->session)
                (*p)->tty = -1;
    }

    schedule();
}

int sys_exit(int error_code) {
    do_exit((error_code&0xff)<<8);
}

int sys_waitpid(pid_t pid,unsigned long * stat_addr, int options) {
    int flag;
    struct task_struct *p;
    unsigned long oldblocked;

    verify_area(stat_addr,4);
repeat:
    flag=0;
    for (p = current->p_cptr ; p ; p = p->p_osptr) {
        if (pid>0) {
            if (p->pid != pid)
                continue;
        } else if (!pid) {
            if (p->pgrp != current->pgrp)
                continue;
        } else if (pid != -1) {
            if (p->pgrp != -pid)
                continue;
        }
        switch (p->state) {
            case TASK_STOPPED:
                if (!(options & WUNTRACED) || 
                    !p->exit_code)
                    continue;
                put_fs_long((p->exit_code << 8) | 0x7f,
                    stat_addr);
                p->exit_code = 0;
                return p->pid;
            case TASK_ZOMBIE:
                current->cutime += p->utime;
                current->cstime += p->stime;
                flag = p->pid;
                put_fs_long(p->exit_code, stat_addr);
                release(p);
                return flag;
            default:
                flag=1;
                continue;
        }
    }
    if (flag) {
        if (options & WNOHANG)
            return 0;
        current->state=TASK_INTERRUPTIBLE;
        oldblocked = current->blocked;
        current->blocked &= ~(1<<(SIGCHLD-1));
        schedule();
        current->blocked = oldblocked;
        if (current->signal & ~(current->blocked | (1<<(SIGCHLD-1))))
            return -ERESTARTSYS;
        else
            goto repeat;
    }
    return -ECHILD;
}
