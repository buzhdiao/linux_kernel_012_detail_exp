#ifndef _A_OUT_H
#define _A_OUT_H

#define __GNU_EXEC_MACROS__

struct exec {
    unsigned long a_magic;
    unsigned a_text;
    unsigned a_data;
    unsigned a_bss;
    unsigned a_syms;
    unsigned a_entry;
    unsigned a_trsize;
    unsigned a_drsize;
};

#ifndef N_MAGIC
#define N_MAGIC(exec) ((exec).a_magic)
#endif

#ifndef OMAGIC
#define OMAGIC 0407
#define NMAGIC 0410
#define ZMAGIC 0413
#endif

#ifndef N_BADMAG
#define N_BADMAG(x) \
    (N_MAGIC(x) != OMAGIC && N_MAGIC(x) != NMAGIC   \
     && N_MAGIC(x) != ZMAGIC)
#endif

#define _N_BADMAG(x)    \
    (N_MAGIC(x) != OMAGIC && N_MAGIC(x) != NMAGIC   \
     && N_MAGIC(x) != ZMAGIC)

#define _N_HDROFF(x) (SEGMENT_SIZE - sizeof (struct exec))

#ifndef N_TXTOFF
#define N_TXTOFF(x) \
    (N_MAGIC(x) == ZMAGIC ? SEGMENT_SIZE : sizeof (struct exec))
#endif

#ifndef N_DATOFF
#define N_DATOFF(x) (N_TXTOFF(x) + (x).a_text)
#endif

#ifndef N_TRELOFF
#define N_TRELOFF(x) (N_DATOFF(x) + (x).a_data)
#endif

#ifndef N_DRELOFF
#define N_DRELOFF(x) (N_TRELOFF(x) + (x).a_trsize)
#endif

#ifndef N_SYMOFF
#define N_SYMOFF(x) (N_DRELOFF(x) + (x).a_drsize)
#endif

#ifndef N_STROFF
#define N_STROFF(x) (N_SYMOFF(x) + (x).a_syms)
#endif

#ifndef N_TXTADDR
#define N_TXTADDR(x) 0
#endif

#define PAGE_SIZE 4096
#define SEGMENT_SIZE 1024

#define _N_SEGMENT_ROUND(x) (((x) + SEGMENT_SIZE - 1) & ~(SEGMENT_SIZE - 1))
#define _N_TXTENDADDR(x) (N_TXTADDR(x)+(x).a_text)

#ifndef N_DATADDR
#define N_DATADDR(x) \
    (N_MAGIC(x)==OMAGIC? (_N_TXTENDADDR(x)) \
     : (_N_SEGMENT_ROUND (_N_TXTENDADDR(x))))
#endif

#ifndef N_BSSADDR
#define N_BSSADDR(x) (N_DATADDR(x) + (x).a_data)
#endif

#ifndef N_NLIST_DECLARED
struct nlist {
    union {
        char *n_name;
        struct nlist *n_next;
        long n_strx;
    } n_un;

    unsigned char n_type;
    char n_other;
    short n_desc;
    unsigned long n_value;
};
#endif

#ifndef N_UNDF
#define N_UNDF 0
#endif
#ifndef N_ABS
#define N_ABS 2
#endif
#ifndef N_TEXT
#define N_TEXT 4
#endif
#ifndef N_DATA
#define N_DATA 6
#endif
#ifndef N_BSS
#define N_BSS 8
#endif
#ifndef N_COMM
#define N_COMM 10
#endif
#ifndef N_FN
#define N_FN 15
#endif

#ifndef N_EXT
#define N_EXT 1
#endif
#ifndef N_TYPE
#define N_TYPE 036
#endif
#ifndef N_STAB
#define N_STAB 0340
#endif

#define N_INDR 0xa

/* constants for ld. */
#define N_SETA  0x14
#define N_SETT  0x16
#define N_SETD  0x18
#define N_SETB  0x1A

#define N_SETV  0x1C

#ifndef N_RELOCATION_INFO_DECLARED
struct relocation_info {
    int r_address;
    unsigned int r_symbolnum:24;
    unsigned int r_pcrel:1;
    unsigned int r_length:2;
    unsigned int r_extern:1;
    unsigned int r_pad:4;
};
#endif

#endif

