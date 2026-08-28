/**
 * @file    stapp.h
 * @author  shadcy
 * @brief   STAX Application Package (.stapp) Format Definitions
 *
 * .stapp is a flat-archive format (STX) for distributing games and apps
 * on STAX OS. A single .stapp file contains the ELF binary, manifest,
 * icon, and data assets. No compression — simple concatenation with a
 * header table for minimal C implementation on bare metal.
 *
 * Archive layout:
 *   [stapp_header_t]            — magic, version, num_entries
 *   [stapp_entry_t * N]         — file name table with offsets
 *   [raw file data ...]         — concatenated file contents
 *
 * @license GPL-3.0-or-later
 * Copyright (c) 2026 Shreyash Wanjari (Shadcy)
 */
#ifndef STAPP_H
#define STAPP_H

#include <stdint.h>
#include <stddef.h>

/* ============================================================================
 * Archive Format Constants
 * ============================================================================ */
#define STAPP_MAGIC         0x53545858u  /* "STXX" */
#define STAPP_VERSION       1u
#define STAPP_NAME_MAX      48           /* max filename length in entry */
#define STAPP_ENTRIES_MAX   32           /* max files per package */
#define STAPP_MANIFEST      "manifest.txt"
#define STAPP_DEFAULT_ELF   "app.elf"
#define STAPP_DEFAULT_ICON  "icon.bmp"

/* Entry flags */
#define STAPP_FLAG_EXEC     0x01u        /* ELF executable */
#define STAPP_FLAG_ASSET    0x02u        /* Data asset (WAD, BMP, etc.) */
#define STAPP_FLAG_ICON     0x04u        /* Desktop icon */
#define STAPP_FLAG_MANIFEST 0x08u        /* manifest.txt */

/* ============================================================================
 * On-disk structures
 * ============================================================================ */
typedef struct __attribute__((packed)) {
    char     name[STAPP_NAME_MAX];  /* relative path inside archive */
    uint32_t offset;                /* byte offset from start of file */
    uint32_t size;                  /* byte size of file content */
    uint32_t flags;                 /* STAPP_FLAG_* */
    uint32_t reserved;
} stapp_entry_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;                 /* STAPP_MAGIC */
    uint32_t version;               /* STAPP_VERSION */
    uint32_t num_entries;
    uint32_t reserved[5];
} stapp_header_t;

/* ============================================================================
 * Runtime context (in-memory after stapp_open)
 * ============================================================================ */
#define STAPP_MANIFEST_KEYS 16
#define STAPP_KEY_LEN       32
#define STAPP_VAL_LEN       64

typedef struct {
    char key[STAPP_KEY_LEN];
    char val[STAPP_VAL_LEN];
} stapp_kv_t;

typedef struct {
    char          path[64];                  /* FAT path of .stapp file */
    stapp_header_t hdr;
    stapp_entry_t  entries[STAPP_ENTRIES_MAX];
    stapp_kv_t     manifest[STAPP_MANIFEST_KEYS];
    int            manifest_count;
    int            valid;
} stapp_t;

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * stapp_open — open and parse a .stapp archive from the FAT filesystem.
 * @param path  FAT path e.g. "/DOOM.STAPP"
 * @param app   out: populated stapp_t context
 * @return 0 on success, negative on error
 */
int stapp_open(const char *path, stapp_t *app);

/**
 * stapp_get_manifest — read a manifest.txt key/value pair.
 * @param app  opened stapp context
 * @param key  key to look up (e.g. "name", "entry", "data")
 * @param buf  output buffer
 * @param bufsz size of output buffer
 * @return 0 on success, -1 if key not found
 */
int stapp_get_manifest(const stapp_t *app, const char *key, char *buf, size_t bufsz);

/**
 * stapp_extract_to_buf — extract a named file from the archive into a heap buffer.
 * Caller must kfree() the returned buffer.
 * @param path  FAT path of the .stapp archive
 * @param name  internal archive filename (e.g. "app.elf")
 * @param out_size  out: number of bytes written
 * @return pointer to allocated buffer, or NULL on error
 */
void *stapp_extract_to_buf(const char *path, const char *name, uint32_t *out_size);

/**
 * stapp_exec — launch a .stapp package: reads manifest, extracts ELF,
 * sets up argv[0]=WAD path if needed, and calls elf_exec.
 * @return exit code of the process
 */
int stapp_exec(const char *path);

#endif /* STAPP_H */
