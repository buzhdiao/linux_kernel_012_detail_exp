#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <a.out.h>

#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <asm/segment.h>

#define MAX_ARG_PAGES 32

unsigned long * create_tables(char * p,int argc,int envc) {
    unsigned long *argv,*envp;
    unsigned long * sp;

    sp = (unsigned long *) (0xfffffffc & (unsigned long) p);
    sp -= envc+1;

    envp = sp;
    sp -= argc+1;
    argv = sp;
    put_fs_long((unsigned long)envp,--sp);
    put_fs_long((unsigned long)argv,--sp);
    put_fs_long((unsigned long)argc,--sp);
    while (argc-->0) {
        put_fs_long((unsigned long) p,argv++);
        while (get_fs_byte(p++)) /* nothing */ ;
    }

    put_fs_long(0,argv);
    while (envc-->0) {
        put_fs_long((unsigned long) p,envp++);
        while (get_fs_byte(p++)) /* nothing */ ;
    }

    put_fs_long(0,envp);
    return sp;
}

int count(char ** argv) {
    int i=0;
    char ** tmp;
    if ((tmp = argv)) {
        while (get_fs_long((unsigned long *) (tmp++)))
            i++;
    }
    return i;
}

unsigned long copy_strings(int argc,char ** argv,unsigned long *page,
        unsigned long p, int from_kmem) {
    char *tmp, *pag;
    int len, offset = 0;
    unsigned long old_fs, new_fs;

    if (!p)
        return 0;
    new_fs = get_ds();
    old_fs = get_fs();

    if (from_kmem==2)
        set_fs(new_fs);

    while (argc-- > 0) {
        if (from_kmem == 1)
            set_fs(new_fs);
        if (!(tmp = (char *)get_fs_long(((unsigned long *)argv)+argc)))
            panic("argc is wrong");
        if (from_kmem == 1)
            set_fs(old_fs);
        len = 0;
        do {
            len++;
        } while (get_fs_byte(tmp++));

        if (p-len < 0) {
            set_fs(old_fs);
            return 0;
        }

        while (len) {
            --p; --tmp; --len;
            if (--offset < 0) {
                offset = p % PAGE_SIZE;
                if (from_kmem==2)
                    set_fs(old_fs);
                if (!(pag = (char *) page[p/PAGE_SIZE])) {
                    page[p / PAGE_SIZE] = (unsigned long*) get_free_page();
                    pag = (char*) page[p / PAGE_SIZE];

                    if (!pag)
                        return 0;
                }
                if (from_kmem==2)
                    set_fs(new_fs);
            }
            *(pag + offset) = get_fs_byte(tmp);
        }
    }

    if (from_kmem==2)
        set_fs(old_fs);

    return p;
}

static unsigned long change_ldt(unsigned long text_size,unsigned long * page) {
    unsigned long code_limit,data_limit,code_base,data_base;
    int i;

    code_limit = TASK_SIZE;
    data_limit = TASK_SIZE;
    code_base = get_base(current->ldt[1]);
    data_base = code_base;
    set_base(current->ldt[1],code_base);
    set_limit(current->ldt[1],code_limit);
    set_base(current->ldt[2],data_base);
    set_limit(current->ldt[2],data_limit);
    __asm__("pushl $0x17\n\tpop %%fs"::);
    data_base += data_limit - LIBRARY_SIZE;
    for (i=MAX_ARG_PAGES-1 ; i>=0 ; i--) {
        data_base -= PAGE_SIZE;
        if (page[i])
            put_dirty_page(page[i],data_base);
    }
    return data_limit;
}

int do_execve(unsigned long * eip,long tmp,char * filename,
        char ** argv, char ** envp) {
    struct m_inode * inode;
    struct buffer_head * bh;
    struct exec ex;
    unsigned long page[MAX_ARG_PAGES];
    int i,argc,envc;
    int e_uid, e_gid;
    int retval;
    int sh_bang = 0;
    unsigned long p=PAGE_SIZE*MAX_ARG_PAGES-4;

    if ((0xffff & eip[1]) != 0x000f)
        panic("execve called from supervisor mode");
    for (i=0 ; i<MAX_ARG_PAGES ; i++)
        page[i]=0;
    if (!(inode=namei(filename)))
        return -ENOENT;

    argc = count(argv);
    envc = count(envp);

    if (!S_ISREG(inode->i_mode)) {
        retval = -EACCES;
        goto exec_error2;
    }

    i = inode->i_mode;
    e_uid = (i & S_ISUID) ? inode->i_uid : current->euid;
    e_gid = (i & S_ISGID) ? inode->i_gid : current->egid;

    if (current->euid == inode->i_uid)
        i >>= 6;
    else if (in_group_p(inode->i_gid))
        i >>= 3;
    if (!(i & 1) &&
        !((inode->i_mode & 0111) && suser())) {
        retval = -ENOEXEC;
        goto exec_error2;
    }

    if (!(bh = bread(inode->i_dev,inode->i_zone[0]))) {
        retval = -EACCES;
        goto exec_error2;
    }
    ex = *((struct exec *) bh->b_data);
    brelse(bh);

    if (N_MAGIC(ex) != ZMAGIC || ex.a_trsize || ex.a_drsize ||
            ex.a_text+ex.a_data+ex.a_bss>0x3000000 ||
            inode->i_size < ex.a_text+ex.a_syms+N_TXTOFF(ex)) {
        retval = -ENOEXEC;
        goto exec_error2;
    }

    if (N_TXTOFF(ex) != BLOCK_SIZE) {
        printk("%s: N_TXTOFF != BLOCK_SIZE. See a.out.h.", filename);
        retval = -ENOEXEC;
        goto exec_error2;
    }

    if (!sh_bang) {
        p = copy_strings(envc,envp,page,p,0);
        p = copy_strings(argc,argv,page,p,0);
        if (!p) {
            retval = -ENOMEM;
            goto exec_error2;
        }
    }

    if (current->executable)
        iput(current->executable);
    current->executable = inode;

    for (i=0 ; i<NR_OPEN ; i++)
        if ((current->close_on_exec>>i)&1)
            sys_close(i);
    current->close_on_exec = 0;

    free_page_tables(get_base(current->ldt[1]),get_limit(0x0f));
    free_page_tables(get_base(current->ldt[2]),get_limit(0x17));

    p += change_ldt(ex.a_text,page);
    p -= LIBRARY_SIZE + MAX_ARG_PAGES*PAGE_SIZE;
    p = (unsigned long) create_tables((char *)p,argc,envc);

    current->brk = ex.a_bss +
        (current->end_data = ex.a_data +
         (current->end_code = ex.a_text));
    current->start_stack = p & 0xfffff000;
    current->suid = current->euid = e_uid;
    current->sgid = current->egid = e_gid;
    eip[0] = ex.a_entry;
    eip[3] = p;
    return 0;
exec_error2:
    iput(inode);
exec_error1:
    for (i=0 ; i<MAX_ARG_PAGES ; i++)
        free_page(page[i]);
    return(retval);
}

