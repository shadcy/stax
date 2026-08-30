/**
 * @file    launch.h
 * @author  shadcy
 * @brief   STAX Application Package (.launch) Format Definitions
 *
 * .launch is a flat-archive format for distributing games and apps
 * on STAX OS. A single .launch file contains the ELF binary, manifest,
 * icon, and data assets. No compression — simple concatenation with a
 * header table for minimal C implementation on bare metal.
 *
 * Archive layout:
 *   [launch_header_t]           — magic, version, num_entries
 *   [launch_entry_t * N]        — file name table with offsets
 *   [raw file data ...]         — concatenated file contents
 *
 * @license GPL-3.0-or-later
 * Copyright (c) 2026 Shreyash Wanjari (Shadcy)
 */
#ifndef LAUNCH_H
#define LAUNCH_H

#include <stdint.h>
#include <stddef.h>

/* ============================================================================
 * Archive Format Constants
 * ============================================================================ */
#define LAUNCH_MAGIC         0x53545858u  /* "STXX" */
#define LAUNCH_VERSION       1u
#define LAUNCH_NAME_MAX      48           /* max filename length in entry */
#define LAUNCH_ENTRIES_MAX   32           /* max files per package */
#define LAUNCH_MANIFEST      "manifest.txt"
#define LAUNCH_DEFAULT_ELF   "app.elf"
#define LAUNCH_DEFAULT_ICON  "icon.bmp"

/* Entry flags */
#define LAUNCH_FLAG_EXEC     0x01u        /* ELF executable */
#define LAUNCH_FLAG_ASSET    0x02u        /* Data asset (WAD, BMP, etc.) */
#define LAUNCH_FLAG_ICON     0x04u        /* Desktop icon */
#define LAUNCH_FLAG_MANIFEST 0x08u        /* manifest.txt */

/* Compatibility aliases */
#define STAPP_MAGIC          LAUNCH_MAGIC
#define STAPP_VERSION        LAUNCH_VERSION
#define STAPP_NAME_MAX       LAUNCH_NAME_MAX
#define STAPP_ENTRIES_MAX    LAUNCH_ENTRIES_MAX
#define STAPP_FLAG_EXEC      LAUNCH_FLAG_EXEC
#define STAPP_FLAG_ASSET     LAUNCH_FLAG_ASSET
#define STAPP_FLAG_ICON      LAUNCH_FLAG_ICON
#define STAPP_FLAG_MANIFEST  LAUNCH_FLAG_MANIFEST

/* ============================================================================
 * On-disk structures
 * ============================================================================ */
typedef struct __attribute__((packed)) {
    char     name[LAUNCH_NAME_MAX]; /* relative path inside archive */
    uint32_t offset;                /* byte offset from start of file */
    uint32_t size;                  /* byte size of file content */
    uint32_t flags;                 /* LAUNCH_FLAG_* */
    uint32_t reserved;
} launch_entry_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;                 /* LAUNCH_MAGIC */
    uint32_t version;               /* LAUNCH_VERSION */
    uint32_t num_entries;
    uint32_t reserved[5];
} launch_header_t;

/* Compatibility type aliases */
typedef launch_entry_t  stapp_entry_t;
typedef launch_header_t stapp_header_t;

/* ============================================================================
 * Runtime context (in-memory after launch_open)
 * ============================================================================ */
#define LAUNCH_MANIFEST_KEYS 16
#define LAUNCH_KEY_LEN       32
#define LAUNCH_VAL_LEN       64

typedef struct {
    char key[LAUNCH_KEY_LEN];
    char val[LAUNCH_VAL_LEN];
} launch_kv_t;

typedef struct {
    char            path[64];                    /* FAT path of .launch file */
    launch_header_t hdr;
    launch_entry_t  entries[LAUNCH_ENTRIES_MAX];
    launch_kv_t     manifest[LAUNCH_MANIFEST_KEYS];
    int             manifest_count;
    int             valid;
} launch_t;

typedef launch_kv_t stapp_kv_t;
typedef launch_t    stapp_t;

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * launch_open — open and parse a .launch archive from the FAT filesystem.
 * @param path  FAT path e.g. "/DOOM.LAUNCH"
 * @param app   out: populated launch_t context
 * @return 0 on success, negative on error
 */
int launch_open(const char *path, launch_t *app);

/**
 * launch_get_manifest — read a manifest.txt key/value pair.
 * @param app  opened launch context
 * @param key  key to look up (e.g. "name", "entry", "data")
 * @param buf  output buffer
 * @param bufsz size of output buffer
 * @return 0 on success, -1 if key not found
 */
int launch_get_manifest(const launch_t *app, const char *key, char *buf, size_t bufsz);

/**
 * launch_extract_to_buf — extract a named file from the archive into a heap buffer.
 * Caller must kfree() the returned buffer.
 * @param path  FAT path of the .launch archive
 * @param name  internal archive filename (e.g. "app.elf")
 * @param out_size  out: number of bytes written
 * @return pointer to allocated buffer, or NULL on error
 */
void *launch_extract_to_buf(const char *path, const char *name, uint32_t *out_size);

/**
 * launch_exec — launch a .launch package: reads manifest, extracts ELF,
 * sets up argv[0]=WAD path if needed, and calls elf_exec.
 * @return exit code of the process
 */
int launch_exec(const char *path);

/* Compatibility macros */
#define stapp_open           launch_open
#define stapp_get_manifest   launch_get_manifest
#define stapp_extract_to_buf launch_extract_to_buf
#define stapp_exec           launch_exec

#endif /* LAUNCH_H */
