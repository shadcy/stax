/**
 * @file    mmu.c
 * @author  shadcy
 * @brief   Two-Level 4KB Virtual Memory Paging, Address Space Management, and Fault Handlers.
 *
 * Part of the STAX Operating System.
 *
 * @license GPL-3.0-or-later
 * Copyright (c) 2026 Shreyash Wanjari (Shadcy)
 */

#include "mmu.h"
#include "console.h"
#include "heap.h"
#include "page.h"
#include "scheduler.h"
#include <string.h>

/* Master Kernel Page Directory (aligned to 16KB boundary) */
static pagedir_t master_page_directory __attribute__((aligned(16384)));

/* Descriptor Types for ARMv5 (ARM926EJ-S) */
#define L1_TYPE_FAULT       0x00
#define L1_TYPE_COARSE      0x01
#define L1_TYPE_SECTION     0x02

#define L2_TYPE_FAULT       0x00
#define L2_TYPE_LARGE       0x01
#define L2_TYPE_SMALL       0x02

#define MMU_DESC_CACHEABLE  0x08
#define MMU_DESC_BUFFERABLE 0x04
#define MMU_DESC_DOMAIN(x)  ((x) << 5)
#define MMU_DESC_AP_RW      (0x3 << 10)

/* ============================================================================
 * MMU Hardware Flush & Invalidation Helpers
 * ============================================================================ */

void mmu_flush_tlb(void) {
    asm volatile (
        "mov r0, #0\n"
        "mcr p15, 0, r0, c7, c7, 0\n" /* Invalidate I/D caches */
        "mcr p15, 0, r0, c8, c7, 0\n" /* Invalidate unified TLB */
        "mcr p15, 0, r0, c7, c10, 4\n" /* Data Synchronization Barrier */
        : : : "r0", "memory"
    );
}

void mmu_switch_address_space(pagedir_t *pd) {
    if (!pd) pd = &master_page_directory;
    uint32_t ttbr = (uint32_t)pd;
    asm volatile (
        "mov r1, #0\n"
        "mcr p15, 0, r1, c7, c7, 0\n"  /* Invalidate caches */
        "mcr p15, 0, r1, c8, c7, 0\n"  /* Invalidate TLB */
        "mcr p15, 0, %0, c2, c0, 0\n"  /* Load new TTBR0 */
        "mcr p15, 0, r1, c8, c7, 0\n"  /* Invalidate TLB again */
        "mcr p15, 0, r1, c7, c10, 4\n" /* Drain write buffer */
        : : "r"(ttbr) : "r1", "memory"
    );
}

/* ============================================================================
 * Master MMU Initialization
 * ============================================================================ */

void mmu_init(void) {
    int i;
    
    /* 1. Clear Master Page Directory */
    memset(&master_page_directory, 0, sizeof(pagedir_t));
    
    /* 2. Identity Map 32MB RAM using 1MB Section Descriptors (0x00000000 to 0x02000000) */
    for (i = 0x000; i < 0x020; i++) {
        uint32_t attrs = MMU_DESC_CACHEABLE | MMU_DESC_BUFFERABLE;
        /* Framebuffer region (28MB to 32MB) must be Non-Cacheable + Bufferable */
        if (i >= 28 && i <= 31) {
            attrs = MMU_DESC_BUFFERABLE;
        }
        
        master_page_directory.entries[i] = (i << 20) | MMU_DESC_AP_RW | 
                                            MMU_DESC_DOMAIN(0) | attrs | L1_TYPE_SECTION;
    }
    
    /* 3. Identity Map Peripherals / MMIO (0x10000000 to 0x10200000, 2MB) */
    for (i = 0x100; i < 0x102; i++) {
        master_page_directory.entries[i] = (i << 20) | MMU_DESC_AP_RW | 
                                            MMU_DESC_DOMAIN(0) | L1_TYPE_SECTION;
    }
    
    /* 4. Configure CP15 Control Registers */
    uint32_t ttbr = (uint32_t)&master_page_directory;
    asm volatile (
        "mov r1, #0\n"
        "mcr p15, 0, r1, c7, c7, 0\n" /* Invalidate I/D caches */
        "mcr p15, 0, r1, c8, c7, 0\n" /* Invalidate unified TLB */
        
        /* Set TTBR0 */
        "mcr p15, 0, %0, c2, c0, 0\n"
        
        /* Set Domain Access Control to Client (01) for Domain 0 */
        "ldr r1, =0x00000001\n"
        "mcr p15, 0, r1, c3, c0, 0\n"
        
        /* Enable MMU (M bit 0) and Caches (C bit 2, I bit 12), clear V bit (bit 13) */
        "mrc p15, 0, r1, c1, c0, 0\n"
        "orr r1, r1, #0x0001\n" /* MMU enable */
        "orr r1, r1, #0x0004\n" /* D-Cache enable */
        "orr r1, r1, #0x1000\n" /* I-Cache enable */
        "bic r1, r1, #0x2000\n" /* Clear V bit (Low vectors at 0x00000000) */
        "mcr p15, 0, r1, c1, c0, 0\n"
        : : "r"(ttbr) : "r1", "memory"
    );
    
    kputs("MMU: Two-level 4KB paging enabled (32MB RAM, 2MB MMIO mapped).\n");
}

/* ============================================================================
 * Dynamic Page Mapping & Address Spaces
 * ============================================================================ */

pagedir_t *mmu_create_address_space(void) {
    pagedir_t *pd = (pagedir_t *)kmalloc(sizeof(pagedir_t));
    if (!pd) return NULL;
    
    /* Copy kernel mappings (Sections 0..31 and MMIO 0x100..0x101) */
    memcpy(pd, &master_page_directory, sizeof(pagedir_t));
    return pd;
}

void mmu_destroy_address_space(pagedir_t *pd) {
    if (!pd || pd == &master_page_directory) return;
    
    /* Free dynamically allocated L2 Coarse Page Tables (in user region) */
    for (int i = 0x020; i < 4096; i++) {
        if (i >= 0x100 && i < 0x102) continue; /* Skip MMIO */
        uint32_t entry = pd->entries[i];
        if ((entry & 0x3) == L1_TYPE_COARSE) {
            uint32_t pt_phys = entry & 0xFFFFFC00;
            kfree((void *)pt_phys);
        }
    }
    kfree(pd);
    mmu_flush_tlb();
}

int mmu_map_page(pagedir_t *pd, uint32_t vaddr, uint32_t paddr, uint32_t flags) {
    if (!pd) pd = &master_page_directory;
    
    uint32_t l1_idx = (vaddr >> 20) & 0xFFF;
    uint32_t l2_idx = (vaddr >> 12) & 0xFF;
    
    pagetable_t *pt = NULL;
    uint32_t l1_desc = pd->entries[l1_idx];
    
    if ((l1_desc & 0x3) == L1_TYPE_COARSE) {
        pt = (pagetable_t *)(l1_desc & 0xFFFFFC00);
    } else {
        /* Allocate a new L2 Coarse Page Table (1024 bytes, 1KB aligned) */
        pt = (pagetable_t *)kmalloc(sizeof(pagetable_t));
        if (!pt) return -1;
        memset(pt, 0, sizeof(pagetable_t));
        
        pd->entries[l1_idx] = ((uint32_t)pt & 0xFFFFFC00) | MMU_DESC_DOMAIN(0) | L1_TYPE_COARSE;
    }
    
    /* Configure AP bits for 4KB Small Page */
    uint32_t ap = flags & 0x03;
    uint32_t ap_bits = (ap << 10) | (ap << 8) | (ap << 6) | (ap << 4);
    
    uint32_t cb_bits = 0;
    if (!(flags & MMU_PAGE_NOCACHE)) {
        cb_bits |= MMU_DESC_CACHEABLE;
    }
    if (flags & MMU_PAGE_BUFFERABLE) {
        cb_bits |= MMU_DESC_BUFFERABLE;
    }
    
    pt->entries[l2_idx] = (paddr & PAGE_MASK) | ap_bits | cb_bits | L2_TYPE_SMALL;
    mmu_flush_tlb();
    return 0;
}

int mmu_unmap_page(pagedir_t *pd, uint32_t vaddr) {
    if (!pd) pd = &master_page_directory;
    uint32_t l1_idx = (vaddr >> 20) & 0xFFF;
    uint32_t l2_idx = (vaddr >> 12) & 0xFF;
    
    uint32_t l1_desc = pd->entries[l1_idx];
    if ((l1_desc & 0x3) != L1_TYPE_COARSE) return -1;
    
    pagetable_t *pt = (pagetable_t *)(l1_desc & 0xFFFFFC00);
    pt->entries[l2_idx] = L2_TYPE_FAULT;
    mmu_flush_tlb();
    return 0;
}

uint32_t mmu_get_phys(pagedir_t *pd, uint32_t vaddr) {
    if (!pd) pd = &master_page_directory;
    uint32_t l1_idx = (vaddr >> 20) & 0xFFF;
    uint32_t l2_idx = (vaddr >> 12) & 0xFF;
    uint32_t offset = vaddr & (PAGE_SIZE - 1);
    
    uint32_t l1_desc = pd->entries[l1_idx];
    if ((l1_desc & 0x3) == L1_TYPE_SECTION) {
        return (l1_desc & 0xFFF00000) | (vaddr & 0x000FFFFF);
    } else if ((l1_desc & 0x3) == L1_TYPE_COARSE) {
        pagetable_t *pt = (pagetable_t *)(l1_desc & 0xFFFFFC00);
        uint32_t l2_desc = pt->entries[l2_idx];
        if ((l2_desc & 0x3) == L2_TYPE_SMALL) {
            return (l2_desc & PAGE_MASK) | offset;
        }
    }
    return 0;
}

/* ============================================================================
 * Hardware Abort Handlers (Data Abort & Prefetch Abort)
 * ============================================================================ */

void data_abort_handler_c(uint32_t fault_addr, uint32_t fsr, uint32_t lr, uint32_t spsr) {
    uint32_t mode = spsr & 0x1F;
    kprintf("\n[MMU FAULT] Data Abort\n");
    kprintf("  Fault Address (FAR): 0x%x\n", fault_addr);
    kprintf("  Fault Status  (FSR): 0x%x\n", fsr);
    kprintf("  Faulting PC   (LR) : 0x%x\n", lr);
    kprintf("  CPU Mode      (SPSR): 0x%x (%s)\n", spsr, (mode == 0x10) ? "USR" : "SVC");
    
    if (mode == 0x10) {
        /* If fault occurred in User space, terminate user task safely */
        kprintf("[MMU] Terminating User Task (PID %d)\n", current_task ? current_task->state : 0);
        task_exit();
    } else {
        /* Kernel space fault: halt system */
        kprintf("[MMU FATAL] Kernel Page Fault! System halted.\n");
        while (1);
    }
}

void prefetch_abort_handler_c(uint32_t fault_pc, uint32_t lr, uint32_t spsr) {
    (void)lr;
    uint32_t mode = spsr & 0x1F;
    kprintf("\n[MMU FAULT] Prefetch Abort (Bad Instruction Fetch)\n");
    kprintf("  Faulting PC  : 0x%x\n", fault_pc);
    kprintf("  CPU Mode     : 0x%x (%s)\n", spsr, (mode == 0x10) ? "USR" : "SVC");
    
    if (mode == 0x10) {
        kprintf("[MMU] Terminating User Task (PID %d)\n", current_task ? current_task->state : 0);
        task_exit();
    } else {
        kprintf("[MMU FATAL] Kernel Prefetch Fault! System halted.\n");
        while (1);
    }
}
