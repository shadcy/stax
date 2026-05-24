/* ============================================================================
 * TIOS — fat.h
 * FAT filesystem compatibility wrapper over FatFs
 * ============================================================================ */

#ifndef FAT_H
#define FAT_H

#include <stdint.h>
#include "fatfs/ff.h"

/* Disk interface */
int pl181_disk_read(uint32_t lba, uint8_t *buffer);
int pl181_disk_write(uint32_t lba, const uint8_t *buffer);

/* Wrapper file structure */
typedef struct {
    FIL f;
} fat_file_t;

/* FAT filesystem operations */
int fat_init(void);
fat_file_t *fat_open(const char *filename);
int fat_read(fat_file_t *file, void *buffer, uint32_t count);
int fat_close(fat_file_t *file);
int fat_seek(fat_file_t *file, int offset);
uint32_t fat_file_size(fat_file_t *file);
void fat_list_root(void);

#endif /* FAT_H */
