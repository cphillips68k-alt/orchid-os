#include "fs.h"
#include "fat32.h"

static fs_file_t fd_table[FS_MAX_FDS];

bool fs_init(void) {
    for (s32 i = 0; i < FS_MAX_FDS; i++) {
        fd_table[i].in_use = false;
    }
    return fat32_init();
}

s32 fs_open(const char *path) {
    fat32_entry_t entry;
    if (!fat32_lookup(path, &entry)) return -1;

    for (s32 i = 0; i < FS_MAX_FDS; i++) {
        if (!fd_table[i].in_use) {
            fd_table[i].entry = entry;
            fd_table[i].offset = 0;
            fd_table[i].in_use = true;
            return i;
        }
    }
    return -1; // No free FDs
}

s32 fs_read(s32 fd, u8 *buf, u32 len) {
    if (fd < 0 || fd >= FS_MAX_FDS || !fd_table[fd].in_use) return -1;
    
    s32 bytes = fat32_read(&fd_table[fd].entry, fd_table[fd].offset, buf, len);
    if (bytes > 0) fd_table[fd].offset += bytes;
    return bytes;
}

void fs_close(s32 fd) {
    if (fd >= 0 && fd < FS_MAX_FDS) {
        fd_table[fd].in_use = false;
    }
}

bool fs_list(const char *path, char *name, u32 *size, bool *is_dir, u32 index) {
    fat32_entry_t entry;
    if (!fat32_lookup(path, &entry)) return false;
    return fat32_list_dir(&entry, name, size, is_dir, index);
}