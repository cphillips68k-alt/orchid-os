#ifndef RAMDISK_H
#define RAMDISK_H

#include "types.h"

bool ramdisk_init(void);
bool ramdisk_read_sector(u32 lba, u8 *buf);
bool ramdisk_has_disk(void);

#endif