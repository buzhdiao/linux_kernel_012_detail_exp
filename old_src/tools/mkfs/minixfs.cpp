#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "minixfs.hpp"

#define bitop(name,op) \
static inline int name(char * addr,unsigned int nr) \
{ \
    int __res; \
    __asm__ __volatile__("bt" op " %1,%2; adcl $0,%0" \
            :"=g" (__res) \
            :"r" (nr),"m" (*(addr)),"0" (0)); \
    return __res; \
}

bitop(bit,"")
bitop(setbit,"s")
bitop(clrbit,"r")

#define mark_inode(x) (setbit(_inode_map,(x)))
#define unmark_inode(x) (clrbit(_inode_map,(x)))

#define mark_zone(x) (setbit(_zone_map,(x)-FIRSTZONE+1))
#define unmark_zone(x) (clrbit(_zone_map,(x)-FIRSTZONE+1))

FileSystem::FileSystem(char* img_name, bool is_create) {
    image_name = img_name;
    for (int i = 0; i < BLOCKS; i++) {
        _images[i] = NULL;
    }
    _super_blk = NULL;
    _inode_map = NULL;
    _zone_map  = NULL;

    load_image(is_create);
}

int FileSystem::read_block(FILE* f, int block) {
    if (_images[block] == NULL) {
        _images[block] = new char[BLOCK_SIZE];
    }

    if (fread(_images[block], sizeof(char), BLOCK_SIZE, f) != BLOCK_SIZE) {
        printf("read file error, block is %d\n", block);
        return -1;
    }

    return 0;
}

int FileSystem::load_image(bool is_create) {
    if (is_create) {
        for (int i = 0; i < BLOCKS; i++) {
            _images[i] = new char[BLOCK_SIZE];
        }
        setup_super_block();
        setup_map();
        return 0;
    }

    FILE* f = fopen(image_name, "r+");
    for (int i = 0; i < BLOCKS; i++) {
        if (read_block(f, i) != 0) {
            release_images();
            fclose(f);
            return -1;
        }
    }
    fclose(f);

    _super_blk = (super_block*)_images[1];
    _inode_map = _images[2];
    _zone_map  = _images[2 + IMAPS];

    _root = iget(ROOT_INO);

    return 0;
}

void FileSystem::release_images() {
    for (int i = 0; i < BLOCKS; i++) {
        if (i >= 4 && i < 10)
            continue;
        if (_images[i]) {
            delete[] _images[i];
            _images[i] = NULL;
        }
    }
}

m_inode* FileSystem::namei(char* dir_name) {
    assert(dir_name[0] == '/');
    char** dirs = split(dir_name);
    char** dir_list = dirs;

    m_inode* pwd = new m_inode(_root, 1);

    while (*dir_list) {
        m_inode* old = pwd;
        pwd = find_entry(pwd, *dir_list);
        delete old;
        if (!pwd) {
            printf("Error: file/dir %s not exists\n", *(dir_list));
            return NULL;
        }
        dir_list++;
    }

    free_strings(dirs);
    return pwd;
}

m_inode* FileSystem::find_entry(m_inode* pwd, char* name) {
    char* pwd_block = get_block(bmap(pwd->p, 0));

    for (int i = 0; i < pwd->p->i_size / sizeof(struct dir_entry); i++) {
        struct dir_entry* dir = ((struct dir_entry*)pwd_block) + i;
        if (strcmp(dir->name, name) == 0) {
            return new m_inode(iget(dir->inode), dir->inode);
        }
    }
    return NULL;
}

char * FileSystem::get_block(int nblock) {
    return _images[nblock];
}

unsigned short FileSystem::_bmap(struct d_inode* inode, int n, bool is_create) {
    char* ind;
    if (n < 7) {
        if (is_create && !inode->i_zone[n]) {
            inode->i_zone[n] = get_new_block();
        }
        return inode->i_zone[n];
    }

    n -= 7;
    if (n < 512) {
        if (is_create && !inode->i_zone[7]) {
            inode->i_zone[7] = get_new_block();
        }
        ind = get_block(inode->i_zone[7]);
        if (is_create && !((unsigned short*)ind)[n])
            ((unsigned short*)ind)[n] = get_new_block();

        return *(((unsigned short*)ind) + n);
    }

    n -= 512;
    if (is_create && !inode->i_zone[8])
        inode->i_zone[8] = get_new_block();

    ind = get_block(inode->i_zone[8]);

    if (is_create && !((unsigned short*)ind)[n>>9])
        ((unsigned short*)ind)[n>>9] = get_new_block();

    ind = get_block(*(((unsigned short*)ind) + (n >> 9)));

    if (!(*(((unsigned short*)ind) + (n & 511)))) {
        *(((unsigned short*)ind) + (n & 511)) = get_new_block();
    }

    return *(((unsigned short*)ind) + (n & 511));
}

unsigned short FileSystem::bmap(struct d_inode* inode, int n) {
    return _bmap(inode, n, false);
}

unsigned short FileSystem::create_block(struct d_inode* inode, int n) {
    return _bmap(inode, n, true);
}

unsigned short FileSystem::get_new_block() {
    int i = 0, j = 0, k = 0;
    for (i = 0; i < ZMAPS; i++) {
        for (j = 0; j < BLOCK_SIZE; j++) {
            char bitmap = _zone_map[i * BLOCK_SIZE + j];
            if ((bitmap & 0xff) < 0xff) {
                for (k = 0; k < 8; k++) {
                    if (((1 << k) & bitmap) == 0) {
                        _zone_map[i*BLOCK_SIZE + j] |= (1 << k);
                        goto found;
                    }
                }
            }
        }
    }

found:
    return (unsigned short)(i * BLOCK_SIZE * 8 + j * 8 + k) + FIRSTZONE - 1;
}

struct d_inode* FileSystem::iget(int nr) {
    int inode_block = (nr - 1) / INODES_PER_BLOCK;
    int index_in_block = (nr - 1) % INODES_PER_BLOCK;

    return (struct d_inode*)(_images[2+IMAPS+ZMAPS+inode_block] +
        index_in_block * sizeof(struct d_inode));
}

void FileSystem::list_dir(char* dir_name) {
    m_inode* inode = namei(dir_name);
    if (!inode) {
        printf("Error: Can not directory %s\n", dir_name);
        return;
    }

    char* pwd_block = get_block(bmap(inode->p, 0));

    for (int i = 0; i < inode->p->i_size / sizeof(struct dir_entry); i++) {
        struct dir_entry* dir = ((struct dir_entry*)pwd_block) + i;
        printf("inode:%d, mode:%06o, name:%s\n", dir->inode, iget(dir->inode)->i_mode, dir->name);
    }

    delete inode;
}

void FileSystem::init_fs() {
    // create an empty dir, root.
    delete get_new_dir_inode(NULL);
}

FileSystem::~FileSystem() {
    release_images();
}

void FileSystem::setup_map() {
    _inode_map = _images[2];
    _zone_map  = _images[2 + IMAPS];
    memset(_inode_map,0xff, IMAPS * BLOCK_SIZE);
    memset(_zone_map, 0xff, ZMAPS * BLOCK_SIZE);

    for (int i = 0; i < INODE_BLOCKS; i++) {
        memset(_images[2+IMAPS+ZMAPS+i], 0, BLOCK_SIZE);
    }

    for (int i = FIRSTZONE ; i<ZONES ; i++)
        unmark_zone(i);

    for (int i = ROOT_INO ; i<INODES ; i++)
        unmark_inode(i);

    printf("%d inodes\n",INODES);
    printf("%d imaps\n", IMAPS);
    printf("%d blocks\n",ZONES);
    printf("%d zmaps\n", ZMAPS);
    printf("Firstdatazone=%d (%ld)\n",FIRSTZONE,NORM_FIRSTZONE);
    printf("Zonesize=%d\n",BLOCK_SIZE<<ZONESIZE);
    printf("Maxsize=%d\n\n",MAXSIZE);
}

void FileSystem::setup_super_block() {
    _super_blk = (super_block*)_images[1];
    memset(_super_blk,0, BLOCK_SIZE);

    ZONESIZE = 0;
    ZONES = BLOCKS;
    INODES = BLOCKS / 3;
    MAXSIZE = (7+512+512*512)*1024;
    MAGIC = SUPER_MAGIC;

    IMAPS = UPPER(INODES,BITS_PER_BLOCK);
    ZMAPS = UPPER(BLOCKS - NORM_FIRSTZONE,BITS_PER_BLOCK);
    FIRSTZONE = NORM_FIRSTZONE;
}

void FileSystem::write_image() {
    FILE* tfile = fopen(image_name, "w+");
    for (int i = 0; i < BLOCKS; i++) {
        fwrite(_images[i], sizeof(char), BLOCK_SIZE, tfile);
    }
    fclose(tfile);
}

m_inode* FileSystem::get_new_dir_inode(m_inode* parent) {
    int nr = find_empty_inode();

    d_inode* inode = iget(nr);
    inode->i_mode = I_DIRECTORY | 0777;
    inode->i_nlinks = 2;
    inode->i_size = 2 * sizeof(struct dir_entry);
    inode->i_zone[0] = get_new_block();
    dir_entry* de = (struct dir_entry*)_images[inode->i_zone[0]];
    de->inode = nr;
    strncpy(de->name, ".", NAME_LEN);

    de++;
    de->inode = parent ? parent->i_num : nr;
    strncpy(de->name, "..", NAME_LEN);

    return new m_inode(inode, nr);
}

int FileSystem::find_empty_inode() {
    int i = 0, j = 0, k = 0;
    for (i = 0; i < IMAPS; i++) {
        for (j = 0; j < BLOCK_SIZE; j++) {
            char bitmap = _inode_map[i * BLOCK_SIZE + j];
            if ((bitmap & 0xff) < 0xff) {
                for (k = 0; k < 8; k++) {
                    if (((1 << k) & bitmap) == 0) {
                        _inode_map[i*BLOCK_SIZE + j] |= (1 << k);
                        goto found;
                    }
                }
            }
        }
    }

found:
    return  i * BLOCK_SIZE * 8 + j * 8 + k;
}

void FileSystem::copy_to(FileSystem* tfs, char* sfile, char* tfile) {
    m_inode* smi = namei(sfile);
    char* parent_name = get_dir_name(tfile);
    m_inode* tmi = tfs->namei(tfile);
    if (!tmi) {
        tfs->make_dir(parent_name);
        tmi = tfs->create_new_file(tfile, smi->p->i_size);
    }

    if (tmi == NULL) {
        printf("Error: Can not create new file %s\n", tfile);
        return;
    }

    int block = 0;
    for (int i = 0; i < smi->p->i_size / BLOCK_SIZE; i++) {
        int index = bmap(smi->p, i);
        if (index == 0)
            continue;

        memcpy(tfs->get_block(tfs->create_block(tmi->p, block++)), 
                get_block(index), BLOCK_SIZE);
    }

    // some thing special in i_zone 0.
    if (smi->p->i_size == 0) {
        tmi->p->i_zone[0] = smi->p->i_zone[0];
    }

    tmi->p->i_size = smi->p->i_size;
    printf("isize is %d\n", tmi->p->i_size);
    tmi->p->i_mode = smi->p->i_mode;
    printf("imode is %d\n", tmi->p->i_mode);
    tmi->p->i_nlinks = smi->p->i_nlinks;
    printf("inlinks is %d\n", tmi->p->i_nlinks);
    tmi->p->i_time = smi->p->i_time;
    tmi->p->i_gid = smi->p->i_gid;
    tmi->p->i_uid = smi->p->i_uid;
}

m_inode* FileSystem::make_dir(char* dir_name) {
    m_inode* ndnode = namei(dir_name);
    if (ndnode) {
        return ndnode;
    }

    char* parent_name = get_dir_name(dir_name);
    m_inode* parent = namei(parent_name);

    // make sure parent's inode is not null.
    if (!parent) {
        parent = make_dir(parent_name);
        if (!parent) {
            printf("Error: can not make dir %s\n", parent_name);
            return NULL;
        }
    }

    char* dname = get_entry_name(dir_name);
    printf("mkdir: %s\n", dname);

    ndnode = get_new_dir_inode(parent);

    char* pwd_block = get_block(bmap(parent->p, 0));
    struct dir_entry* dir = (struct dir_entry*)pwd_block;

    int i = 0;
    for (i = 0; i < parent->p->i_size / sizeof(struct dir_entry); i++) {
        if (!dir->inode) {
            dir->inode = ndnode->i_num;
            strncpy(dir->name, dname, NAME_LEN);
            break;
        }

        if ((char*)dir > pwd_block + BLOCK_SIZE) {
            pwd_block = get_block(create_block(parent->p, i / DIR_ENTRIES_PER_BLOCK));
            dir = (struct dir_entry*)pwd_block;
        }
        
        dir++;
    }

    if (i * sizeof(struct dir_entry) == parent->p->i_size) {
        parent->p->i_size = (i + 1) * sizeof(struct dir_entry);
        dir->inode = ndnode->i_num;
        strncpy(dir->name, dname, NAME_LEN);
    }

    list_dir(parent_name);

    delete[] dname;
    delete parent;

    return ndnode;
}

m_inode* FileSystem::get_new_file_inode(long length) {
    int nr = find_empty_inode();
    d_inode* inode = iget(nr);
    inode->i_size = (unsigned int)length;
    for (int i = 0; i < UPPER(inode->i_size, BLOCK_SIZE); i++) {
        create_block(inode, i);
    }
    inode->i_mode = I_REGULAR | 0777;

    return new m_inode(inode, nr);
}

m_inode* FileSystem::create_new_file(char* dir_name, long length) {
    char* parent_name = get_dir_name(dir_name);
    m_inode* parent = namei(parent_name);
    if (!parent) {
        parent= make_dir(parent_name);
    }

    if (!parent) {
        printf("Error: can not create directory %s\n", parent_name);
        return NULL;
    }

    char* dname = get_entry_name(dir_name);
    printf("new file: %s\n", dname);

    m_inode* ndnode = get_new_file_inode(length);

    char* pwd_block = get_block(bmap(parent->p, 0));
    struct dir_entry* dir = (struct dir_entry*)pwd_block;

    int i = 0;
    for (i = 0; i < parent->p->i_size / sizeof(struct dir_entry); i++) {
        if (!dir->inode) {
            dir->inode = ndnode->i_num;
            strncpy(dir->name, dname, NAME_LEN);
            break;
        }

        if ((char*)dir > pwd_block + BLOCK_SIZE) {
            pwd_block = get_block(bmap(parent->p, i / DIR_ENTRIES_PER_BLOCK));
            dir = (struct dir_entry*)pwd_block;
        }
        
        dir++;
    }

    if (i * sizeof(struct dir_entry) == parent->p->i_size) {
        parent->p->i_size = (i + 1) * sizeof(struct dir_entry);
        dir->inode = ndnode->i_num;
        strncpy(dir->name, dname, NAME_LEN);
    }

    return ndnode;
}

int FileSystem::write_file(const char* src_file, char* dst_file) {
    FILE* srcf = fopen(src_file, "r");

    m_inode* finode = namei(dst_file);
    long length = file_length(srcf);
    if (!finode) {
        finode = create_new_file(dst_file, length);
    }

    finode->p->i_mode = I_REGULAR | 0777;
    finode->p->i_size = length;
    finode->p->i_uid = 0;
    finode->p->i_gid = 0;
    finode->p->i_nlinks = 1;

    int total = 0;
    for (int i = 0; i < UPPER(finode->p->i_size, BLOCK_SIZE); i++) {
        total += fread(get_block(create_block(finode->p, i)), sizeof(char), BLOCK_SIZE, srcf);
        printf("%d block write\n", bmap(finode->p, i));
    }
    printf("%d bytes read\n", total);

    write_image();
    fclose(srcf);
    return 0;
}

void FileSystem::extract_file(char* file_name) {
    m_inode* mi = namei(file_name);
    if (mi == NULL) {
        printf("Can not find file %s\n", file_name);
        return;
    }

    printf("file mode is %ud\n", mi->p->i_size);
    printf("file nlinks is %d\n", mi->p->i_nlinks);
    if (mi->p->i_size == 0) {
        printf("file %s is empty and first zone is %d\n", file_name, mi->p->i_zone[0]);
        return;
    }

    FILE* extract = fopen("extract_temp", "w+");
    for (int i = 0; i < UPPER(mi->p->i_size, BLOCK_SIZE); i++) {
        int index = bmap(mi->p, i);
        if (index == 0)
            continue;

        if (fwrite(_images[index], sizeof(char), BLOCK_SIZE, extract) != BLOCK_SIZE) {
            printf("Error: write failed in extacting file");
            fclose(extract);
            return;
        }
    }

    fclose(extract);
}

