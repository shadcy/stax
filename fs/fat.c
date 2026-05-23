/* ============================================================================
 * TIOS — fat_real.c
 * Real FAT16 filesystem driver backed by PL181 disk I/O
 *
 * Provides fat_open(), fat_read(), fat_close(), fat_file_size()
 * These replace the stub implementations in fat.c
 * ============================================================================ */

#include "fat.h"
#include <stdint.h>
#include <stddef.h>

/* ---- Minimal helpers ---- */
static void *r_memset(void *s, int c, unsigned n) {
    uint8_t *p = (uint8_t*)s; while (n--) *p++ = (uint8_t)c; return s;
}
static void *r_memcpy(void *d, const void *s, unsigned n) {
    uint8_t *dst=(uint8_t*)d; const uint8_t *src=(const uint8_t*)s;
    while (n--) *dst++=*src++; return d;
}
static int r_toupper(int c) { if(c>='a'&&c<='z') return c-32; return c; }
static int r_strncasecmp(const char *a, const char *b, int n) {
    while (n--) {
        int ca=r_toupper((unsigned char)*a);
        int cb=r_toupper((unsigned char)*b);
        if(ca!=cb) return ca-cb; if(!ca) return 0; a++; b++;
    }
    return 0;
}

/* ---- Sector cache (one sector) ---- */
#define SECTOR_SZ   512
static uint8_t  sec_cache[SECTOR_SZ];
static uint32_t sec_cached = 0xFFFFFFFFu;

static int read_sector(uint32_t lba, uint8_t *buf)
{
    if (lba == sec_cached) {
        r_memcpy(buf, sec_cache, SECTOR_SZ);
        return 0;
    }
    int rc = disk_read(lba, buf);
    if (rc == 0) {
        r_memcpy(sec_cache, buf, SECTOR_SZ);
        sec_cached = lba;
    }
    return rc;
}

/* ---- BPB (Boot Parameter Block) values parsed once ---- */
static struct {
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint32_t fat_start_lba;      /* LBA of FAT #0 */
    uint32_t root_dir_lba;       /* LBA of root directory (FAT16) */
    uint32_t data_start_lba;     /* LBA of first data cluster */
    uint32_t fat_size_sectors;
    int      initialized;
} bpb;

static int fat_parse_bpb(void)
{
    uint8_t buf[SECTOR_SZ];
    if (read_sector(0, buf) != 0) { kputs("[FAT] read_sector(0) failed\n"); return -1; }

    /* Check signature */
    if (buf[510] != 0x55 || buf[511] != 0xAA) { kputs("[FAT] bad signature\n"); return -1; }

    bpb.bytes_per_sector   = (uint16_t)(buf[11] | (buf[12] << 8));
    bpb.sectors_per_cluster = buf[13];
    bpb.reserved_sectors    = (uint16_t)(buf[14] | (buf[15] << 8));
    bpb.num_fats            = buf[16];
    bpb.root_entry_count    = (uint16_t)(buf[17] | (buf[18] << 8));

    uint16_t fat_size_16 = (uint16_t)(buf[22] | (buf[23] << 8));
    uint32_t fat_size_32 = (uint32_t)(buf[36]|(buf[37]<<8)|(buf[38]<<16)|(buf[39]<<24));
    bpb.fat_size_sectors = fat_size_16 ? fat_size_16 : fat_size_32;

    bpb.fat_start_lba  = bpb.reserved_sectors;
    bpb.root_dir_lba   = bpb.fat_start_lba + bpb.num_fats * bpb.fat_size_sectors;

    uint32_t root_dir_sectors = ((uint32_t)bpb.root_entry_count * 32 + SECTOR_SZ - 1) / SECTOR_SZ;
    bpb.data_start_lba = bpb.root_dir_lba + root_dir_sectors;

    bpb.initialized = 1;
    return 0;
}

/* ---- Get next FAT16 cluster ---- */
static uint32_t fat_next_cluster(uint32_t cluster)
{
    uint32_t fat_offset = cluster * 2;
    uint32_t fat_sector = bpb.fat_start_lba + fat_offset / SECTOR_SZ;
    uint32_t fat_entry_off = fat_offset % SECTOR_SZ;

    uint8_t buf[SECTOR_SZ];
    if (read_sector(fat_sector, buf) != 0) return 0xFFFFu;
    return (uint32_t)(buf[fat_entry_off] | (buf[fat_entry_off + 1] << 8));
}

/* ---- Convert cluster number to first LBA ---- */
static uint32_t cluster_to_lba(uint32_t cluster)
{
    return bpb.data_start_lba + (cluster - 2) * bpb.sectors_per_cluster;
}

/* ---- Convert 8.3 filename to FAT directory entry format (11 chars, padded) ---- */
static void make_fat83(const char *name, char out[11])
{
    r_memset(out, ' ', 11);
    int i = 0;
    /* Copy up to 8 chars of base name */
    while (*name && *name != '.' && i < 8) out[i++] = (char)r_toupper((unsigned char)*name++);
    /* Skip to extension */
    while (*name && *name != '.') name++;
    if (*name == '.') {
        name++;
        int j = 8;
        while (*name && j < 11) out[j++] = (char)r_toupper((unsigned char)*name++);
    }
}

/* ---- File handle pool (max 2 open files at once) ---- */
#define MAX_OPEN 2
static fat_file_t file_pool[MAX_OPEN];
static int        file_pool_used[MAX_OPEN];

/* ---- fat_init ---- */
int fat_init(void)
{
    r_memset(file_pool, 0, sizeof(file_pool));
    r_memset(file_pool_used, 0, sizeof(file_pool_used));
    sec_cached = 0xFFFFFFFFu;
    bpb.initialized = 0;
    return 0;
}

/* ---- fat_open ---- */
fat_file_t *fat_open(const char *filename)
{
    if (!bpb.initialized && fat_parse_bpb() != 0) return NULL;

    /* Convert filename to FAT 8.3 */
    char fat83[11];
    make_fat83(filename, fat83);

    /* Search root directory */
    uint8_t buf[SECTOR_SZ];
    uint32_t root_sectors = ((uint32_t)bpb.root_entry_count * 32 + SECTOR_SZ - 1) / SECTOR_SZ;

    for (uint32_t s = 0; s < root_sectors; s++) {
        if (read_sector(bpb.root_dir_lba + s, buf) != 0) return NULL;

        for (int e = 0; e < SECTOR_SZ / 32; e++) {
            uint8_t *entry = buf + e * 32;
            if (entry[0] == 0x00) { kputs("[FAT] End of directory at sector "); kputs("\n"); return NULL; }
            if (entry[0] == 0xE5) continue;      /* Deleted */
            if (entry[11] & 0x08) continue;      /* Volume ID */
            if (entry[11] & 0x10) continue;      /* Directory */

            char dname[12];
            char dname2[12];
            r_memcpy(dname, entry, 11);
            dname[11] = '\0';
            r_memcpy(dname2, fat83, 11);
            dname2[11] = '\0';
            if (r_strncasecmp((const char*)entry, fat83, 11) == 0) {
                /* Found! Allocate file handle */
                for (int i = 0; i < MAX_OPEN; i++) {
                    if (!file_pool_used[i]) {
                        fat_file_t *f = &file_pool[i];
                        r_memcpy(&f->dir_entry, entry, 32);
                        f->current_cluster = (uint32_t)(entry[26] | (entry[27] << 8));
                        f->current_pos     = 0;
                        f->size = (uint32_t)(entry[28]|(entry[29]<<8)|(entry[30]<<16)|(entry[31]<<24));

                        file_pool_used[i] = 1;
                        return f;
                    }
                }
                return NULL;  /* No free handles */
            }
        }
    }
    return NULL;
}

/* ---- fat_read ---- */
int fat_read(fat_file_t *file, void *buffer, uint32_t count)
{
    if (!file || !buffer || !count) return 0;
    if (file->current_pos >= file->size) return 0;
    if (count > file->size - file->current_pos)
        count = file->size - file->current_pos;

    uint8_t  *dst  = (uint8_t *)buffer;
    uint32_t  left = count;

    while (left > 0) {
        if (file->current_cluster < 2 || file->current_cluster >= 0xFFF8u) break;

        uint32_t cluster_bytes = (uint32_t)bpb.sectors_per_cluster * SECTOR_SZ;
        uint32_t offset_in_cluster = file->current_pos % cluster_bytes;
        uint32_t bytes_in_cluster  = cluster_bytes - offset_in_cluster;
        if (bytes_in_cluster > left) bytes_in_cluster = left;

        /* Read sector by sector within cluster */
        uint32_t cluster_lba = cluster_to_lba(file->current_cluster);
        uint32_t sec_idx = offset_in_cluster / SECTOR_SZ;
        uint32_t sec_off = offset_in_cluster % SECTOR_SZ;

        uint8_t sec_buf[SECTOR_SZ];
        uint32_t bytes_to_go = bytes_in_cluster;

        while (bytes_to_go > 0) {
            if (read_sector(cluster_lba + sec_idx, sec_buf) != 0) break;
            uint32_t chunk = SECTOR_SZ - sec_off;
            if (chunk > bytes_to_go) chunk = bytes_to_go;
            r_memcpy(dst, sec_buf + sec_off, chunk);
            dst          += chunk;
            bytes_to_go  -= chunk;
            left         -= chunk;
            file->current_pos += chunk;
            sec_off = 0;
            sec_idx++;
        }

        /* Move to next cluster if we exhausted the current one */
        if (file->current_pos % cluster_bytes == 0 && left > 0) {
            file->current_cluster = fat_next_cluster(file->current_cluster);
        }
    }

    return (int)(count - left);
}

/* ---- fat_close ---- */
int fat_close(fat_file_t *file)
{
    for (int i = 0; i < MAX_OPEN; i++) {
        if (&file_pool[i] == file) {
            file_pool_used[i] = 0;
            return 0;
        }
    }
    return -1;
}

/* ---- fat_file_size ---- */
uint32_t fat_file_size(fat_file_t *file)
{
    return file ? file->size : 0;
}

/* ---- fat_seek — reposition without re-reading ---- */
int fat_seek(fat_file_t *file, int offset)
{
    if (!file) return -1;
    if (offset < 0 || (uint32_t)offset > file->size) return -1;

    /* Reset to start and walk forward cluster by cluster */
    uint32_t cluster_bytes = (uint32_t)bpb.sectors_per_cluster * SECTOR_SZ;
    file->current_cluster  = (uint32_t)(file->dir_entry.first_cluster_lo);
    file->current_pos      = 0;

    while ((uint32_t)offset >= cluster_bytes &&
           file->current_cluster >= 2 && file->current_cluster < 0xFFF8u) {
        file->current_cluster = fat_next_cluster(file->current_cluster);
        file->current_pos    += cluster_bytes;
        offset               -= cluster_bytes;
    }
    file->current_pos += (uint32_t)offset;
    return 0;
}

/* ---- fat_list_root ---- */
void fat_list_root(void) {}
