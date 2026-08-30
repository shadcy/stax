/**
 * @file    launch.c
 * @author  shadcy
 * @brief   STAX Application Package (.launch) Loader
 *
 * Implements launch_open, launch_get_manifest, launch_extract_to_buf,
 * and launch_exec for launching .launch packages from the FAT filesystem.
 *
 * @license GPL-3.0-or-later
 * Copyright (c) 2026 Shreyash Wanjari (Shadcy)
 */

#include "launch.h"
#include "elf.h"
#include "heap.h"
#include "string.h"
#include "console.h"
#include "fatfs/ff.h"

/* ============================================================================
 * Internal helpers
 * ============================================================================ */

static int launch_str_eq(const char *a, const char *b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return (*a == '\0' && *b == '\0');
}

static size_t launch_strlen(const char *s) {
    size_t n = 0; while (s[n]) n++; return n;
}

static void launch_strncpy(char *dst, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n - 1 && src[i]; i++) dst[i] = src[i];
    dst[i] = '\0';
}

/* Parse "key=value\n" lines from a manifest buffer into kv table */
static int parse_manifest(const char *buf, uint32_t size,
                          launch_kv_t *kv, int max_kv)
{
    int count = 0;
    const char *p = buf;
    const char *end = buf + size;

    while (p < end && count < max_kv) {
        /* Find '=' on this line */
        const char *eq = p;
        while (eq < end && *eq != '=' && *eq != '\n' && *eq != '\0') eq++;
        if (eq >= end || *eq != '=') {
            /* Skip line */
            while (p < end && *p != '\n') p++;
            if (p < end) p++;
            continue;
        }

        /* Key */
        size_t klen = (size_t)(eq - p);
        if (klen == 0 || klen >= LAUNCH_KEY_LEN) { p = eq + 1; continue; }
        size_t i;
        for (i = 0; i < klen; i++) kv[count].key[i] = p[i];
        kv[count].key[i] = '\0';

        /* Value */
        const char *vs = eq + 1;
        const char *ve = vs;
        while (ve < end && *ve != '\n' && *ve != '\r' && *ve != '\0') ve++;
        size_t vlen = (size_t)(ve - vs);
        if (vlen >= LAUNCH_VAL_LEN) vlen = LAUNCH_VAL_LEN - 1;
        for (i = 0; i < vlen; i++) kv[count].val[i] = vs[i];
        kv[count].val[i] = '\0';

        count++;
        p = ve;
        while (p < end && (*p == '\n' || *p == '\r')) p++;
    }
    return count;
}

/* ============================================================================
 * launch_open
 * ============================================================================ */
int launch_open(const char *path, launch_t *app)
{
    FIL fil;
    UINT br;
    FRESULT fr;

    if (!path || !app) return -1;
    memset(app, 0, sizeof(launch_t));

    fr = f_open(&fil, path, FA_READ);
    if (fr != FR_OK) {
        kprintf("[LAUNCH] Cannot open '%s' (err %d)\n", path, (int)fr);
        return -1;
    }

    /* Read archive header */
    fr = f_read(&fil, &app->hdr, sizeof(launch_header_t), &br);
    if (fr != FR_OK || br != sizeof(launch_header_t)) { f_close(&fil); return -2; }

    if (app->hdr.magic != LAUNCH_MAGIC) {
        kprintf("[LAUNCH] Bad magic in '%s' (got 0x%x)\n", path, app->hdr.magic);
        f_close(&fil);
        return -3;
    }
    if (app->hdr.num_entries > LAUNCH_ENTRIES_MAX) {
        kprintf("[LAUNCH] Too many entries (%u)\n", app->hdr.num_entries);
        f_close(&fil);
        return -4;
    }

    /* Read entry table */
    uint32_t n = app->hdr.num_entries;
    fr = f_read(&fil, app->entries, sizeof(launch_entry_t) * n, &br);
    if (fr != FR_OK || br != sizeof(launch_entry_t) * n) { f_close(&fil); return -5; }

    f_close(&fil);

    /* Save path */
    launch_strncpy(app->path, path, sizeof(app->path));

    /* Extract and parse manifest */
    uint32_t msize = 0;
    void *mbuf = launch_extract_to_buf(path, LAUNCH_MANIFEST, &msize);
    if (mbuf && msize > 0) {
        app->manifest_count = parse_manifest((const char *)mbuf, msize,
                                             app->manifest, LAUNCH_MANIFEST_KEYS);
        kfree(mbuf);
    }

    app->valid = 1;
    return 0;
}

/* ============================================================================
 * launch_get_manifest
 * ============================================================================ */
int launch_get_manifest(const launch_t *app, const char *key, char *buf, size_t bufsz)
{
    if (!app || !key || !buf || bufsz == 0) return -1;
    for (int i = 0; i < app->manifest_count; i++) {
        if (launch_str_eq(app->manifest[i].key, key)) {
            launch_strncpy(buf, app->manifest[i].val, bufsz);
            return 0;
        }
    }
    buf[0] = '\0';
    return -1;
}

/* ============================================================================
 * launch_extract_to_buf
 * ============================================================================ */
void *launch_extract_to_buf(const char *launch_path, const char *name, uint32_t *out_size)
{
    FIL fil;
    UINT br;
    FRESULT fr;
    launch_header_t hdr;
    launch_entry_t  entries[LAUNCH_ENTRIES_MAX];

    if (!launch_path || !name || !out_size) return NULL;
    *out_size = 0;

    fr = f_open(&fil, launch_path, FA_READ);
    if (fr != FR_OK) return NULL;

    /* Read header */
    f_read(&fil, &hdr, sizeof(launch_header_t), &br);
    if (hdr.magic != LAUNCH_MAGIC || hdr.num_entries > LAUNCH_ENTRIES_MAX) {
        f_close(&fil); return NULL;
    }

    /* Read entries */
    uint32_t n = hdr.num_entries;
    f_read(&fil, entries, sizeof(launch_entry_t) * n, &br);

    /* Find the named entry */
    for (uint32_t i = 0; i < n; i++) {
        if (launch_str_eq(entries[i].name, name)) {
            uint32_t size = entries[i].size;
            if (size == 0) { f_close(&fil); return NULL; }

            void *buf = kmalloc(size + 1);
            if (!buf) { f_close(&fil); return NULL; }

            f_lseek(&fil, entries[i].offset);
            f_read(&fil, buf, size, &br);
            f_close(&fil);

            if (br != size) { kfree(buf); return NULL; }
            ((uint8_t *)buf)[size] = 0; /* null-terminate for text files */
            *out_size = size;
            return buf;
        }
    }

    f_close(&fil);
    return NULL;
}

/* ============================================================================
 * launch_exec — extract ELF from archive into temp file, then elf_exec it
 * ============================================================================ */
int launch_exec(const char *path)
{
    launch_t app;
    char entry_name[LAUNCH_NAME_MAX];
    char data_name[LAUNCH_NAME_MAX];
    char data_fat_path[64];

    kprintf("[LAUNCH] Loading package: %s\n", path);

    if (launch_open(path, &app) != 0) {
        kprintf("[LAUNCH] Failed to open package.\n");
        return -1;
    }

    /* Get entry ELF name from manifest */
    if (launch_get_manifest(&app, "entry", entry_name, sizeof(entry_name)) != 0) {
        launch_strncpy(entry_name, LAUNCH_DEFAULT_ELF, sizeof(entry_name));
    }

    /* Get app name for display */
    char app_name[64] = "App";
    launch_get_manifest(&app, "name", app_name, sizeof(app_name));
    kprintf("[LAUNCH] Launching '%s' ...\n", app_name);

    /* Extract data asset to FAT /tmp/ if manifest specifies one */
    data_fat_path[0] = '\0';
    if (launch_get_manifest(&app, "data", data_name, sizeof(data_name)) == 0
        && data_name[0] != '\0')
    {
        /* Extract to /TMP/<basename> */
        const char *base = data_name;
        for (const char *p = data_name; *p; p++) {
            if (*p == '/' || *p == '\\') base = p + 1;
        }
        data_fat_path[0] = '/';
        launch_strncpy(data_fat_path + 1, base, sizeof(data_fat_path) - 1);

        /* Check if already extracted */
        FIL chk;
        int needs_extract = (f_open(&chk, data_fat_path, FA_READ) != FR_OK);
        if (!needs_extract) f_close(&chk);

        if (needs_extract) {
            kprintf("[LAUNCH] Extracting data: %s -> %s\n", data_name, data_fat_path);
            uint32_t dsize = 0;
            void *dbuf = launch_extract_to_buf(path, data_name, &dsize);
            if (dbuf && dsize > 0) {
                FIL fo; UINT bw;
                if (f_open(&fo, data_fat_path, FA_WRITE | FA_CREATE_ALWAYS) == FR_OK) {
                    f_write(&fo, dbuf, dsize, &bw);
                    f_close(&fo);
                    kprintf("[LAUNCH] Extracted %u bytes to %s\n", dsize, data_fat_path);
                } else {
                    kprintf("[LAUNCH] Warning: failed to write data to FAT\n");
                    data_fat_path[0] = '\0';
                }
                kfree(dbuf);
            }
        } else {
            kprintf("[LAUNCH] Data already present: %s\n", data_fat_path);
        }
    }

    /* Extract ELF to /_LAUNCH.ELF */
    const char *tmp_elf = "/_LAUNCH.ELF";
    uint32_t elfsize = 0;
    void *elfbuf = launch_extract_to_buf(path, entry_name, &elfsize);
    if (!elfbuf || elfsize == 0) {
        kprintf("[LAUNCH] Failed to extract ELF '%s'\n", entry_name);
        return -1;
    }

    /* Write ELF to temporary FAT file */
    FIL fo; UINT bw;
    FRESULT fr = f_open(&fo, tmp_elf, FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK) {
        kprintf("[LAUNCH] Cannot write temp ELF (err %d)\n", (int)fr);
        kfree(elfbuf);
        return -1;
    }
    f_write(&fo, elfbuf, elfsize, &bw);
    f_close(&fo);
    kfree(elfbuf);

    if (bw != elfsize) {
        kprintf("[LAUNCH] Short write: %u/%u bytes\n", bw, elfsize);
        return -1;
    }

    /* Build argv: argv[0] = data path (e.g. WAD), or entry name */
    char *argv[4];
    char argv0_buf[64];
    int argc = 0;
    if (data_fat_path[0] != '\0') {
        launch_strncpy(argv0_buf, data_fat_path + 1, sizeof(argv0_buf)); /* strip leading / */
        argv[argc++] = argv0_buf;
    }
    argv[argc] = NULL;

    /* Execute */
    extern int elf_exec(const char *path, int argc, char **argv);
    int rc = elf_exec(tmp_elf, argc, argv);

    /* Clean up temp ELF */
    f_unlink(tmp_elf);

    kprintf("[LAUNCH] '%s' exited (code %d)\n", app_name, rc);
    return rc;
}
