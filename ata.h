#ifndef ATA_H
#define ATA_H

#include "types.h"

#define ATA_SECTOR_SIZE 512

bool ata_init(void);
bool ata_read_sector(u32 lba, u8 *buf);

#endif