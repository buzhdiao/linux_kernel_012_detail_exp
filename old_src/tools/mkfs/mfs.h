#ifndef _FS_H
#define _FS_H

#include <sys/types.h>
#include <stdio.h>

#define I_DIRECTORY     0040000
#define I_REGULAR       0100000

enum Mode {
    READ,
    WRITE,
    LIST,
    EXTRACT,
    COPY,
    COPY_FILE,
    MAKE_DIR,
    INIT,
};

void buffer_init(long buffer_end);

#define MAJOR(a) (((unsigned)(a))>>8)
#define MINOR(a) ((a)&0xff)

#define NAME_LEN 14
#define ROOT_INO 1

#define I_MAP_SLOTS 8
#define Z_MAP_SLOTS 8

#define SUPER_MAGIC 0x137F

#define NR_OPEN 20
#define NR_INODE 32
#define NR_FILE 64
#define NR_SUPER 8
#define NR_HASH 307
#define BLOCK_SIZE 1024

#ifndef NULL
#define NULL ((void *) 0)
#endif

#define INODES_PER_BLOCK ((BLOCK_SIZE)/(sizeof (struct d_inode)))
#define DIR_ENTRIES_PER_BLOCK ((BLOCK_SIZE)/(sizeof (struct dir_entry)))

struct buffer_head {
    char * b_data;
    unsigned long b_blocknr;
    unsigned short b_dev;
    unsigned char b_uptodate;
    unsigned char b_dirt;
    unsigned char b_count;
    unsigned char b_lock;
    struct task_struct * b_wait;
    struct buffer_head * b_prev;
    struct buffer_head * b_next;
    struct buffer_head * b_prev_free;
    struct buffer_head * b_next_free;
};

struct d_inode {
    unsigned short i_mode;
    unsigned short i_uid;
    unsigned int i_size;
    unsigned int i_time;
    unsigned char i_gid;
    unsigned char i_nlinks;
    unsigned short i_zone[9];
};

class m_inode {
public:
    d_inode* p;
    unsigned short i_num;

    m_inode(d_inode* dp, unsigned short num): p(dp), i_num(num) {}
    ~m_inode() {
        p = NULL;
    }
};

struct file {
    unsigned short f_mode;
    unsigned short f_flags;
    unsigned short f_count;
    struct m_inode * f_inode;
    off_t f_pos;
};

struct super_block {
    unsigned short s_ninodes;
    unsigned short s_nzones;
    unsigned short s_imap_blocks;
    unsigned short s_zmap_blocks;
    unsigned short s_firstdatazone;
    unsigned short s_log_zone_size;
    unsigned int s_max_size;
    unsigned short s_magic;
    char * s_imap[8];
    char * s_zmap[8];
};

struct d_super_block {
    unsigned short s_ninodes;
    unsigned short s_nzones;
    unsigned short s_imap_blocks;
    unsigned short s_zmap_blocks;
    unsigned short s_firstdatazone;
    unsigned short s_log_zone_size;
    unsigned int s_max_size;
    unsigned short s_magic;
};

struct dir_entry {
    unsigned short inode;
    char name[NAME_LEN];
};


extern struct m_inode inode_table[NR_INODE];
extern struct file file_table[NR_FILE];

extern void truncate(struct m_inode * inode);
extern void sync_inodes(void);
extern int create_block(struct m_inode * inode,int block);

extern struct m_inode * new_inode(int dev);
extern void iput(struct m_inode * inode);

extern inline void wait_on_buffer(struct buffer_head* bh);
extern struct buffer_head * getblk(int dev, int block);
extern void ll_rw_block(int rw, struct buffer_head * bh);
extern void brelse(struct buffer_head * buf);
extern struct buffer_head * bread(int dev,int block);
extern void bread_page(unsigned long addr,int dev,int b[4]);

extern struct m_inode * namei(const char * pathname);
extern int open_namei(const char * pathname, int flag, int mode,
        struct m_inode ** res_inode);

extern struct m_inode * get_empty_inode(void);
extern struct buffer_head * get_hash_table(int dev, int block);

extern int new_block(int dev);
extern int free_block(int dev, int block);
extern int sync_dev(int dev);

extern struct super_block * get_super(int dev);

char* get_dir_name(char* full_name);
char* get_entry_name(char* full_name);
void free_strings(char** dirs);
char** split(const char* name);
long file_length(FILE* f);

#endif
