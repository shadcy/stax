/**
 * @file    mmu.h
 * @author  shadcy
 * @brief   Two-Level 4KB Paging and Virtual Memory Management Interface for STAX.
 *
 * Part of the STAX Operating System.
 *
 * @license GPL-3.0-or-later
 * Copyright (c) 2026 Shreyash Wanjari (Shadcy)
 */

#ifndef MMU_H
#define MMU_H

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE           4096
#define PAGE_MASK           (~(PAGE_SIZE - 1))

/* Page access permission flags */
#define MMU_PAGE_KERNEL_RW  0x01    /* SVC: RW, USR: No access */
#define MMU_PAGE_USER_RO    0x02    /* SVC: RW, USR: Read-Only */
#define MMU_PAGE_USER_RW    0x03    /* SVC: RW, USR: Read/Write */
#define MMU_PAGE_NOCACHE    0x04    /* Non-cacheable (I/O, Framebuffers) */
#define MMU_PAGE_BUFFERABLE 0x08    /* Bufferable write-combining */

/* Page Directory type (4096 L1 Section / Coarse Table entries = 16KB) */
typedef struct {
    uint32_t entries[4096];
} __attribute__((aligned(16384))) pagedir_t;

/* L2 Coarse Page Table (256 Small 4KB Page entries = 1KB) */
typedef struct {
    uint32_t entries[256];
} __attribute__((aligned(1024))) pagetable_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize the kernel master MMU and identity maps */
void mmu_init(void);

/* Create a new address space for a user process (shares kernel mappings) */
pagedir_t *mmu_create_address_space(void);

/* Free an address space and all its L2 page tables */
void mmu_destroy_address_space(pagedir_t *pd);

/* Map a virtual 4KB page to a physical address in the specified page directory */
int mmu_map_page(pagedir_t *pd, uint32_t vaddr, uint32_t paddr, uint32_t flags);

/* Unmap a virtual 4KB page */
int mmu_unmap_page(pagedir_t *pd, uint32_t vaddr);

/* Query physical address corresponding to a virtual address */
uint32_t mmu_get_phys(pagedir_t *pd, uint32_t vaddr);

/* Switch CPU Translation Table Base Register (TTBR0) to a new address space */
void mmu_switch_address_space(pagedir_t *pd);

/* Invalidate unified TLB and caches */
void mmu_flush_tlb(void);

/* C Abort dispatcher called from assembly vectors */
void data_abort_handler_c(uint32_t fault_addr, uint32_t fsr, uint32_t lr, uint32_t spsr);
void prefetch_abort_handler_c(uint32_t fault_pc, uint32_t lr, uint32_t spsr);

#ifdef __cplusplus
}
#endif

#endif /* MMU_H */
