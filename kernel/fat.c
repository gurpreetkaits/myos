#include "fat.h"
#include "ata.h"
#include "string.h"

/*
 * FAT16 filesystem driver
 * Parses BPB, reads FAT table, navigates root directory, reads files.
 */

/* BIOS Parameter Block (at sector 0 of the FAT volume) */
typedef struct {
    uint8_t  jmp[3];
    char     oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
} __attribute__((packed)) bpb_t;

/* Directory entry (32 bytes) */
typedef struct {
    char     name[8];
    char     ext[3];
    uint8_t  attr;
    uint8_t  reserved;
    uint8_t  create_time_tenths;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t access_date;
    uint16_t first_cluster_hi;
    uint16_t modify_time;
    uint16_t modify_date;
    uint16_t first_cluster_lo;
    uint32_t file_size;
} __attribute__((packed)) fat16_dirent_t;

/* Cached filesystem info */
static bool mounted = false;
static uint32_t fat_start_lba;
static uint32_t root_dir_lba;
static uint32_t data_start_lba;
static uint16_t root_entry_count;
static uint8_t  sectors_per_cluster;
static uint16_t bytes_per_sector;
static uint16_t fat_size;

/* Sector buffer */
static uint8_t sector_buf[512];
static uint8_t fat_table_buf[512];

static void format_83_name(const fat16_dirent_t *entry, char *out) {
    int i, j = 0;

    /* Copy name, trim trailing spaces */
    for (i = 0; i < 8 && entry->name[i] != ' '; i++) {
        out[j++] = entry->name[i];
    }

    /* Add dot and extension if present */
    if (entry->ext[0] != ' ') {
        out[j++] = '.';
        for (i = 0; i < 3 && entry->ext[i] != ' '; i++) {
            out[j++] = entry->ext[i];
        }
    }

    out[j] = '\0';
}

/* Convert user filename to 8.3 format for comparison */
static void to_83_name(const char *filename, char *name83) {
    memset(name83, ' ', 11);
    int i = 0, j = 0;

    /* Copy name part (up to 8 chars) */
    while (filename[i] && filename[i] != '.' && j < 8) {
        name83[j++] = to_upper(filename[i++]);
    }

    /* Skip to extension */
    while (filename[i] && filename[i] != '.') i++;
    if (filename[i] == '.') {
        i++;
        j = 8;
        while (filename[i] && j < 11) {
            name83[j++] = to_upper(filename[i++]);
        }
    }
}

bool fat_init(void) {
    if (!ata_secondary_present()) return false;

    /* Read boot sector (BPB) */
    if (!ata_read_sectors(0, 1, sector_buf)) return false;

    bpb_t *bpb = (bpb_t *)sector_buf;

    /* Basic validation */
    if (bpb->bytes_per_sector != 512) return false;
    if (bpb->num_fats == 0) return false;

    bytes_per_sector    = bpb->bytes_per_sector;
    sectors_per_cluster = bpb->sectors_per_cluster;
    root_entry_count    = bpb->root_entry_count;
    fat_size            = bpb->fat_size_16;

    fat_start_lba  = bpb->reserved_sectors;
    root_dir_lba   = fat_start_lba + (bpb->num_fats * fat_size);

    uint32_t root_dir_sectors = ((root_entry_count * 32) + (bytes_per_sector - 1)) / bytes_per_sector;
    data_start_lba = root_dir_lba + root_dir_sectors;

    mounted = true;
    return true;
}

bool fat_is_mounted(void) {
    return mounted;
}

int fat_list_root(fat_dir_entry_t *entries, int max_entries) {
    if (!mounted) return -1;

    int count = 0;
    uint32_t root_sectors = (root_entry_count * 32 + 511) / 512;

    for (uint32_t s = 0; s < root_sectors && count < max_entries; s++) {
        if (!ata_read_sectors(root_dir_lba + s, 1, sector_buf)) break;

        fat16_dirent_t *de = (fat16_dirent_t *)sector_buf;
        int entries_per_sector = 512 / sizeof(fat16_dirent_t);

        for (int i = 0; i < entries_per_sector && count < max_entries; i++) {
            if (de[i].name[0] == 0x00) return count;      /* End of directory */
            if ((uint8_t)de[i].name[0] == 0xE5) continue;  /* Deleted entry */
            if (de[i].attr == FAT_ATTR_VOLUME_ID) continue; /* Volume label */
            if (de[i].attr & 0x0F) continue;  /* Skip LFN entries (attr=0x0F) */

            format_83_name(&de[i], entries[count].name);
            entries[count].size = de[i].file_size;
            entries[count].first_cluster = de[i].first_cluster_lo;
            entries[count].attr = de[i].attr;
            count++;
        }
    }

    return count;
}

static uint16_t fat_next_cluster(uint16_t cluster) {
    /* Each FAT16 entry is 2 bytes */
    uint32_t fat_offset = cluster * 2;
    uint32_t fat_sector = fat_start_lba + (fat_offset / 512);
    uint32_t entry_offset = fat_offset % 512;

    if (!ata_read_sectors(fat_sector, 1, fat_table_buf)) return 0xFFFF;

    uint16_t next = *(uint16_t *)&fat_table_buf[entry_offset];
    return next;
}

static uint32_t cluster_to_lba(uint16_t cluster) {
    return data_start_lba + (cluster - 2) * sectors_per_cluster;
}

int fat_read_file(const char *filename, void *buffer, uint32_t max_size) {
    if (!mounted) return -1;

    /* Find file in root directory */
    char name83[11];
    to_83_name(filename, name83);

    uint32_t root_sectors = (root_entry_count * 32 + 511) / 512;
    uint16_t first_cluster = 0;
    uint32_t file_size = 0;
    bool found = false;

    for (uint32_t s = 0; s < root_sectors; s++) {
        if (!ata_read_sectors(root_dir_lba + s, 1, sector_buf)) return -1;

        fat16_dirent_t *de = (fat16_dirent_t *)sector_buf;
        int entries_per_sector = 512 / sizeof(fat16_dirent_t);

        for (int i = 0; i < entries_per_sector; i++) {
            if (de[i].name[0] == 0x00) return -1;         /* End of directory */
            if ((uint8_t)de[i].name[0] == 0xE5) continue;  /* Deleted */
            if (de[i].attr & 0x08) continue;                /* Volume label */
            if (de[i].attr & 0x0F) continue;                /* LFN */

            if (memcmp(de[i].name, name83, 8) == 0 && memcmp(de[i].ext, name83 + 8, 3) == 0) {
                first_cluster = de[i].first_cluster_lo;
                file_size = de[i].file_size;
                found = true;
                break;
            }
        }
        if (found) break;
    }

    if (!found) return -1;

    /* Read file data following the cluster chain */
    uint8_t *buf = (uint8_t *)buffer;
    uint32_t bytes_read = 0;
    uint16_t cluster = first_cluster;

    while (cluster >= 2 && cluster < 0xFFF8 && bytes_read < file_size) {
        uint32_t lba = cluster_to_lba(cluster);

        for (int s = 0; s < sectors_per_cluster && bytes_read < file_size; s++) {
            if (!ata_read_sectors(lba + s, 1, sector_buf)) return bytes_read;

            uint32_t to_copy = 512;
            if (bytes_read + to_copy > file_size) to_copy = file_size - bytes_read;
            if (bytes_read + to_copy > max_size) to_copy = max_size - bytes_read;

            memcpy(buf + bytes_read, sector_buf, to_copy);
            bytes_read += to_copy;

            if (bytes_read >= max_size) return bytes_read;
        }

        cluster = fat_next_cluster(cluster);
    }

    return bytes_read;
}
