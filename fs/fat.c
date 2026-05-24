/* ============================================================================
 * TIOS — fat.c
 * FAT filesystem compatibility wrapper over FatFs
 * ============================================================================ */

#include "fat.h"
#include "console.h"
#include "heap.h"

static FATFS fs;

int fat_init(void)
{
    FRESULT res = f_mount(&fs, "", 1);
    if (res != FR_OK) {
        kputs("[FAT] f_mount failed\n");
        return -1;
    }
    return 0;
}

fat_file_t *fat_open(const char *filename)
{
    fat_file_t *file = (fat_file_t *)kmalloc(sizeof(fat_file_t));
    if (!file) return NULL;
    
    FRESULT res = f_open(&file->f, filename, FA_READ);
    if (res != FR_OK) {
        kfree(file);
        return NULL;
    }
    return file;
}

int fat_read(fat_file_t *file, void *buffer, uint32_t count)
{
    if (!file) return 0;
    UINT br;
    FRESULT res = f_read(&file->f, buffer, count, &br);
    if (res != FR_OK) return 0;
    return (int)br;
}

int fat_close(fat_file_t *file)
{
    if (!file) return -1;
    f_close(&file->f);
    kfree(file);
    return 0;
}

int fat_seek(fat_file_t *file, int offset)
{
    if (!file) return -1;
    FRESULT res = f_lseek(&file->f, offset);
    if (res != FR_OK) return -1;
    return 0;
}

uint32_t fat_file_size(fat_file_t *file)
{
    if (!file) return 0;
    return (uint32_t)f_size(&file->f);
}

void fat_list_root(void)
{
    /* Handled by fs command in command.c */
}
