/* ============================================================================
 * TIOS — fat.h
 * FAT12/16 filesystem driver for Phase 6e
 * ============================================================================ */

#ifndef FAT_H
#define FAT_H

#include <stdint.h>

/* Disk sector size */
#define SECTOR_SIZE 512

/* FAT filesystem constants */
#define FAT12_MAX_CLUSTERS 4085
#define FAT16_MAX_CLUSTERS 65525

/* FAT entry values */
#define FAT_FREE       0x000
#define FAT_BAD        0xFF7
#define FAT_EOF_12     0xFFF
#define FAT_EOF_16     0xFFFF
#define FAT_EOF_32     0x0FFFFFFF

/* Directory entry structure */
typedef struct {
    uint8_t name[11];
    uint8_t attr;
    uint8_t nt_res;
    uint8_t create_time_tenth;
    uint8_t create_time[2];
    uint8_t create_date[2];
    uint8_t last_access_date[2];
    uint16_t first_cluster_hi;
    uint8_t write_time[2];
    uint8_t write_date[2];
    uint16_t first_cluster_lo;
    uint32_t file_size;
} __attribute__((packed)) dir_entry_t;

/* FAT attribute bits */
#define FAT_ATTR_READ_ONLY  0x01
#define FAT_ATTR_HIDDEN     0x02
#define FAT_ATTR_SYSTEM     0x04
#define FAT_ATTR_VOLUME_ID  0x08
#define FAT_ATTR_DIRECTORY  0x10
#define FAT_ATTR_ARCHIVE    0x20
#define FAT_ATTR_LONG_NAME  (FAT_ATTR_READ_ONLY | FAT_ATTR_HIDDEN | FAT_ATTR_SYSTEM | FAT_ATTR_VOLUME_ID)

/* FAT filesystem info */
typedef struct {
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_ver;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot;
    
    /* Calculated values */
    uint16_t fat_type;
    uint32_t fat_start;
    uint32_t data_start;
    uint32_t root_start;
    uint32_t root_size;
    uint32_t clusters;
} fat_info_t;

/* File handle structure */
typedef struct {
    dir_entry_t dir_entry;
    uint32_t current_cluster;
    uint32_t current_pos;
    uint32_t size;
} fat_file_t;

/* Disk interface */
int disk_read(uint32_t lba, uint8_t *buffer);

/* FAT filesystem operations */
int fat_init(void);
fat_file_t *fat_open(const char *filename);
int fat_read(fat_file_t *file, void *buffer, uint32_t count);
int fat_close(fat_file_t *file);
int fat_seek(fat_file_t *file, int offset);
uint32_t fat_file_size(fat_file_t *file);
void fat_list_root(void);

#endif /* FAT_H */
