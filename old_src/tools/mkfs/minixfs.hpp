#ifndef _MINIX_FS
#define _MINIX_FS

#include "mfs.h"
#include "common.hpp"

class FileSystem {
private:
    char* _images[BLOCKS];
    char* image_name;

    super_block*    _super_blk;
    char*           _inode_map;
    char*           _zone_map;

    d_inode*        _root;

    int load_image(bool is_create);
    void release_images();
    int read_block(FILE* f, int block);


    // bitmap
    int find_empty_inode();
    unsigned short get_new_block();

    // inode
    d_inode* iget(int nr);
    m_inode* find_entry(m_inode* pwd, char* name);

    unsigned short _bmap(struct d_inode* inode, int n, bool is_create);
    unsigned short bmap(struct d_inode* inode, int n);

    void setup_super_block();
    void setup_map();

public:
    FileSystem(char* img_name, bool is_create = false);
    ~FileSystem();

    void list_dir(char* dir_name);
    void init_fs();
    void write_image();
    void copy_to(FileSystem* tfs, char* sfile, char* tfile);

    m_inode* namei(char* dir_name);
    char * get_block(int nblock);
    unsigned short create_block(struct d_inode* inode, int n);
    m_inode* get_new_dir_inode(m_inode* parent);
    m_inode* get_new_file_inode(long length);
    m_inode* make_dir(char* dir_name);
    int write_file(const char* src_file, char* dst_file);
    m_inode* create_new_file(char* tgt_file, long length);
    void extract_file(char* filename);
};

#endif
