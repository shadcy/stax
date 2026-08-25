/* ============================================================================
 * STAX — bench/bench_vm.c
 * Virtual memory / page allocator / MMU benchmarks and documentation
 *
 * What IS implemented:
 *   - 1-level MMU (section-granularity, 1MB sections)
 *   - Bitmap page allocator (4KB pages, 30MB pool)
 *   - Identity-mapped kernel address space
 *
 * What is NOT implemented (documented honestly):
 *   - Per-process page tables
 *   - Page fault handling
 *   - Demand paging / copy-on-write
 *   - User-mode virtual address space
 *   - Page-table walk latency (no L2 table)
 * ============================================================================ */

#include "bench.h"
#include "page.h"
#include "console.h"

#define PAGE_SIZE_BYTES 4096

/* ============================================================================
 * bench_vm_print_layout — document the static MMU layout
 * ============================================================================ */
static void bench_vm_print_layout(void)
{
    bench_section("MMU / VIRTUAL MEMORY LAYOUT");

    kputs("  Architecture : ARM926EJ-S (ARMv5TE)\n");
    kputs("  Page size    : 1 MB sections (L1 only, no L2)\n");
    kputs("  Table size   : 4096 entries x 4 bytes = 16 KB\n");
    kputs("  Table align  : 16 KB (CP15 TTBR0 requirement)\n");
    kputs("  Cache        : D-cache + I-cache enabled\n\n");

    kputs("  Virtual Address Layout (identity-mapped):\n");
    kputs("  +-----------------------+-------------------------------+\n");
    kputs("  | VA Range              | Purpose                       |\n");
    kputs("  +-----------------------+-------------------------------+\n");
    kputs("  | 0x00000000-0x0000FFFF | Exception vectors + SSRAM     |\n");
    kputs("  | 0x00010000-0x000FFFFF | Bootloader + free             |\n");
    kputs("  | 0x00100000-0x01DFFFFF | Kernel code+data+heap (30MB)  |\n");
    kputs("  | 0x01C00000-0x01EFFFFF | Front framebuffer (1MB)       |\n");
    kputs("  | 0x01E00000-0x01FFFFFF | Back framebuffer (1MB)        |\n");
    kputs("  | 0x10000000-0x101FFFFF | MMIO peripherals (2MB)        |\n");
    kputs("  | all else              | Fault (unmapped)              |\n");
    kputs("  +-----------------------+-------------------------------+\n\n");

    kputs("  Framebuffer sections: Non-Cacheable+Bufferable\n");
    kputs("    (prevents cache thrashing during fb_swap)\n");
    kputs("  MMIO sections: Strongly Ordered (no cache, no buffer)\n");
    kputs("  RAM sections:  Cacheable + Bufferable\n\n");

    kputs("  Page table overhead:\n");
    kputs("    L1 table : 16,384 bytes\n");
    kputs("    L2 table : NONE (section-only mapping)\n");
    kputs("    Bitmap   : 1,024 bytes (8192 page bits)\n");
    kputs("    Total    : 17,408 bytes (~17 KB)\n\n");

    kputs("  NOT IMPLEMENTED:\n");
    kputs("    - Per-process page tables\n");
    kputs("    - Page fault handler (data_handler is stub)\n");
    kputs("    - Demand paging / copy-on-write\n");
    kputs("    - User-mode address space separation\n");
    kputs("    - TLB shootdown / ASID management\n");
}

/* ============================================================================
 * bench_page_alloc_single — latency for alloc_page() / free_page()
 * ============================================================================ */
static void bench_page_single(void)
{
    bench_result_t result;
    static uint32_t samples[256];
    void *ptrs[256];

    bench_section("PAGE ALLOCATOR LATENCY (4KB pages)");
    bench_timer_init();

    /* alloc_page latency */
    for (int i = 0; i < 256; i++) {
        bench_timer_reset();
        ptrs[i] = alloc_page();
        samples[i] = bench_timer_read_us();
        if (!ptrs[i]) {
            kputs("  [WARN] page OOM at iter ");
            kput_uint((uint32_t)i);
            kputs("\n");
            for (int j = 0; j < i; j++) if(ptrs[j]) free_page(ptrs[j]);
            return;
        }
    }
    for (int i = 0; i < 256; i++) free_page(ptrs[i]);
    result.name = "alloc_page_4KB";
    result.unit = "us";
    bench_compute(&result, samples, 256);
    bench_report(&result);

    /* free_page latency */
    for (int i = 0; i < 256; i++) {
        ptrs[i] = alloc_page();
        if (!ptrs[i]) return;
    }
    for (int i = 0; i < 256; i++) {
        bench_timer_reset();
        free_page(ptrs[i]);
        samples[i] = bench_timer_read_us();
        ptrs[i] = NULL;
    }
    result.name = "free_page_4KB";
    result.unit = "us";
    bench_compute(&result, samples, 256);
    bench_report(&result);
}

/* ============================================================================
 * bench_page_multi — latency for alloc_pages(N) with N > 1
 * ============================================================================ */
static void bench_page_multi(void)
{
    bench_result_t result;
    static uint32_t samples[64];
    void *ptrs[64];
    const int counts[] = {1, 4, 16, 64, 256};
    const char *cnames[] = {"alloc_pages_1p", "alloc_pages_4p",
                             "alloc_pages_16p", "alloc_pages_64p",
                             "alloc_pages_256p"};
    const int NPAGES = 5;

    bench_section("MULTI-PAGE ALLOCATION LATENCY");

    for (int ci = 0; ci < NPAGES; ci++) {
        int cnt = counts[ci];
        int iters = 64 / cnt;
        if (iters < 4) iters = 4;

        for (int i = 0; i < iters; i++) {
            bench_timer_reset();
            ptrs[i] = alloc_pages(cnt);
            samples[i] = bench_timer_read_us();
            if (!ptrs[i]) {
                for (int j = 0; j < i; j++) if(ptrs[j]) free_pages(ptrs[j], cnt);
                kputs("  [WARN] multi-page OOM for N=");
                kput_uint((uint32_t)cnt);
                kputs("\n");
                goto next_cnt;
            }
        }
        for (int i = 0; i < iters; i++) {
            if (ptrs[i]) free_pages(ptrs[i], cnt);
        }
        result.name = cnames[ci];
        result.unit = "us";
        bench_compute(&result, samples, (uint32_t)iters);
        bench_report(&result);
next_cnt:;
    }
}

/* ============================================================================
 * bench_vm_memory_report — print current page allocator state
 * ============================================================================ */
static void bench_vm_memory_report(void)
{
    bench_section("PAGE ALLOCATOR STATE");

    int free_mem  = get_free_memory();
    int total_mem = get_total_memory();
    int used_mem  = total_mem - free_mem;

    kputs("  Total managed : ");
    kput_uint((uint32_t)total_mem);
    kputs(" bytes (");
    kput_uint((uint32_t)(total_mem / 1024));
    kputs(" KB)\n");

    kputs("  Used          : ");
    kput_uint((uint32_t)used_mem);
    kputs(" bytes (");
    kput_uint((uint32_t)(used_mem / 1024));
    kputs(" KB)\n");

    kputs("  Free          : ");
    kput_uint((uint32_t)free_mem);
    kputs(" bytes (");
    kput_uint((uint32_t)(free_mem / 1024));
    kputs(" KB)\n");

    if (total_mem > 0) {
        uint32_t util_pct = (uint32_t)(((uint64_t)used_mem * 100) / (uint64_t)total_mem);
        kputs("  Utilization   : ");
        kput_uint(util_pct);
        kputs("%\n");
    }

    kputs("BENCH:page_free_kb,1,");
    kput_uint((uint32_t)(free_mem / 1024));
    kputs(",");
    kput_uint((uint32_t)(free_mem / 1024));
    kputs(",");
    kput_uint((uint32_t)(free_mem / 1024));
    kputs(",");
    kput_uint((uint32_t)(free_mem / 1024));
    kputs(",0\n");
}

/* ============================================================================
 * bench_vm_run — entry point
 * ============================================================================ */
void bench_vm_run(void)
{
    bench_vm_print_layout();
    bench_vm_memory_report();
    bench_page_single();
    bench_page_multi();
}
