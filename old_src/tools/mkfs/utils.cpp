#include "mfs.h"

#include <cstring>

long file_length(FILE* f) {
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, 0);
    return file_size;
}

void free_strings(char** dirs) {
    char** dir_list = dirs;
    while (*dir_list) {
        delete[] (*dir_list);
        dir_list++;
    }

    delete[] dirs;
}

char** split(const char* name) {
    const char* p = name;
    int cnt = 0;
    while (*p) {
        if (*p == '/') {
            cnt += 1;
        }
        p++;
    }
    char** array = new char*[cnt+1];
    for (int i = 0; i < cnt + 1; i++) {
        array[i] = NULL;
    }

    p = name + 1;
    int j = 0, i = 0;
    while (*p) {
        if (*p == '/') {
            array[j][i++] = '\0';
            j++;
            i = 0;
        }
        else {
            if (array[j] == NULL) {
                array[j] = new char[NAME_LEN + 1];
            }
            array[j][i++] = *p;
        }
        p++;
    }
    if (array[j])
        array[j][i] = '\0';

    return array;
}


char* get_dir_name(char* full_name) {
    char* p = full_name;
    int len = strlen(full_name);
    int dir_len = 0;

    if (full_name[len-1] == '/') {
        full_name[len-1] = '\0';
        len--;
    }

    p = full_name + len - 1;

    while (p >= full_name) {
        if (*p == '/') {
            dir_len = p - full_name;
            break;
        }
        p--;
    }

    char * d = NULL;
    if (dir_len == 0) {
        d = new char[2];
        d[0] = '/';
        d[1] = '\0';
        return d;
    }

    d = new char[dir_len + 1];
    strncpy(d, full_name, dir_len);
    d[dir_len] = '\0';

    return d;
}

char* get_entry_name(char* full_name) {
    char* p = full_name;
    int len = strlen(full_name);
    int dir_len = 0;

    if (full_name[len-1] == '/') {
        full_name[len-1] = '\0';
        len--;
    }

    p = full_name + len - 1;

    while (p >= full_name) {
        if (*p == '/') {
            dir_len = len - (p - full_name);
            break;
        }
        p--;
    }

    char * d = NULL;
    d = new char[dir_len + 1];
    strncpy(d, p + 1, dir_len);
    d[dir_len] = '\0';

    return d;
}

