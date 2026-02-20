#include "ata.h"
#include "io.h"
#include "string.h"

/*
 * ATA PIO driver - reads from the FAT data disk.
 * Probes primary slave, secondary master/slave to find the data drive.
 */

#define ATA_REG_DATA       0x0
#define ATA_REG_ERROR      0x1
#define ATA_REG_SECCOUNT   0x2
#define ATA_REG_LBA_LO     0x3
#define ATA_REG_LBA_MID    0x4
#define ATA_REG_LBA_HI     0x5
#define ATA_REG_DRIVE      0x6
#define ATA_REG_STATUS     0x7
#define ATA_REG_COMMAND    0x7

#define ATA_STATUS_BSY     0x80
#define ATA_STATUS_DRQ     0x08
#define ATA_STATUS_ERR     0x01

#define ATA_CMD_READ       0x20
#define ATA_CMD_IDENTIFY   0xEC

static uint16_t ata_base = 0;
static uint8_t  ata_drive_sel = 0;  /* 0xE0=master, 0xF0=slave */
static bool     ata_present = false;

static void ata_wait_bsy_on(uint16_t base) {
    while (inb(base + ATA_REG_STATUS) & ATA_STATUS_BSY);
}

static bool ata_identify(uint16_t base, uint8_t drive_select) {
    outb(base + ATA_REG_DRIVE, drive_select);
    outb(base + ATA_REG_SECCOUNT, 0);
    outb(base + ATA_REG_LBA_LO, 0);
    outb(base + ATA_REG_LBA_MID, 0);
    outb(base + ATA_REG_LBA_HI, 0);
    outb(base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

    uint8_t status = inb(base + ATA_REG_STATUS);
    if (status == 0) return false;

    ata_wait_bsy_on(base);

    if (inb(base + ATA_REG_LBA_MID) != 0 || inb(base + ATA_REG_LBA_HI) != 0)
        return false;

    while (1) {
        status = inb(base + ATA_REG_STATUS);
        if (status & ATA_STATUS_ERR) return false;
        if (status & ATA_STATUS_DRQ) break;
    }

    for (int i = 0; i < 256; i++) inw(base + ATA_REG_DATA);
    return true;
}

bool ata_init(void) {
    /* Primary slave (index=1) — most common for QEMU second disk */
    if (ata_identify(ATA_PRIMARY_IO, 0xF0)) {
        ata_base = ATA_PRIMARY_IO;
        ata_drive_sel = 0xF0;
        ata_present = true;
        return true;
    }

    /* Secondary master (index=2) */
    if (ata_identify(ATA_SECONDARY_IO, 0xE0)) {
        ata_base = ATA_SECONDARY_IO;
        ata_drive_sel = 0xE0;
        ata_present = true;
        return true;
    }

    /* Secondary slave (index=3) */
    if (ata_identify(ATA_SECONDARY_IO, 0xF0)) {
        ata_base = ATA_SECONDARY_IO;
        ata_drive_sel = 0xF0;
        ata_present = true;
        return true;
    }

    ata_present = false;
    return false;
}

bool ata_read_sectors(uint32_t lba, uint8_t count, void *buffer) {
    if (!ata_present || count == 0) return false;

    uint16_t *buf = (uint16_t *)buffer;

    ata_wait_bsy_on(ata_base);

    outb(ata_base + ATA_REG_DRIVE, ata_drive_sel | ((lba >> 24) & 0x0F));
    outb(ata_base + ATA_REG_SECCOUNT, count);
    outb(ata_base + ATA_REG_LBA_LO, lba & 0xFF);
    outb(ata_base + ATA_REG_LBA_MID, (lba >> 8) & 0xFF);
    outb(ata_base + ATA_REG_LBA_HI, (lba >> 16) & 0xFF);
    outb(ata_base + ATA_REG_COMMAND, ATA_CMD_READ);

    for (int s = 0; s < count; s++) {
        ata_wait_bsy_on(ata_base);

        uint8_t status = inb(ata_base + ATA_REG_STATUS);
        if (status & ATA_STATUS_ERR) return false;

        while (!(inb(ata_base + ATA_REG_STATUS) & ATA_STATUS_DRQ));

        for (int i = 0; i < 256; i++) {
            buf[s * 256 + i] = inw(ata_base + ATA_REG_DATA);
        }
    }

    return true;
}

bool ata_secondary_present(void) {
    return ata_present;
}
