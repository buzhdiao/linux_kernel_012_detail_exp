#include <stdio.h>
#include <memory.h>
#include <malloc.h>
#include <assert.h>
#include <iostream>

using namespace std;

#include "minixfs.hpp"

// Read an image file.
FILE* read_file() {
    return NULL;
}

// Copy a file into root image.
int write_file(const char* src_file, char* dst_img, char* dst_file) {
    cout << src_file << " " << dst_img << " " << dst_file << endl;
    FileSystem* fs = new FileSystem(dst_img);
    fs->write_file(src_file, dst_file);
    delete fs;
    return 0;
}

void init_fs(char* src_img) {
    FileSystem* fs = new FileSystem(src_img, true);
    fs->init_fs();
    fs->write_image();
    delete fs;
}

void extract_file(char* img_file, char* filename) {
    FileSystem* fs = new FileSystem(img_file);
    fs->extract_file(filename);
    delete fs;
}

int copy_file(char* src_img, char* tgt_img, char* src_file, char* tgt_file) {
    FileSystem *sfs = new FileSystem(src_img);
    FileSystem *tfs = new FileSystem(tgt_img);
    sfs->copy_to(tfs, src_file, tgt_file);
    tfs->write_image();

    delete sfs;
    delete tfs;

    return 0;
}

void usage() {
}

#define LENGTH 32

int main(int argc, char** argv) {
    char cmd[LENGTH];
    char src_img[LENGTH];
    char dst_img[LENGTH];
    char src_file[LENGTH];
    char dst_file[LENGTH];

    usage();
    while (true) {
        scanf("%s", cmd);
        if (strcmp("exit", cmd) == 0) {
            break;
        }
        else if (strcmp("copy", cmd) == 0 || strcmp("cp", cmd) == 0) {
            printf("input source image file:");
            scanf("%s", src_img);
            printf("input target image file:");
            scanf("%s", dst_img);
            printf("input source file:");
            scanf("%s", src_file);
            printf("input target file:");
            scanf("%s", dst_file);

            copy_file(src_img, dst_img, src_file, dst_file);
        }
        else if (strcmp("list", cmd) == 0 || strcmp("ls", cmd) == 0) {
            printf("input source image file:");
            scanf("%s", src_img);
            printf("input directory need to show:");
            scanf("%s", src_file);
            FileSystem* fs = new FileSystem(src_img);
            fs->list_dir(src_file);
            delete fs;
        }
        else if (strcmp("init", cmd) == 0) {
            printf("input source image file:");
            scanf("%s", src_img);
            init_fs(src_img);
        }
        else if (strcmp("put", cmd) == 0) {
            printf("input source local file:");
            scanf("%s", src_file);
            printf("input target image file:");
            scanf("%s", dst_img);
            printf("input target file:");
            scanf("%s", dst_file);
            write_file(src_file, dst_img, dst_file);
        }
        else if (strcmp("get", cmd) == 0) {
            printf("input target image file:");
            scanf("%s", dst_img);
            printf("input target file:");
            scanf("%s", dst_file);
            extract_file(dst_img, dst_file);
        }
    }
}

