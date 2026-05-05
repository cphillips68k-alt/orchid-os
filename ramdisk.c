#include "ramdisk.h"
#include "types.h"
#include "lib.h"

// Minimal FAT32 ramdisk image (512 bytes per sector, 1 sector = boot sector)
// Just enough to have a root directory with HELLO.TXT
static u8 ramdisk_data[16384] __attribute__((aligned(4))); // 32 sectors

static bool ramdisk_present = false;

bool ramdisk_init(void) {
    // In a real build, you'd load this from a file or embed it
    // For now, we'll stub it — no disk present
    ramdisk_present = false;
    return true;
}

bool ramdisk_read_sector(u32 lba, u8 *buf) {
    if (!ramdisk_present) return false;
    u32 offset = lba * 512;
    if (offset + 512 > sizeof(ramdisk_data)) return false;
    memcpy(buf, ramdisk_data + offset, 512);
    return true;
}

bool ramdisk_has_disk(void) {
    return ramdisk_present;
}