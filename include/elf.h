/**
 * @file    elf.h
 * @author  shadcy
 * @brief   ELF-32 Executable and Linkable Format Definitions & Loader API.
 *
 * Part of the STAX Operating System.
 *
 * @license GPL-3.0-or-later
 * Copyright (c) 2026 Shreyash Wanjari (Shadcy)
 */

#ifndef STAX_ELF_H
#define STAX_ELF_H

#include <stdint.h>
#include <stddef.h>
#include "mmu.h"

/* ELF Identification Indices */
#define EI_MAG0         0
#define EI_MAG1         1
#define EI_MAG2         2
#define EI_MAG3         3
#define EI_CLASS        4
#define EI_DATA         5
#define EI_VERSION      6
#define EI_OSABI        7
#define EI_ABIVERSION   8
#define EI_PAD          9
#define EI_NIDENT       16

/* ELF Magic Numbers */
#define ELFMAG0         0x7f
#define ELFMAG1         'E'
#define ELFMAG2         'L'
#define ELFMAG3         'F'

/* ELF Architecture Constants */
#define ELFCLASS32      1
#define ELFDATA2LSB     1   /* Little Endian */
#define EV_CURRENT      1
#define ET_EXEC         2   /* Executable File */
#define EM_ARM          40  /* ARM 32-bit Architecture */

/* Segment Types (Program Header) */
#define PT_NULL         0
#define PT_LOAD         1   /* Loadable Segment */
#define PT_DYNAMIC      2
#define PT_INTERP       3
#define PT_NOTE         4
#define PT_SHLIB        5
#define PT_PHDR         6

/* Segment Flags */
#define PF_X            0x1 /* Execute */
#define PF_W            0x2 /* Write */
#define PF_R            0x4 /* Read */

/* User-space Virtual Memory Layout Defaults (128MB Region, isolated from Kernel 0..32MB) */
#define USER_TEXT_BASE  0x08000000  /* User Virtual Base: 128 MB */
#define USER_STACK_TOP  0x08100000  /* Top of User Stack */
#define USER_STACK_SIZE 0x00010000  /* 64 KB Stack */

/* ELF-32 Type Definitions */
typedef uint32_t Elf32_Addr;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Off;
typedef int32_t  Elf32_Sword;
typedef uint32_t Elf32_Word;

/* ELF-32 File Header */
typedef struct {
    unsigned char e_ident[EI_NIDENT];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
} __attribute__((packed)) Elf32_Ehdr;

/* ELF-32 Program (Segment) Header */
typedef struct {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
} __attribute__((packed)) Elf32_Phdr;

/* Loaded Process Descriptor */
typedef struct {
    uint32_t   entry_point;     /* Virtual address of entry point */
    uint32_t   user_stack_top;  /* Initial SP in user mode */
    pagedir_t *page_dir;        /* Dedicated 2-level 4KB page directory */
    uint32_t   brk_start;       /* Base of user heap */
    uint32_t   brk_current;     /* Current break address */
} elf_process_t;

/* Loader Return Codes */
#define ELF_OK          0
#define ELF_ERR_OPEN   -1
#define ELF_ERR_FORMAT -2
#define ELF_ERR_ARM    -3
#define ELF_ERR_NOMEM  -4
#define ELF_ERR_READ   -5

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Parse and map an ELF-32 binary into a fresh user address space.
 * @param path Path on SD card (e.g. "/bin/hello.elf" or "hello.elf")
 * @param proc Pointer to output process descriptor
 * @return ELF_OK on success, negative error code on failure.
 */
int elf_load_binary(const char *path, elf_process_t *proc);

/**
 * @brief Load and execute an ELF-32 binary as an active user process.
 * @param path Path to ELF file
 * @param argc Number of arguments
 * @param argv Argument vector
 * @return 0 on success, negative error on failure.
 */
int elf_exec(const char *path, int argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif /* STAX_ELF_H */
