#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>

#include <signal.h>
#include <errno.h>

static inline void save_old(char * from,char * to)
{
	int i;

	verify_area(to, sizeof(struct sigaction));
	for (i=0 ; i< sizeof(struct sigaction) ; i++) {
		put_fs_byte(*from,to);
		from++;
		to++;
	}
}

static inline void get_new(char * from,char * to) {
	int i;

	for (i=0 ; i< sizeof(struct sigaction) ; i++)
		*(to++) = get_fs_byte(from++);
}


int sys_signal(int signum, long handler, long restorer) {
    struct sigaction tmp;

    if (signum<1 || signum>32 || signum==SIGKILL || signum==SIGSTOP)
        return -EINVAL;
    tmp.sa_handler = (void (*)(int)) handler;
    tmp.sa_mask = 0;
    tmp.sa_flags = SA_ONESHOT | SA_NOMASK;
    tmp.sa_restorer = (void (*)(void)) restorer;
    handler = (long) current->sigaction[signum-1].sa_handler;
    current->sigaction[signum-1] = tmp;
    return handler;
}

int sys_sigaction(int signum, const struct sigaction * action,
	struct sigaction * oldaction) {
	struct sigaction tmp;

	if (signum<1 || signum>32 || signum==SIGKILL || signum==SIGSTOP)
		return -EINVAL;
	tmp = current->sigaction[signum-1];
	get_new((char *) action,
		(char *) (signum-1+current->sigaction));
	if (oldaction)
		save_old((char *) &tmp,(char *) oldaction);
	if (current->sigaction[signum-1].sa_flags & SA_NOMASK)
		current->sigaction[signum-1].sa_mask = 0;
	else
		current->sigaction[signum-1].sa_mask |= (1<<(signum-1));
	return 0;
}


/*
 * Routine writes a core dump image in the current directory.
 * Currently not implemented.
 */
int core_dump(long signr) {
    return(0);    /* We didn't do a dump */
}


int do_signal(long signr,long eax,long ebx, long ecx, long edx, long orig_eax,
    long fs, long es, long ds,
    long eip, long cs, long eflags,
    unsigned long * esp, long ss) {
    unsigned long sa_handler;
    long old_eip=eip;
    struct sigaction * sa = current->sigaction + signr - 1;
    int longs;

    unsigned long * tmp_esp;

    printk("pid: %d, signr: %x, eax=%d, oeax = %d, int=%d\n", 
        current->pid, signr, eax, orig_eax, 
        sa->sa_flags & SA_INTERRUPT);

    if ((orig_eax != -1) &&
        ((eax == -ERESTARTSYS) || (eax == -ERESTARTNOINTR))) {
        if ((eax == -ERESTARTSYS) && ((sa->sa_flags & SA_INTERRUPT) ||
            signr < SIGCONT || signr > SIGTTOU))
            *(&eax) = -EINTR;
        else {
            *(&eax) = orig_eax;
            *(&eip) = old_eip -= 2;
        }
    }
    sa_handler = (unsigned long) sa->sa_handler;
    if (sa_handler==1)
        return(1);   /* Ignore, see if there are more signals... */
    if (!sa_handler) {
        switch (signr) {
        case SIGCONT:
        case SIGCHLD:
            return(1);  /* Ignore, ... */

        case SIGSTOP:
        case SIGTSTP:
        case SIGTTIN:
        case SIGTTOU:
            current->state = TASK_STOPPED;
            current->exit_code = signr;
            if (!(current->p_pptr->sigaction[SIGCHLD-1].sa_flags & 
                    SA_NOCLDSTOP))
                current->p_pptr->signal |= (1<<(SIGCHLD-1));
            return(1);  /* Reschedule another event */

        case SIGQUIT:
        case SIGILL:
        case SIGTRAP:
        case SIGIOT:
        case SIGFPE:
        case SIGSEGV:
            if (core_dump(signr))
                do_exit(signr|0x80);
            /* fall through */
        default:
            do_exit(signr);
        }
    }
    /*
     * OK, we're invoking a handler 
     */
    if (sa->sa_flags & SA_ONESHOT)
        sa->sa_handler = NULL;
    *(&eip) = sa_handler;
    longs = (sa->sa_flags & SA_NOMASK)?7:8;
    *(&esp) -= longs;
    verify_area(esp,longs*4);
    tmp_esp=esp;
    put_fs_long((long) sa->sa_restorer,tmp_esp++);
    put_fs_long(signr,tmp_esp++);
    if (!(sa->sa_flags & SA_NOMASK))
        put_fs_long(current->blocked,tmp_esp++);
    put_fs_long(eax,tmp_esp++);
    put_fs_long(ecx,tmp_esp++);
    put_fs_long(edx,tmp_esp++);
    put_fs_long(eflags,tmp_esp++);
    put_fs_long(old_eip,tmp_esp++);
    current->blocked |= sa->sa_mask;
    return(0);        /* Continue, execute handler */
}
