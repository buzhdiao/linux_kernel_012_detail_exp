#include <linux/sched.h>

unsigned long HIGH_MEMORY = 0;

unsigned char mem_map [ PAGING_PAGES ] = {0,};

#define copy_page(from,to) \
__asm__("cld ; rep ; movsl"::"S" (from),"D" (to),"c" (1024):)

static inline void oom() {
    printk("out of memory\n\r");
    //do_exit(SIGSEGV);
}

void free_page(unsigned long addr) {
    if (addr < LOW_MEM) return;
    if (addr >= HIGH_MEMORY)
        panic("trying to free nonexistent page");

    addr -= LOW_MEM;
    addr >>= 12;
    if (mem_map[addr]--) return;
    mem_map[addr]=0;
    panic("trying to free free page");
}

int free_page_tables(unsigned long from,unsigned long size) {
    unsigned long *pg_table;
    unsigned long * dir, nr;

    if (from & 0x3fffff)
        panic("free_page_tables called with wrong alignment");
    if (!from)
        panic("Trying to free up swapper memory space");
    size = (size + 0x3fffff) >> 22;
    dir = (unsigned long *) ((from>>20) & 0xffc);

    for ( ; size-->0 ; dir++) {
        if (!(1 & *dir))
            continue;
        pg_table = (unsigned long *) (0xfffff000 & *dir);
        for (nr=0 ; nr<1024 ; nr++) {
            if (*pg_table) {
                if (1 & *pg_table)
                    free_page(0xfffff000 & *pg_table);
                *pg_table = 0;
            }
            pg_table++;
        }
        free_page(0xfffff000 & *dir);
        *dir = 0;
    }
    invalidate();
    return 0;
}

int copy_page_tables(unsigned long from,unsigned long to,long size) {
    unsigned long * from_page_table;
    unsigned long * to_page_table;
    unsigned long this_page;
    unsigned long * from_dir, * to_dir;
    unsigned long nr;

    if ((from&0x3fffff) || (to&0x3fffff)) {
        panic("copy_page_tables called with wrong alignment");
    }

    /* Get high 10 bits. As PDE is 4 byts, so right shift 20.*/
    from_dir = (unsigned long *) ((from>>20) & 0xffc);
    to_dir = (unsigned long *) ((to>>20) & 0xffc);

    size = ((unsigned) (size+0x3fffff)) >> 22;
    for( ; size-->0 ; from_dir++,to_dir++) {
        if (1 & *to_dir)
            panic("copy_page_tables: already exist");
        if (!(1 & *from_dir))
            continue;

        from_page_table = (unsigned long *) (0xfffff000 & *from_dir);
        if (!(to_page_table = (unsigned long *) get_free_page()))
            return -1;

        *to_dir = ((unsigned long) to_page_table) | 7;
        nr = (from==0)?0xA0:1024;

        for ( ; nr-- > 0 ; from_page_table++,to_page_table++) {
            this_page = *from_page_table;
            if (!this_page)
                continue;
            if (!(1 & this_page))
                continue;

            this_page &= ~2;
            *to_page_table = this_page;

            if (this_page > LOW_MEM) {
                *from_page_table = this_page;
                this_page -= LOW_MEM;
                this_page >>= 12;
                mem_map[this_page]++;
            }
        }
    }
    invalidate();
    return 0;
}

void un_wp_page(unsigned long * table_entry) {
    unsigned long old_page,new_page;
    old_page = 0xfffff000 & *table_entry;

    if (old_page >= LOW_MEM && mem_map[MAP_NR(old_page)]==1) {
        *table_entry |= 2;
        invalidate();
        return;
    }

    new_page=get_free_page();
    if (old_page >= LOW_MEM)
        mem_map[MAP_NR(old_page)]--;
    copy_page(old_page,new_page);
    *table_entry = new_page | 7;
    invalidate();
}

void do_wp_page(unsigned long error_code, unsigned long address) {
    if (address < TASK_SIZE)
        panic("\n\rBAD! KERNEL MEMORY WP-ERR!\n\r");

    un_wp_page((unsigned long *)
            (((address>>10) & 0xffc) + (0xfffff000 &
                *((unsigned long *) ((address>>20) &0xffc)))));
}

void write_verify(unsigned long address) {
	unsigned long page;

	if (!( (page = *((unsigned long *) ((address>>20) & 0xffc)) )&1))
		return;
	page &= 0xfffff000;
	page += ((address>>10) & 0xffc);
	if ((3 & *(unsigned long *) page) == 1)  /* non-writeable, present */
		un_wp_page((unsigned long *) page);
	return;
}

static unsigned long put_page(unsigned long page,unsigned long address) {
    unsigned long tmp, *page_table;
    if (page < LOW_MEM || page >= HIGH_MEMORY)
        printk("Trying to put page %p at %p\n",page,address);
    if (mem_map[(page-LOW_MEM)>>12] != 1)
        printk("mem_map disagrees with %p at %p\n",page,address);

    page_table = (unsigned long *) ((address>>20) & 0xffc);
    if ((*page_table)&1)
        page_table = (unsigned long *) (0xfffff000 & *page_table);
    else {
        if (!(tmp=get_free_page()))
            return 0;
        *page_table = tmp | 7;
        page_table = (unsigned long *) tmp;
    }
    page_table[(address>>12) & 0x3ff] = page | 7;
    return page;
}

unsigned long put_dirty_page(unsigned long page, unsigned long address) {
    unsigned long tmp, *page_table;

    if (page < LOW_MEM || page >= HIGH_MEMORY)
        printk("Trying to put page %p at %p\n",page,address);
    if (mem_map[(page-LOW_MEM)>>12] != 1)
        printk("mem_map disagrees with %p at %p\n",page,address);
    page_table = (unsigned long *) ((address>>20) & 0xffc);
    if ((*page_table)&1)
        page_table = (unsigned long *) (0xfffff000 & *page_table);
    else {
        if (!(tmp=get_free_page()))
            return 0;
        *page_table = tmp|7;
        page_table = (unsigned long *) tmp;
    }
    page_table[(address>>12) & 0x3ff] = page | (PAGE_DIRTY | 7);
    return page;
}

void get_empty_page(unsigned long address) {
    unsigned long tmp;
    if (!(tmp = get_free_page()) || !put_page(tmp, address)) {
        free_page(tmp);
        oom();
    }
}

static int try_to_share(unsigned long address, struct task_struct * p) {
    unsigned long from;
    unsigned long to;
    unsigned long from_page;
    unsigned long to_page;
    unsigned long phys_addr;

    from_page = to_page = ((address>>20) & 0xffc);
    from_page += ((p->start_code>>20) & 0xffc);
    to_page += ((current->start_code>>20) & 0xffc);

    from = *(unsigned long *) from_page;
    if (!(from & 1))
        return 0;
    from &= 0xfffff000;
    from_page = from + ((address>>10) & 0xffc);
    phys_addr = *(unsigned long *) from_page;
    if ((phys_addr & 0x41) != 0x01)
        return 0;
    phys_addr &= 0xfffff000;
    if (phys_addr >= HIGH_MEMORY || phys_addr < LOW_MEM)
        return 0;

    to = *(unsigned long *) to_page;
    if (!(to & 1)) {
        if ((to = get_free_page()))
            *(unsigned long *) to_page = to | 7;
        else
            oom();
    }

    to &= 0xfffff000;
    to_page = to + ((address>>10) & 0xffc);
    if (1 & *(unsigned long *) to_page)
        panic("try_to_share: to_page already exists");

    *(unsigned long *) from_page &= ~2;
    *(unsigned long *) to_page = *(unsigned long *) from_page;
    invalidate();
    phys_addr -= LOW_MEM;
    phys_addr >>= 12;
    mem_map[phys_addr]++;
    return 1;
}

static int share_page(struct m_inode * inode, unsigned long address) {
    struct task_struct ** p;

    if (inode->i_count < 2 || !inode)
        return 0;

    for (p = &LAST_TASK ; p > &FIRST_TASK ; --p) {
        if (!*p)
            continue;
        if (current == *p)
            continue;
        if (inode != (*p)->executable)
            continue;
        if (try_to_share(address, *p))
            return 0;
    }

    return 0;
}

void do_no_page(unsigned long error_code,unsigned long address) {
	int nr[4];
	unsigned long tmp;
	unsigned long page;
	int block,i;
	struct m_inode * inode;

	if (address < TASK_SIZE)
		panic("\n\rBAD!! KERNEL PAGE MISSING\n\r");
	if (address - current->start_code > TASK_SIZE) {
		panic("Bad things happen: nonexistent page error in do_no_page\n\r");
		//do_exit(SIGSEGV);
	}
	page = *(unsigned long *) ((address >> 20) & 0xffc);
	address &= 0xfffff000;
	tmp = address - current->start_code;
    if (tmp >= LIBRARY_OFFSET ) {
        inode = current->library;
        block = 1 + (tmp-LIBRARY_OFFSET) / BLOCK_SIZE;
    } else if (tmp < current->end_data) {
        inode = current->executable;
        block = 1 + tmp / BLOCK_SIZE;
    } else {
        inode = NULL;
        block = 0;
    }

    if (!inode) {
        get_empty_page(address);
        return;
    }

	if (share_page(inode,tmp))
		return;
	if (!(page = get_free_page()))
		oom();
/* remember that 1 block is used for header */
	for (i=0 ; i<4 ; block++,i++)
		nr[i] = bmap(inode,block);
	bread_page(page,inode->i_dev,nr);
	i = tmp + 4096 - current->end_data;
	if (i>4095)
		i = 0;
	tmp = page + 4096;
	while (i-- > 0) {
		tmp--;
		*(char *)tmp = 0;
	}
	if (put_page(page,address))
		return;
	free_page(page);
	oom();
}

void mem_init(long start_mem, long end_mem) {
    int i;

    HIGH_MEMORY = end_mem;

    for (i = 0; i < PAGING_PAGES; i++) {
        mem_map[i] = USED;
    }

    i = MAP_NR(start_mem);
    end_mem -= start_mem;
    end_mem >>= 12;
    while (end_mem--) {
        mem_map[i++] = 0;
    }
}

