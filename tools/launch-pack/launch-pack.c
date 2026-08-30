/*
 * stapp-pack — STAX Application Package Builder
 *
 * Host-side tool (runs on Linux/macOS) that assembles a .stapp archive
 * from a manifest file and a list of files.
 *
 * Usage:
 *   stapp-pack output.stapp manifest.txt [file1 [file2 ...]]
 *
 * All files are stored at their basename inside the archive, except files
 * listed under a "data/" prefix in the manifest which keep their prefix.
 *
 * @license GPL-3.0-or-later
 * Copyright (c) 2026 Shreyash Wanjari (Shadcy)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#define STAPP_MAGIC         0x53545858u
#define STAPP_VERSION       1u
#define STAPP_NAME_MAX      48
#define STAPP_ENTRIES_MAX   32
#define STAPP_FLAG_EXEC     0x01u
#define STAPP_FLAG_ASSET    0x02u
#define STAPP_FLAG_ICON     0x04u
#define STAPP_FLAG_MANIFEST 0x08u

typedef struct __attribute__((packed)) {
    char     name[STAPP_NAME_MAX];
    uint32_t offset;
    uint32_t size;
    uint32_t flags;
    uint32_t reserved;
} stapp_entry_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t version;
    uint32_t num_entries;
    uint32_t reserved[5];
} stapp_header_t;

static const char *basename_of(const char *path)
{
    const char *b = path;
    for (const char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\') b = p + 1;
    }
    return b;
}

static long file_size(FILE *f)
{
    long pos = ftell(f);
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, pos, SEEK_SET);
    return sz;
}

static uint32_t detect_flags(const char *name)
{
    size_t n = strlen(name);
    if (n >= 4) {
        const char *ext = name + n - 4;
        if (strcasecmp(ext, ".elf") == 0) return STAPP_FLAG_EXEC;
        if (strcasecmp(ext, ".bmp") == 0) return STAPP_FLAG_ICON;
    }
    if (n >= 3 && strcasecmp(name + n - 3, "txt") == 0) return STAPP_FLAG_MANIFEST;
    return STAPP_FLAG_ASSET;
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s output.stapp manifest.txt [file1 ...]\n", argv[0]);
        fprintf(stderr, "\nExample:\n");
        fprintf(stderr, "  %s doom.stapp manifest_doom.txt build/doom.elf games/em-doom/doom1.wad\n", argv[0]);
        return 1;
    }

    const char *out_path      = argv[1];
    const char *manifest_path = argv[2];

    /* Collect all file paths (manifest + extra files) */
    int nfiles = 0;
    const char *files[STAPP_ENTRIES_MAX];
    const char *names[STAPP_ENTRIES_MAX]; /* archive-internal names */

    /* manifest is always first */
    files[nfiles] = manifest_path;
    names[nfiles] = "manifest.txt";
    nfiles++;

    /* remaining args */
    int elf_seen = 0;
    for (int i = 3; i < argc && nfiles < STAPP_ENTRIES_MAX; i++) {
        files[nfiles] = argv[i];
        names[nfiles] = basename_of(argv[i]);
        size_t n = strlen(names[nfiles]);
        /* ELF files are always stored as "app.elf" (first one wins) */
        if (n >= 4 && strcasecmp(names[nfiles] + n - 4, ".elf") == 0) {
            if (!elf_seen) { names[nfiles] = "app.elf"; elf_seen = 1; }
        }
        /* WAD files go under data/ prefix */
        else if (n >= 4 && strcasecmp(names[nfiles] + n - 4, ".wad") == 0) {
            /* store as "data/<name>" */
            static char wad_names[STAPP_ENTRIES_MAX][STAPP_NAME_MAX];
            snprintf(wad_names[nfiles], STAPP_NAME_MAX, "data/%s", names[nfiles]);
            /* lowercase */
            for (char *p = wad_names[nfiles] + 5; *p; p++) {
                if (*p >= 'A' && *p <= 'Z') *p += 32;
            }
            names[nfiles] = wad_names[nfiles];
        }
        nfiles++;
    }

    printf("stapp-pack: building '%s' with %d file(s)\n", out_path, nfiles);

    /* Calculate data offsets:
     *   header (32 bytes) + entry_table (64*N bytes) + file_data */
    uint32_t header_size  = (uint32_t)sizeof(stapp_header_t);
    uint32_t entries_size = (uint32_t)(sizeof(stapp_entry_t) * nfiles);
    uint32_t data_offset  = header_size + entries_size;

    stapp_entry_t entries[STAPP_ENTRIES_MAX];
    memset(entries, 0, sizeof(entries));

    /* First pass: compute sizes and offsets */
    uint32_t cur_offset = data_offset;
    for (int i = 0; i < nfiles; i++) {
        FILE *f = fopen(files[i], "rb");
        if (!f) {
            fprintf(stderr, "ERROR: cannot open '%s': %s\n", files[i], strerror(errno));
            return 1;
        }
        long sz = file_size(f);
        fclose(f);

        strncpy(entries[i].name, names[i], STAPP_NAME_MAX - 1);
        entries[i].name[STAPP_NAME_MAX - 1] = '\0';
        entries[i].offset   = cur_offset;
        entries[i].size     = (uint32_t)sz;
        entries[i].flags    = detect_flags(names[i]);
        entries[i].reserved = 0;

        printf("  [%2d] %-40s  %7u bytes  offset=0x%05x  flags=0x%02x\n",
               i, entries[i].name, entries[i].size, entries[i].offset, entries[i].flags);
        cur_offset += (uint32_t)sz;
    }

    /* Write output */
    FILE *out = fopen(out_path, "wb");
    if (!out) {
        fprintf(stderr, "ERROR: cannot create '%s': %s\n", out_path, strerror(errno));
        return 1;
    }

    /* Write header */
    stapp_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic       = STAPP_MAGIC;
    hdr.version     = STAPP_VERSION;
    hdr.num_entries = (uint32_t)nfiles;
    fwrite(&hdr, sizeof(hdr), 1, out);

    /* Write entry table */
    fwrite(entries, sizeof(stapp_entry_t), nfiles, out);

    /* Write file data */
    for (int i = 0; i < nfiles; i++) {
        FILE *f = fopen(files[i], "rb");
        if (!f) {
            fprintf(stderr, "ERROR: cannot open '%s': %s\n", files[i], strerror(errno));
            fclose(out);
            return 1;
        }
        uint8_t buf[4096];
        size_t rd;
        while ((rd = fread(buf, 1, sizeof(buf), f)) > 0)
            fwrite(buf, 1, rd, out);
        fclose(f);
    }

    long total = ftell(out);
    fclose(out);

    printf("stapp-pack: done! '%s' = %ld bytes total\n", out_path, total);
    return 0;
}
