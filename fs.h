#ifndef FS_H
#define FS_H

#include "types.h"
#include "fat32.h"

#define FS_MAX_FDS 16
#define FS_MAX_NAME 13

typedef struct {
    fat32_entry_t entry;
    u32           offset;
    bool          in_use;
} fs_file_t;

bool fs_init(void);
s32  fs_open(const char *path);
s32  fs_read(s32 fd, u8 *buf, u32 len);
void fs_close(s32 fd);
bool fs_list(const char *path, char *name, u32 *size, bool *is_dir, u32 index);

#endif