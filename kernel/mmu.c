#include "mmu.h"
#include <stdint.h>
#include "console.h"

/* 16KB aligned Translation Table Base (TTB) for 4096 * 4-byte entries */
static uint32_t __attribute__((aligned(16384))) page_table[4096];

#define MMU_DESC_TYPE_SECTION 0x2
#define MMU_DESC_CACHEABLE    0x8
#define MMU_DESC_BUFFERABLE   0x4
#define MMU_DESC_AP_RW        (0x3 << 10) /* Read/Write access for Any privilege */
#define MMU_DESC_DOMAIN(x)    ((x) << 5)

void mmu_init(void) {
    int i;
    
    /* 1. Clear translation table (fault all accesses initially) */
    for (i = 0; i < 4096; i++) {
        page_table[i] = 0;
    }
    
    /* 2. Identity Map RAM: 0x00000000 to 0x02000000 (32MB) */
    for (i = 0x000; i < 0x020; i++) {
        uint32_t attrs = MMU_DESC_CACHEABLE | MMU_DESC_BUFFERABLE;
        /* Frontbuffer is at 2MB mark (0x00200000). Make it Non-Cacheable so the
           PL110 controller reads live pixels from RAM instead of hitting stale cache. */
        if (i == 2) attrs = MMU_DESC_BUFFERABLE;
        
        page_table[i] = (i << 20) | MMU_DESC_AP_RW | MMU_DESC_DOMAIN(0) | 
                        attrs | MMU_DESC_TYPE_SECTION;
    }
    
    /* 3. Identity Map Peripherals: 0x10000000 to 0x101FFFFF (2MB for VersatilePB PL110, PL011, etc)
       Set as Non-Cacheable, Non-Bufferable (Strongly Ordered) */
    for (i = 0x100; i < 0x102; i++) {
        page_table[i] = (i << 20) | MMU_DESC_AP_RW | MMU_DESC_DOMAIN(0) | 
                        MMU_DESC_TYPE_SECTION;
    }
    
    /* 4. Configure CP15 */
    uint32_t ttbr = (uint32_t)page_table;
    
    asm volatile (
        /* Invalidate caches and TLBs before enabling */
        "mov r1, #0\n"
        "mcr p15, 0, r1, c7, c7, 0\n" /* Invalidate I/D caches */
        "mcr p15, 0, r1, c8, c7, 0\n" /* Invalidate unified TLB */
        
        /* Set TTBR0 */
        "mcr p15, 0, %0, c2, c0, 0\n"
        
        /* Set Domain Access Control to Client (01) for Domain 0 */
        "ldr r1, =0x00000001\n"
        "mcr p15, 0, r1, c3, c0, 0\n"
        
        /* Enable MMU (M bit 0) and Caches (C bit 2, I bit 12) */
        "mrc p15, 0, r1, c1, c0, 0\n"
        "orr r1, r1, #0x0001\n" /* MMU enable */
        "orr r1, r1, #0x0004\n" /* D-Cache enable */
        "orr r1, r1, #0x1000\n" /* I-Cache enable */
        "mcr p15, 0, r1, c1, c0, 0\n"
        : : "r"(ttbr) : "r1", "memory"
    );
    
    kputs("MMU: Initialized and Caches enabled (32MB RAM, 2MB MMIO mapped).\n");
}
