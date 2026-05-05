#ifndef FAT32_H
#define FAT32_H

#include "types.h"

#define FAT32_MAX_PATH 256
#define FAT32_MAX_FILE_SIZE (4ULL * 1024 * 1024 * 1024 - 1)

typedef struct {
    u32 first_cluster;
    u32 size;
    u8  is_dir;
} fat32_entry_t;

bool fat32_init(void);
bool fat32_lookup(const char *path, fat32_entry_t *entry);
s32  fat32_read(const fat32_entry_t *entry, u32 offset, u8 *buf, u32 len);
bool fat32_list_dir(const fat32_entry_t *dir, char *name_buf, u32 *size, bool *is_dir, u32 index);

#endif