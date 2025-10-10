#include <linux/config.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/system.h>

#include <errno.h>

#define test_bit(bitnr,addr) ({ \
register int __res __asm__("ax"); \
__asm__("bt %2,%3;setb %%al":"=a" (__res):"a" (0),"r" (bitnr),"m" (*(addr))); \
__res; })

int ROOT_DEV = 0;

struct super_block sb;

struct super_block * read_super(int dev) {
    struct super_block * s = &sb;
    struct buffer_head * bh;
    int i,block;

    if (!(bh = bread(dev,1))) {
        printk("read super error!\n");
    }

    brelse(bh);

    *((struct d_super_block *) s) =
        *((struct d_super_block *) bh->b_data);

    if (s->s_magic != SUPER_MAGIC) {
        printk("read super error!\n");
        return NULL;
    }
    printk("check magic ok!\n\r");

    for (i=0;i<I_MAP_SLOTS;i++)
        s->s_imap[i] = NULL;
    for (i=0;i<Z_MAP_SLOTS;i++)
        s->s_zmap[i] = NULL;
    block=2;
    for (i=0 ; i < s->s_imap_blocks ; i++) {
        if ((s->s_imap[i] = bread(dev,block)))
            block++;
        else
            break;
    }
    for (i=0 ; i < s->s_zmap_blocks ; i++) {
        if ((s->s_zmap[i] = bread(dev,block)))
            block++;
        else
            break;
    }

    if (block != 2+s->s_imap_blocks+s->s_zmap_blocks) {
        for(i=0;i<I_MAP_SLOTS;i++)
            brelse(s->s_imap[i]);
        for(i=0;i<Z_MAP_SLOTS;i++)
            brelse(s->s_zmap[i]);
        s->s_dev=0;
        return NULL;
    }

    s->s_imap[0]->b_data[0] |= 1;
    s->s_zmap[0]->b_data[0] |= 1;

    printk("read super successfully! %d, %d\n", s->s_imap_blocks,
            s->s_zmap_blocks);

    return s;
}

void mount_root() {
    int i,free;
    struct super_block * p;
    struct m_inode * mi;

    for(i=0;i<NR_FILE;i++)
        file_table[i].f_count=0;

    if (!(p = read_super(ROOT_DEV))) {
        printk("Unable to mount root");
    }

    if (!(mi = iget(ROOT_DEV,ROOT_INO)))
        printk("Unable to read root i-node");

    mi->i_count += 3 ;
    p->s_isup = p->s_imount = mi;
    current->pwd = mi;
    current->root = mi;

    free = 0;
    i = p->s_nzones;
    while (-- i >= 0)
        if (!test_bit(i&8191,p->s_zmap[i>>13]->b_data))
            free++;

    printk("%d/%d free blocks\n\r",free,p->s_nzones);
    free=0;
    i=p->s_ninodes+1;
    while (-- i >= 0)
        if (!test_bit(i&8191,p->s_imap[i>>13]->b_data))
            free++;
    printk("%d/%d free inodes\n\r",free,p->s_ninodes);
}

