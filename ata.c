#include "ata.h"
#include "types.h"

#define ATA_PRIMARY_DATA     0x1F0
#define ATA_PRIMARY_ERR      0x1F1
#define ATA_PRIMARY_SECCOUNT 0x1F2
#define ATA_PRIMARY_LBA_LO   0x1F3
#define ATA_PRIMARY_LBA_MID  0x1F4
#define ATA_PRIMARY_LBA_HI   0x1F5
#define ATA_PRIMARY_DRIVE    0x1F6
#define ATA_PRIMARY_STATUS   0x1F7
#define ATA_PRIMARY_CMD      0x1F7

#define ATA_CMD_READ         0x20
#define ATA_CMD_IDENTIFY     0xEC

#define ATA_SR_BSY  0x80
#define ATA_SR_DRDY 0x40
#define ATA_SR_DF   0x20
#define ATA_SR_ERR  0x01

static void ata_delay(void) {
    // 400ns delay on ancient hardware
    inb(ATA_PRIMARY_STATUS);
    inb(ATA_PRIMARY_STATUS);
    inb(ATA_PRIMARY_STATUS);
    inb(ATA_PRIMARY_STATUS);
}

static bool ata_wait_ready(void) {
    u8 status;
    // Wait for BSY to clear and DRDY to set
    for (u32 i = 0; i < 1000000; i++) {
        status = inb(ATA_PRIMARY_STATUS);
        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRDY)) {
            return true;
        }
        ata_delay();
    }
    return false;
}

static bool ata_wait_data(void) {
    u8 status;
    for (u32 i = 0; i < 1000000; i++) {
        status = inb(ATA_PRIMARY_STATUS);
        if (status & ATA_SR_DRDY) {
            if (!(status & ATA_SR_BSY)) return true;
        }
        if (status & ATA_SR_ERR) return false;
        ata_delay();
    }
    return false;
}

bool ata_init(void) {
    // Select master drive
    outb(ATA_PRIMARY_DRIVE, 0xA0);
    ata_delay();

    // Send IDENTIFY to see if anything is there
    outb(ATA_PRIMARY_SECCOUNT, 0);
    outb(ATA_PRIMARY_LBA_LO, 0);
    outb(ATA_PRIMARY_LBA_MID, 0);
    outb(ATA_PRIMARY_LBA_HI, 0);
    outb(ATA_PRIMARY_CMD, ATA_CMD_IDENTIFY);

    // Check status — if 0, no drive
    u8 status = inb(ATA_PRIMARY_STATUS);
    if (status == 0) return false;

    // Wait for busy to clear
    while (inb(ATA_PRIMARY_STATUS) & ATA_SR_BSY) {
        ata_delay();
    }

    // Check for ATAPI (would have 0x01 in lba_mid)
    if (inb(ATA_PRIMARY_LBA_MID) || inb(ATA_PRIMARY_LBA_HI)) {
        return false; // ATAPI device, skip for now
    }

    // Wait for DRDY
    for (u32 i = 0; i < 100000; i++) {
        if (inb(ATA_PRIMARY_STATUS) & ATA_SR_DRDY) return true;
        ata_delay();
    }

    return false;
}

bool ata_read_sector(u32 lba, u8 *buf) {
    if (!ata_wait_ready()) return false;

    // Send read command
    outb(ATA_PRIMARY_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_SECCOUNT, 1);
    outb(ATA_PRIMARY_LBA_LO, (lba & 0xFF));
    outb(ATA_PRIMARY_LBA_MID, (lba >> 8) & 0xFF);
    outb(ATA_PRIMARY_LBA_HI, (lba >> 16) & 0xFF);
    outb(ATA_PRIMARY_CMD, ATA_CMD_READ);

    if (!ata_wait_data()) return false;

    // Read 256 words (512 bytes)
    u16 *buf16 = (u16 *)buf;
    for (u32 i = 0; i < 256; i++) {
        buf16[i] = inw(ATA_PRIMARY_DATA);
        ata_delay();
    }

    return true;
}