#ifndef _COMMON_HPP
#define _COMMON_HPP

#define BLOCKS 1440

#define UPPER(size,n) ((size+((n)-1))/(n))

#define INODE_SIZE (sizeof(struct d_inode))
#define INODE_BLOCKS UPPER(INODES,INODES_PER_BLOCK)
#define INODE_BUFFER_SIZE (INODE_BLOCKS * BLOCK_SIZE)

#define BITS_PER_BLOCK (BLOCK_SIZE<<3)

#define INODES (_super_blk->s_ninodes)
#define ZONESIZE (_super_blk->s_log_zone_size)
#define ZONES (_super_blk->s_nzones)
#define MAXSIZE (_super_blk->s_max_size)
#define IMAPS (_super_blk->s_imap_blocks)
#define ZMAPS (_super_blk->s_zmap_blocks)
#define MAGIC (_super_blk->s_magic)
#define FIRSTZONE (_super_blk->s_firstdatazone)

#define NORM_FIRSTZONE (2+IMAPS+ZMAPS+INODE_BLOCKS)

#endif

