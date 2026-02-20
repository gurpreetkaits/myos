#ifndef ATA_H
#define ATA_H

#include "types.h"

/* ATA PIO ports (primary bus) */
#define ATA_PRIMARY_IO   0x1F0
#define ATA_PRIMARY_CTRL 0x3F6

/* ATA PIO ports (secondary bus - used for second disk) */
#define ATA_SECONDARY_IO   0x170
#define ATA_SECONDARY_CTRL 0x376

bool ata_init(void);
bool ata_read_sectors(uint32_t lba, uint8_t count, void *buffer);
bool ata_secondary_present(void);

#endif
