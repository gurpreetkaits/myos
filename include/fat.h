#ifndef FAT_H
#define FAT_H

#include "types.h"

#define FAT_MAX_FILENAME 13  /* 8.3 + null */
#define FAT_ATTR_READONLY  0x01
#define FAT_ATTR_HIDDEN    0x02
#define FAT_ATTR_SYSTEM    0x04
#define FAT_ATTR_VOLUME_ID 0x08
#define FAT_ATTR_DIRECTORY 0x10
#define FAT_ATTR_ARCHIVE   0x20

typedef struct {
    char     name[FAT_MAX_FILENAME];
    uint32_t size;
    uint16_t first_cluster;
    uint8_t  attr;
} fat_dir_entry_t;

bool fat_init(void);
int  fat_list_root(fat_dir_entry_t *entries, int max_entries);
int  fat_read_file(const char *filename, void *buffer, uint32_t max_size);
bool fat_is_mounted(void);

#endif
