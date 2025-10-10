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

struct exec header = {
    0413, /* a_magic */
    0, /* a_text */
    0, /* a_data */
    0, /* a_bss */
    0, /* a_syms */
    0, /* a_entry */
    0, /* a_trsize */
    0, /* a_drsize */
};
