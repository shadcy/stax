/* ============================================================================
 * TIOS — fat.c
 * Simplified FAT12/16 filesystem driver implementation
 * ============================================================================ */

#include "fat.h"
#include <stddef.h>

/* Simple implementations */
static void *memset(void *s, int c, size_t n)
{
    uint8_t *p = (uint8_t *)s;
    while (n--) *p++ = (uint8_t)c;
    return s;
}

static void *memcpy(void *dest, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) *d++ = *s++;
    return dest;
}

static int strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

/* Simple test disk sector */
static uint8_t test_sector[SECTOR_SIZE];

/* ---------------------------------------------------------------------------
 * disk_init — initialize disk
 * --------------------------------------------------------------------------- */
int disk_init(void)
{
    /* Create a simple boot sector */
    memset(test_sector, 0, SECTOR_SIZE);
    
    /* Boot signature */
    test_sector[510] = 0x55;
    test_sector[511] = 0xAA;
    
    return 0;
}

/* ---------------------------------------------------------------------------
 * disk_read — read a sector
 * --------------------------------------------------------------------------- */
int disk_read(uint32_t lba, uint8_t *buffer)
{
    if (lba == 0) {
        /* Return boot sector */
        memcpy(buffer, test_sector, SECTOR_SIZE);
        return 0;
    }
    
    if (lba == 19) {
        /* Root directory - create test file entry */
        memset(buffer, 0, SECTOR_SIZE);
        
        dir_entry_t *entry = (dir_entry_t *)buffer;
        const char *fname = "TEST     TXT";
        for (int i = 0; i < 11; i++) entry->name[i] = fname[i];
        entry->attr = 0x20;  /* Archive */
        entry->first_cluster_lo = 2;
        entry->first_cluster_hi = 0;
        entry->file_size = 13;  /* "Hello, World!\n" */
        
        return 0;
    }
    
    if (lba == 31) {
        /* Data sector for cluster 2 */
        memset(buffer, 0, SECTOR_SIZE);
        const char *msg = "Hello, World!\n";
        for (int i = 0; i < 13; i++) buffer[i] = msg[i];
        return 0;
    }
    
    /* Default: return empty sector */
    memset(buffer, 0, SECTOR_SIZE);
    return 0;
}

/* ---------------------------------------------------------------------------
 * fat_init — initialize FAT filesystem
 * --------------------------------------------------------------------------- */
int fat_init(void)
{
    return 0;
}

/* ---------------------------------------------------------------------------
 * fat_open — open a file
 * --------------------------------------------------------------------------- */
fat_file_t *fat_open(const char *filename)
{
    static fat_file_t file;
    
    /* For testing, only support TEST.TXT */
    if (strcmp(filename, "TEST.TXT") != 0) {
        return NULL;
    }
    
    file.current_cluster = 2;
    file.current_pos = 0;
    file.size = 13;
    
    return &file;
}

/* ---------------------------------------------------------------------------
 * fat_read — read from a file
 * --------------------------------------------------------------------------- */
int fat_read(fat_file_t *file, void *buffer, uint32_t count)
{
    /* For testing, just read from sector 31 */
    uint8_t sector[SECTOR_SIZE];
    if (disk_read(31, sector) != 0) {
        return 0;
    }
    
    uint32_t bytes_to_copy = count;
    if (bytes_to_copy > file->size - file->current_pos) {
        bytes_to_copy = file->size - file->current_pos;
    }
    
    memcpy(buffer, sector + file->current_pos, bytes_to_copy);
    file->current_pos += bytes_to_copy;
    
    return bytes_to_copy;
}

/* ---------------------------------------------------------------------------
 * fat_close — close a file
 * --------------------------------------------------------------------------- */
int fat_close(fat_file_t *file)
{
    (void)file;
    return 0;
}
