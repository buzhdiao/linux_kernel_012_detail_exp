#include <string.h>
#include <sys/stat.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <asm/system.h>

extern struct super_block sb;

struct m_inode inode_table[NR_INODE]={{0,},};

static void read_inode(struct m_inode * inode);
static void write_inode(struct m_inode * inode);

static inline void wait_on_inode(struct m_inode * inode) {
    cli();
    while(inode->i_lock) {
        sleep_on(&inode->i_wait);
    }
    sti();
}

static inline void lock_inode(struct m_inode * inode) {
    cli();
    while(inode->i_lock) {
        sleep_on(&inode->i_wait);
    }
    inode->i_lock = 1;
    sti();
}

static inline void unlock_inode(struct m_inode * inode) {
    inode->i_lock=0;
    wake_up(&inode->i_wait);
}

void iput(struct m_inode * inode) {
    if (!inode)
        return;
    wait_on_inode(inode);
    if (!inode->i_count)
        printk("iput: trying to free free inode");
    if (!inode->i_dev) {
        inode->i_count--;
        return;
    }
repeat:
    if (inode->i_count>1) {
        inode->i_count--;
        return;
    }
    if (!inode->i_nlinks) {
        truncate(inode);
        free_inode(inode);
        return;
    }
    if (inode->i_dirt) {
        write_inode(inode);
        wait_on_inode(inode);
        goto repeat;
    }
    inode->i_count--;
    return;
}

void sync_inodes() {
    int i;
    struct m_inode * inode;

    inode = 0+inode_table;
    for(i=0 ; i<NR_INODE ; i++,inode++) {
        wait_on_inode(inode);
        if (inode->i_dirt && !inode->i_pipe) {
            write_inode(inode);
        }
    }
}

static int _bmap(struct m_inode * inode,int block,int create) {
    struct buffer_head * bh;
    int i;

    if (block<0) {
        printk("_bmap: block<0");
    }

    if (block >= 7+512+512*512) {
        printk("_bmap: block>big");
    }

    if (block < 7) {
        if (create && !inode->i_zone[block]) {
            if ((inode->i_zone[block] = new_block(inode->i_dev))) {
                inode->i_ctime=CURRENT_TIME;
                inode->i_dirt = 1;
            }
        }
        return inode->i_zone[block];
    }
    block -= 7;
    if (block < 512) {
        if (create && !inode->i_zone[7]) {
            if ((inode->i_zone[7] = new_block(inode->i_dev))) {
                inode->i_ctime=CURRENT_TIME;
                inode->i_dirt=1;
            }
        }
        if (!inode->i_zone[7])
            return 0;
        if (!(bh = bread(inode->i_dev,inode->i_zone[7])))
            return 0;
        i = ((unsigned short *) (bh->b_data))[block];
        if (create && !i) {
            if ((i = new_block(inode->i_dev))) {
                ((unsigned short *) (bh->b_data))[block]=i;
                bh->b_dirt = 1;
            }
        }

        brelse(bh);
        return i;
    }

    block -= 512;
    if (create && !inode->i_zone[8]) {
        if ((inode->i_zone[8] = new_block(inode->i_dev))) {
            inode->i_dirt = 1;
            inode->i_ctime=CURRENT_TIME;
        }
    }
    if (!inode->i_zone[8])
        return 0;
    if (!(bh = bread(inode->i_dev,inode->i_zone[8])))
        return 0;
    i = ((unsigned short *) (bh->b_data))[block >> 9];
    if (create && !i) {
        if ((i = new_block(inode->i_dev))) {
            ((unsigned short *) (bh->b_data))[block >> 9]=i;
            bh->b_dirt = 1;
        }
    }
    brelse(bh);

    if (!i)
        return 0;
    if (!(bh = bread(inode->i_dev,i)))
        return 0;
    i = ((unsigned short *)bh->b_data)[block & 511];
    if (create && !i) {
        if ((i = new_block(inode->i_dev))) {
            ((unsigned short *) (bh->b_data))[block & 511] = i;
            bh->b_dirt = 1;
        }
    }
    brelse(bh);
    return i;
}

int bmap(struct m_inode * inode,int block) {
    return _bmap(inode,block,0);
}

int create_block(struct m_inode * inode, int block) {
    return _bmap(inode,block,1);
}

struct m_inode * get_empty_inode() {
    struct m_inode * inode;
    static struct m_inode * last_inode = inode_table;
    int i;

    do {
        inode = NULL;
        for (i = NR_INODE; i ; i--) {
            if (++last_inode >= inode_table + NR_INODE)
                last_inode = inode_table;

            if (!last_inode->i_count) {
                inode = last_inode;
                if (!inode->i_dirt && !inode->i_lock)
                    break;
            }
        }

        if (!inode) {
            for (i=0 ; i<NR_INODE ; i++)
                printk("%04x: %6d\t",inode_table[i].i_dev,
                        inode_table[i].i_num);
            printk("No free inodes in mem");
        }

        wait_on_inode(inode);
        while (inode->i_dirt) {
            write_inode(inode);
            wait_on_inode(inode);
        }
    } while (inode->i_count);

    memset(inode,0,sizeof(*inode));
    inode->i_count = 1;
    return inode;
}

struct m_inode * get_pipe_inode(void) {
    struct m_inode * inode;

    if (!(inode = get_empty_inode()))
        return NULL;
    if (!(inode->i_size=get_free_page())) {
        inode->i_count = 0;
        return NULL;
    }
    inode->i_count = 2;    /* sum of readers/writers */
    PIPE_HEAD(*inode) = PIPE_TAIL(*inode) = 0;
    inode->i_pipe = 1;
    return inode;
}

struct m_inode * iget(int dev, int nr) {
    struct m_inode * inode, * empty;
    if (!dev)
        printk("iget with dev==0");

    inode = inode_table;
    while (inode < NR_INODE+inode_table) {
        if (inode->i_dev != dev || inode->i_num != nr) {
            inode++;
            continue;
        }
        wait_on_inode(inode);   
        if (inode->i_dev != dev || inode->i_num != nr) {
            inode++;
            continue;
        }
        inode->i_count++;
        return inode;
    }

    empty = get_empty_inode();
    if (!empty)
        return NULL;

    inode=empty;
    inode->i_dev = dev;
    inode->i_num = nr;
    read_inode(inode);
    return inode;
}

static void read_inode(struct m_inode * inode) {
    struct super_block * psb;
    struct buffer_head * bh;
    int block;

    psb = &sb;
    lock_inode(inode);
    block = 2 + psb->s_imap_blocks + psb->s_zmap_blocks +
        (inode->i_num-1)/INODES_PER_BLOCK;

    if (!(bh=bread(inode->i_dev,block)))
        printk("unable to read i-node block");


    *(struct d_inode *)inode =
        ((struct d_inode *)bh->b_data)
            [(inode->i_num-1)%INODES_PER_BLOCK];

    brelse(bh);
    unlock_inode(inode);
}

static void write_inode(struct m_inode * inode) {
    struct super_block * psb = &sb;
    struct buffer_head * bh;
    int block;

    lock_inode(inode);
    if (!inode->i_dirt || !inode->i_dev) {
        unlock_inode(inode);
        return;
    }

    block = 2 + psb->s_imap_blocks + psb->s_zmap_blocks +
        (inode->i_num-1)/INODES_PER_BLOCK;

    if (!(bh=bread(inode->i_dev,block)))
        printk("unable to read i-node block");

    ((struct d_inode *)bh->b_data)
        [(inode->i_num-1)%INODES_PER_BLOCK] =
            *(struct d_inode *)inode;

    bh->b_dirt=1;
    inode->i_dirt=0;
    brelse(bh);
    unlock_inode(inode);
}

