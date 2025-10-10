#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <utime.h>

#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/kernel.h>

#include <sys/stat.h>

#include <asm/segment.h>
int sys_utime(char * filename, struct utimbuf * times) {
    struct m_inode * inode;
    long actime,modtime;

    if (!(inode=namei(filename)))
        return -ENOENT;
    if (times) {
        actime = get_fs_long((unsigned long *) &times->actime);
        modtime = get_fs_long((unsigned long *) &times->modtime);
    } else
        actime = modtime = CURRENT_TIME;
    inode->i_atime = actime;
    inode->i_mtime = modtime;
    inode->i_dirt = 1;
    iput(inode);
    return 0;
}

int sys_ls() {
    printk("in sys_ls\n\r");
    int block, i, j;
    int entries;
    struct buffer_head * bh;
    struct dir_entry * de;
    struct m_inode* inode = current->root;
    printk("mode is %d\n\r", inode->i_mode);

    if (S_ISDIR(inode->i_mode)) {
        entries = inode->i_size / (sizeof (struct dir_entry));
        printk("there are %d files in root.\n\r", entries);

        if (!(block = inode->i_zone[0])) {
            printk("empty i_zone\n");
            return -1;
        }

        if (!(bh = bread(inode->i_dev,block))) {
            printk("can not read block %d\n", block);
            return -1;
        }

        de = (struct dir_entry *) bh->b_data;

        for (i = 0; i < entries; i++) {
            if (!de->inode) {
                continue;
            }
            for (j = 0; j < 14; j++) {
                printk("%c", de->name[j]);
            }
            printk("\n");
            de++;
        }
    }

    return 0;
}

int sys_open(const char * filename,int flag,int mode) {
    struct m_inode * inode;
    struct file * f;
    int i,fd;

    mode &= 0777 & ~current->umask;
    for(fd=0 ; fd<NR_OPEN ; fd++) {
        if (!current->filp[fd])
            break;
    }

    if (fd>=NR_OPEN)
        return -EINVAL;

    current->close_on_exec &= ~(1<<fd);
    f=0+file_table;
    for (i=0 ; i<NR_FILE ; i++,f++)
        if (!f->f_count) break;
    if (i>=NR_FILE)
        return -EINVAL;

    (current->filp[fd]=f)->f_count++;
    if ((i=open_namei(filename, flag, mode, &inode))<0) {
        current->filp[fd]=NULL;
        f->f_count=0;
        return i;
    }

    if (S_ISCHR(inode->i_mode)) {
        printk("open char dev %d\n", fd);
    }

    f->f_mode = inode->i_mode;
    f->f_flags = flag;
    f->f_count = 1;
    f->f_inode = inode;
    f->f_pos = 0;

    return fd;
}

int sys_chdir(const char * filename) {
    struct m_inode * inode;
    if (!(inode = namei(filename)))
        return -ENOENT;

    if (!S_ISDIR(inode->i_mode)) {
        iput(inode);
        return -ENOTDIR;
    }

    iput(current->pwd);
    current->pwd = inode;
    return (0);
}

int sys_close(unsigned int fd) {
    struct file * filp;

    if (fd >= NR_OPEN)
        return -EINVAL;
    current->close_on_exec &= ~(1<<fd);
    if (!(filp = current->filp[fd]))
        return -EINVAL;
    current->filp[fd] = NULL;
    if (filp->f_count == 0)
        panic("Close: file count is 0");
    if (--filp->f_count)
        return (0);
    iput(filp->f_inode);
    return (0);
}

