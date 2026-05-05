#include "fat32.h"
#include "ata.h"
#include "lib.h"

// FAT32 BPB offsets
#define FAT32_BPB_BYTES_PER_SECTOR    0x0B
#define FAT32_BPB_SECTORS_PER_CLUSTER 0x0D
#define FAT32_BPB_RESERVED_SECTORS    0x0E
#define FAT32_BPB_NUM_FATS            0x10
#define FAT32_BPB_SECTORS_PER_FAT     0x24
#define FAT32_BPB_ROOT_CLUSTER        0x2C

// Directory entry structure
#define FAT32_DIR_NAME       0x00  // 11 bytes
#define FAT32_DIR_ATTR       0x0B
#define FAT32_DIR_CLUSTER_HI 0x14
#define FAT32_DIR_CLUSTER_LO 0x1A
#define FAT32_DIR_SIZE       0x1C

#define FAT32_ATTR_DIRECTORY 0x10
#define FAT32_ATTR_LFN       0x0F
#define FAT32_ENTRY_SIZE     32
#define FAT32_EOC            0x0FFFFFF8

static u32 fat_base;
static u32 data_base;
static u32 sectors_per_cluster;
static u32 bytes_per_sector;
static u32 root_cluster;
static u8  sector_buf[512];

static u32 fat_read_entry(u32 cluster) {
    u32 fat_offset = cluster * 4;
    u32 fat_sector = fat_base + (fat_offset / bytes_per_sector);
    u32 entry_offset = fat_offset % bytes_per_sector;

    ata_read_sector(fat_sector, sector_buf);
    return *(u32 *)(sector_buf + entry_offset) & 0x0FFFFFFF;
}

static u32 cluster_to_sector(u32 cluster) {
    return data_base + (cluster - 2) * sectors_per_cluster;
}

static void parse_short_name(u8 *entry, char *name) {
    s32 i, j;
    // 8 character name
    for (i = 0; i < 8; i++) {
        if (entry[FAT32_DIR_NAME + i] == ' ') break;
        name[i] = entry[FAT32_DIR_NAME + i];
    }
    // Extension
    if (entry[FAT32_DIR_NAME + 8] != ' ') {
        name[i++] = '.';
        for (j = 0; j < 3; j++) {
            if (entry[FAT32_DIR_NAME + 8 + j] == ' ') break;
            name[i++] = entry[FAT32_DIR_NAME + 8 + j];
        }
    }
    name[i] = '\0';
}

bool fat32_init(void) {
    // Read boot sector (sector 0)
    if (!ata_read_sector(0, sector_buf)) return false;

    bytes_per_sector = *(u16 *)(sector_buf + FAT32_BPB_BYTES_PER_SECTOR);
    sectors_per_cluster = sector_buf[FAT32_BPB_SECTORS_PER_CLUSTER];
    u16 reserved_sectors = *(u16 *)(sector_buf + FAT32_BPB_RESERVED_SECTORS);
    u8  num_fats = sector_buf[FAT32_BPB_NUM_FATS];
    u32 sectors_per_fat = *(u32 *)(sector_buf + FAT32_BPB_SECTORS_PER_FAT);

    fat_base = reserved_sectors;
    data_base = fat_base + (num_fats * sectors_per_fat);
    root_cluster = *(u32 *)(sector_buf + FAT32_BPB_ROOT_CLUSTER);

    if (bytes_per_sector != 512) return false;

    return true;
}

bool fat32_lookup(const char *path, fat32_entry_t *entry) {
    if (path[0] != '/') return false;
    if (path[1] == '\0') {
        // Root directory
        entry->first_cluster = root_cluster;
        entry->size = 0;
        entry->is_dir = 1;
        return true;
    }

    u32 current_cluster = root_cluster;
    u8  dir_sector[512];

    // Skip leading slash, parse each component
    const char *p = path + 1;
    
    while (*p) {
        // Extract next path component
        char component[13]; // 8.3 + null
        s32 ci = 0;
        while (*p && *p != '/') {
            if (ci < 12) component[ci++] = *p;
            p++;
        }
        component[ci] = '\0';
        if (*p == '/') p++;

        // Search current directory for this component
        bool found = false;
        u32 dir_offset = 0;
        
        while (1) {
            u32 sector = cluster_to_sector(current_cluster) + dir_offset / 512;
            ata_read_sector(sector, dir_sector);
            
            u8 *ent = dir_sector + (dir_offset % 512);
            
            if (ent[0] == 0) break; // End of directory
            if (ent[0] == 0xE5) { dir_offset += 32; continue; } // Deleted
            if (ent[FAT32_DIR_ATTR] == FAT32_ATTR_LFN) { dir_offset += 32; continue; } // Long filename

            char entry_name[13];
            parse_short_name(ent, entry_name);

            // Case-insensitive compare
            if (strcmp_ci(component, entry_name) == 0) {
                u32 cluster = *(u16 *)(ent + FAT32_DIR_CLUSTER_LO) |
                              (*(u16 *)(ent + FAT32_DIR_CLUSTER_HI) << 16);
                u32 size = *(u32 *)(ent + FAT32_DIR_SIZE);
                u8  is_dir = (ent[FAT32_DIR_ATTR] & FAT32_ATTR_DIRECTORY) ? 1 : 0;

                if (*p == '\0') {
                    // Final component
                    entry->first_cluster = cluster;
                    entry->size = size;
                    entry->is_dir = is_dir;
                    return true;
                }

                // Continue deeper
                current_cluster = cluster;
                found = true;
                break;
            }

            dir_offset += 32;

            // Check if we need to move to next cluster
            if (dir_offset >= sectors_per_cluster * 512) {
                u32 next = fat_read_entry(current_cluster);
                if (next >= FAT32_EOC) break;
                current_cluster = next;
                dir_offset = 0;
            }
        }

        if (!found) return false;
    }

    return false;
}

s32 fat32_read(const fat32_entry_t *entry, u32 offset, u8 *buf, u32 len) {
    if (offset >= entry->size) return 0;
    if (offset + len > entry->size) len = entry->size - offset;

    u32 cluster_size = sectors_per_cluster * 512;
    u32 current_cluster = entry->first_cluster;
    u32 bytes_read = 0;

    // Skip to starting cluster
    while (offset >= cluster_size) {
        current_cluster = fat_read_entry(current_cluster);
        if (current_cluster >= FAT32_EOC) return 0;
        offset -= cluster_size;
    }

    u32 cluster_offset = offset;
    u8  cluster_buf[4096]; // Assume max 4K cluster for now

    while (bytes_read < len) {
        u32 sector_in_cluster = cluster_offset / 512;
        u32 byte_in_sector = cluster_offset % 512;
        u32 to_read = len - bytes_read;
        if (to_read > cluster_size - cluster_offset) to_read = cluster_size - cluster_offset;

        u32 lba = cluster_to_sector(current_cluster) + sector_in_cluster;

        // Simple: read sector by sector
        for (u32 i = 0; i <= to_read / 512 && bytes_read < len; i++) {
            ata_read_sector(lba + i, sector_buf);
            
            u32 chunk = len - bytes_read;
            u32 start = (i == 0) ? byte_in_sector : 0;
            if (chunk > 512 - start) chunk = 512 - start;

            memcpy(buf + bytes_read, sector_buf + start, chunk);
            bytes_read += chunk;
        }

        cluster_offset = 0;
        
        // Next cluster if needed
        if (bytes_read < len) {
            current_cluster = fat_read_entry(current_cluster);
            if (current_cluster >= FAT32_EOC) break;
        }
    }

    return bytes_read;
}

bool fat32_list_dir(const fat32_entry_t *dir, char *name_buf, u32 *size, bool *is_dir, u32 index) {
    if (!dir->is_dir) return false;

    u32 current_cluster = dir->first_cluster;
    u32 entry_offset = 0;
    u32 entry_count = 0;
    u8  dir_sector[512];

    while (1) {
        u32 lba = cluster_to_sector(current_cluster) + entry_offset / 512;
        ata_read_sector(lba, dir_sector);

        u8 *ent = dir_sector + (entry_offset % 512);

        if (ent[0] == 0) return false; // End
        if (ent[0] == 0xE5 || ent[FAT32_DIR_ATTR] == FAT32_ATTR_LFN) {
            entry_offset += 32;
            if (entry_offset >= sectors_per_cluster * 512) {
                u32 next = fat_read_entry(current_cluster);
                if (next >= FAT32_EOC) return false;
                current_cluster = next;
                entry_offset = 0;
            }
            continue;
        }

        if (entry_count == index) {
            parse_short_name(ent, name_buf);
            *size = *(u32 *)(ent + FAT32_DIR_SIZE);
            *is_dir = (ent[FAT32_DIR_ATTR] & FAT32_ATTR_DIRECTORY) != 0;
            return true;
        }

        entry_count++;
        entry_offset += 32;
        if (entry_offset >= sectors_per_cluster * 512) {
            u32 next = fat_read_entry(current_cluster);
            if (next >= FAT32_EOC) return false;
            current_cluster = next;
            entry_offset = 0;
        }
    }
}