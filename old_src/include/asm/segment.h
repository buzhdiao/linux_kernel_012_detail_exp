#ifndef _SEGMENT_H
#define _SEGMENT_H

void put_fs_byte(char val,char *addr);
char get_fs_byte(const char * addr);
void put_fs_long(unsigned long val,unsigned long * addr);
unsigned long get_fs_long(const unsigned long *addr);

unsigned long get_fs();
unsigned long get_ds();
void set_fs(unsigned long val);

#endif

