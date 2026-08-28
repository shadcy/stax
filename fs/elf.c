/**
 * @file    elf.c
 * @author  shadcy
 * @brief   Dynamic ELF-32 Executable Binary Loader & Execution Subsystem for STAX.
 *
 * Part of the STAX Operating System.
 *
 * @license GPL-3.0-or-later
 * Copyright (c) 2026 Shreyash Wanjari (Shadcy)
 */

#include "elf.h"
#include "fatfs/ff.h"
#include "mmu.h"
#include "heap.h"
#include "page.h"
#include "string.h"
#include "console.h"
#include "scheduler.h"

/* ============================================================================
 * elf_load_binary — Load and map an ELF-32 binary into a dedicated address space
 * ============================================================================ */
int elf_load_binary(const char *path, elf_process_t *proc)
{
    FIL fil;
    FRESULT fr;
    UINT br;
    Elf32_Ehdr ehdr;

    if (!path || !proc) return ELF_ERR_FORMAT;

    fr = f_open(&fil, path, FA_READ);
    if (fr != FR_OK && path[0] != '/') {
        char alt[64];
        int i = 0;
        alt[i++] = '/';
        while (path[i - 1] && i < 55) {
            alt[i] = path[i - 1];
            i++;
        }
        alt[i] = '\0';
        fr = f_open(&fil, alt, FA_READ);
    }
    if (fr != FR_OK) {
        char alt[64];
        int i = 0;
        if (path[0] != '/') alt[i++] = '/';
        int p = 0;
        while (path[p] && i < 50) alt[i++] = path[p++];
        alt[i++] = '.'; alt[i++] = 'e'; alt[i++] = 'l'; alt[i++] = 'f'; alt[i] = '\0';
        fr = f_open(&fil, alt, FA_READ);
        if (fr != FR_OK) {
            alt[i - 3] = 'E'; alt[i - 2] = 'L'; alt[i - 1] = 'F';
            fr = f_open(&fil, alt, FA_READ);
        }
    }

    if (fr != FR_OK) {
        kprintf("[ELF] Failed to open file: %s (err %d)\n", path, (int)fr);
        return ELF_ERR_OPEN;
    }

    /* 1. Read and validate ELF Header */
    fr = f_read(&fil, &ehdr, sizeof(Elf32_Ehdr), &br);
    if (fr != FR_OK || br != sizeof(Elf32_Ehdr)) {
        f_close(&fil);
        return ELF_ERR_READ;
    }

    /* Check ELF Magic: 0x7F, 'E', 'L', 'F' */
    if (ehdr.e_ident[EI_MAG0] != ELFMAG0 ||
        ehdr.e_ident[EI_MAG1] != ELFMAG1 ||
        ehdr.e_ident[EI_MAG2] != ELFMAG2 ||
        ehdr.e_ident[EI_MAG3] != ELFMAG3) {
        kprintf("[ELF] Invalid magic signature in %s\n", path);
        f_close(&fil);
        return ELF_ERR_FORMAT;
    }

    /* Verify 32-bit Little-Endian ARM Executable */
    if (ehdr.e_ident[EI_CLASS] != ELFCLASS32 ||
        ehdr.e_ident[EI_DATA]  != ELFDATA2LSB ||
        ehdr.e_machine         != EM_ARM) {
        kprintf("[ELF] Incompatible architecture (must be 32-bit ARM Little-Endian)\n");
        f_close(&fil);
        return ELF_ERR_ARM;
    }

    if (ehdr.e_type != ET_EXEC && ehdr.e_type != 3 /* ET_DYN (PIE) */) {
        kprintf("[ELF] File is not an executable (e_type=%d)\n", (int)ehdr.e_type);
        f_close(&fil);
        return ELF_ERR_FORMAT;
    }

    /* 2. Create a fresh isolated 2-level 4KB page directory for the process */
    proc->page_dir = mmu_create_address_space();
    if (!proc->page_dir) {
        kprintf("[ELF] Out of memory creating page directory\n");
        f_close(&fil);
        return ELF_ERR_NOMEM;
    }

    uint32_t highest_vaddr = 0;

    /* 3. Iterate through Program Headers and map PT_LOAD segments */
    for (int i = 0; i < (int)ehdr.e_phnum; i++) {
        Elf32_Phdr phdr;
        f_lseek(&fil, ehdr.e_phoff + i * sizeof(Elf32_Phdr));
        fr = f_read(&fil, &phdr, sizeof(Elf32_Phdr), &br);
        if (fr != FR_OK || br != sizeof(Elf32_Phdr)) {
            mmu_destroy_address_space(proc->page_dir);
            f_close(&fil);
            return ELF_ERR_READ;
        }

        if (phdr.p_type != PT_LOAD || phdr.p_memsz == 0) continue;

        uint32_t vstart = phdr.p_vaddr;
        uint32_t vend   = phdr.p_vaddr + phdr.p_memsz;
        if (vend > highest_vaddr) highest_vaddr = vend;

        uint32_t page_start = vstart & PAGE_MASK;
        uint32_t page_end   = (vend + PAGE_SIZE - 1) & PAGE_MASK;
        uint32_t flags      = (phdr.p_flags & PF_W) ? MMU_PAGE_USER_RW : MMU_PAGE_USER_RO;

        for (uint32_t curr_vaddr = page_start; curr_vaddr < page_end; curr_vaddr += PAGE_SIZE) {
            /* Check if physical page already exists */
            uint32_t phys_page = mmu_get_phys(proc->page_dir, curr_vaddr);
            if (!phys_page) {
                void *page_mem = alloc_page();
                if (!page_mem) {
                    mmu_destroy_address_space(proc->page_dir);
                    f_close(&fil);
                    return ELF_ERR_NOMEM;
                }
                memset(page_mem, 0, PAGE_SIZE);
                phys_page = (uint32_t)page_mem;
                mmu_map_page(proc->page_dir, curr_vaddr, phys_page, flags);
            }

            /* Copy file slice into the physical page */
            if (curr_vaddr < vstart + phdr.p_filesz && (curr_vaddr + PAGE_SIZE) > vstart) {
                uint32_t copy_vstart = (curr_vaddr < vstart) ? vstart : curr_vaddr;
                uint32_t copy_vend   = ((curr_vaddr + PAGE_SIZE) < (vstart + phdr.p_filesz)) 
                                       ? (curr_vaddr + PAGE_SIZE) : (vstart + phdr.p_filesz);
                
                uint32_t page_offset = copy_vstart - curr_vaddr;
                uint32_t file_offset = phdr.p_offset + (copy_vstart - vstart);
                uint32_t copy_bytes  = copy_vend - copy_vstart;

                f_lseek(&fil, file_offset);
                f_read(&fil, (void *)(phys_page + page_offset), copy_bytes, &br);
            }
        }
    }

    /* 4. Allocate and map 256KB User Stack at USER_STACK_TOP */
    uint32_t stack_base = USER_STACK_TOP - USER_STACK_SIZE;
    for (uint32_t sp_vaddr = stack_base; sp_vaddr <= USER_STACK_TOP; sp_vaddr += PAGE_SIZE) {
        void *stack_page = alloc_page();
        if (!stack_page) {
            mmu_destroy_address_space(proc->page_dir);
            f_close(&fil);
            return ELF_ERR_NOMEM;
        }
        memset(stack_page, 0, PAGE_SIZE);
        mmu_map_page(proc->page_dir, sp_vaddr, (uint32_t)stack_page, MMU_PAGE_USER_RW);
    }

    proc->entry_point    = ehdr.e_entry;
    proc->user_stack_top = USER_STACK_TOP - 16; /* 16-byte aligned for AAPCS */
    proc->brk_start      = (highest_vaddr + PAGE_SIZE - 1) & PAGE_MASK;
    proc->brk_current    = proc->brk_start;

    f_close(&fil);
    kprintf("[ELF] Successfully loaded %s (Entry: 0x%x, Stack: 0x%x)\n",
            path, proc->entry_point, proc->user_stack_top);
    return ELF_OK;
}

/* ============================================================================
 * elf_exec — Load and execute an ELF binary as a user task
 * ============================================================================ */
int elf_exec(const char *path, int argc, char **argv)
{
    (void)argc;
    (void)argv;
    elf_process_t proc;
    int rc = elf_load_binary(path, &proc);
    if (rc != ELF_OK) {
        kprintf("[ELF] Failed to execute '%s' (error code %d)\n", path, rc);
        return rc;
    }

    kprintf("[ELF] Launching %s (Entry: 0x%x, SP: 0x%x) in USR mode...\n",
            path, proc.entry_point, proc.user_stack_top);

    /* Switch into the process's dedicated 4KB page directory */
    mmu_switch_address_space(proc.page_dir);

    /* Direct jump to unprivileged USR mode (0x10) — returns on SYS_EXIT */
    extern int arch_enter_user(uint32_t entry_point, uint32_t user_stack_top);
    int exit_code = arch_enter_user(proc.entry_point, proc.user_stack_top);

    /* Restore Master Kernel Page Directory */
    mmu_switch_address_space(NULL);

    /* Clean up isolated process address space */
    mmu_destroy_address_space(proc.page_dir);

    kprintf("[ELF] Process %s finished (exit code: %d)\n", path, exit_code);
    return exit_code;
}
