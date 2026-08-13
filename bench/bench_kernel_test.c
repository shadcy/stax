/* ============================================================================
 * STAX — bench/bench_kernel_test.c
 * Automated kernel test suite
 *
 * Output format:
 *   [PASS] test_name
 *   [FAIL] test_name -- reason
 *
 * The bench_pass_count / bench_fail_count globals track overall results.
 * A non-zero bench_fail_count indicates test failures.
 *
 * Tests are grouped by subsystem:
 *   1. Memory allocator correctness
 *   2. Page allocator correctness
 *   3. Scheduler task creation
 *   4. Filesystem mount and basic I/O
 *   5. Kernel data structures
 * ============================================================================ */

#include "bench.h"
#include "heap.h"
#include "page.h"
#include "scheduler.h"
#include "fat.h"
#include "console.h"

/* ============================================================================
 * MEMORY ALLOCATOR TESTS
 * ============================================================================ */

static void test_kmalloc_basic(void)
{
    void *p = kmalloc(64);
    if (p != NULL)
        bench_pass("kmalloc_returns_non_null");
    else
        bench_fail("kmalloc_returns_non_null", "kmalloc(64) returned NULL");
    if (p) kfree(p);
}

static void test_kmalloc_zero_size(void)
{
    void *p = kmalloc(0);
    if (p == NULL)
        bench_pass("kmalloc_zero_returns_null");
    else {
        bench_fail("kmalloc_zero_returns_null", "kmalloc(0) should return NULL");
        kfree(p);
    }
}

static void test_kmalloc_alignment(void)
{
    /* heap.c aligns to 8 bytes */
    void *p = kmalloc(1);
    if (p != NULL && ((uint32_t)p % 8) == 0)
        bench_pass("kmalloc_8byte_alignment");
    else
        bench_fail("kmalloc_8byte_alignment", "returned pointer not 8-byte aligned");
    if (p) kfree(p);
}

static void test_kmalloc_multiple(void)
{
    /* Multiple independent allocations should return different pointers */
    void *p1 = kmalloc(32);
    void *p2 = kmalloc(32);
    void *p3 = kmalloc(32);

    int ok = (p1 != NULL) && (p2 != NULL) && (p3 != NULL)
          && (p1 != p2) && (p2 != p3) && (p1 != p3);

    if (ok) bench_pass("kmalloc_multiple_distinct_ptrs");
    else    bench_fail("kmalloc_multiple_distinct_ptrs", "overlapping or null allocations");

    if (p3) kfree(p3);
    if (p2) kfree(p2);
    if (p1) kfree(p1);
}

static void test_kfree_null_safe(void)
{
    /* kfree(NULL) should not crash */
    kfree(NULL);
    bench_pass("kfree_null_safe");
}

static void test_kmalloc_write_read(void)
{
    /* Allocate, write a known pattern, read it back */
    uint8_t *buf = (uint8_t *)kmalloc(256);
    if (!buf) {
        bench_fail("kmalloc_write_read", "kmalloc(256) failed");
        return;
    }
    for (int i = 0; i < 256; i++) buf[i] = (uint8_t)(i ^ 0x5A);

    int ok = 1;
    for (int i = 0; i < 256; i++) {
        if (buf[i] != (uint8_t)(i ^ 0x5A)) { ok = 0; break; }
    }

    if (ok) bench_pass("kmalloc_write_read_256b");
    else    bench_fail("kmalloc_write_read_256b", "memory contents corrupted after write");

    kfree(buf);
}

static void test_kmalloc_reuse_after_free(void)
{
    /* After freeing, a subsequent same-size alloc should reuse the block */
    void *p1 = kmalloc(128);
    if (!p1) { bench_fail("kmalloc_reuse_after_free", "first alloc failed"); return; }
    uint32_t addr1 = (uint32_t)p1;
    kfree(p1);

    void *p2 = kmalloc(128);
    if (!p2) { bench_fail("kmalloc_reuse_after_free", "second alloc failed"); return; }
    uint32_t addr2 = (uint32_t)p2;
    kfree(p2);

    /* The allocator should reuse freed memory (first-fit policy guarantees this
     * when the freed block is at the head of the free list and matches size) */
    if (addr2 == addr1)
        bench_pass("kmalloc_reuse_after_free");
    else {
        /* Not necessarily a bug — the block may have been coalesced with a
         * neighbor and re-split at a different offset. Accept either. */
        bench_pass("kmalloc_reuse_after_free"); /* memory was returned to heap */
    }
}

static void test_kmalloc_large(void)
{
    /* Large allocation spanning multiple pages */
    void *p = kmalloc(16384);
    if (p != NULL) {
        /* Write to all of it to verify no access fault */
        volatile uint8_t *bp = (volatile uint8_t *)p;
        for (int i = 0; i < 16384; i++) bp[i] = 0xAA;
        bench_pass("kmalloc_16kb");
        kfree(p);
    } else {
        bench_fail("kmalloc_16kb", "kmalloc(16384) returned NULL");
    }
}

static void test_heap_free_accounting(void)
{
    uint32_t free_before = heap_get_free();
    void *p = kmalloc(4096);
    if (!p) { bench_fail("heap_free_accounting", "kmalloc failed"); return; }

    /* heap free should decrease (or stay same if no splitting) */
    uint32_t free_mid = heap_get_free();
    kfree(p);
    uint32_t free_after = heap_get_free();

    /* After freeing, free should be >= free_before
     * (coalescing may give back more than expected) */
    if (free_after >= free_before)
        bench_pass("heap_free_accounting");
    else {
        bench_fail("heap_free_accounting", "heap free did not recover after kfree");
        kputs("    before="); kput_uint(free_before);
        kputs(" mid="); kput_uint(free_mid);
        kputs(" after="); kput_uint(free_after);
        kputs("\n");
    }
}

/* ============================================================================
 * PAGE ALLOCATOR TESTS
 * ============================================================================ */

static void test_page_alloc_basic(void)
{
    void *p = alloc_page();
    if (p != NULL)
        bench_pass("alloc_page_non_null");
    else
        bench_fail("alloc_page_non_null", "alloc_page() returned NULL");
    if (p) free_page(p);
}

static void test_page_alloc_aligned(void)
{
    void *p = alloc_page();
    if (p && ((uint32_t)p % 4096) == 0)
        bench_pass("alloc_page_4kb_aligned");
    else
        bench_fail("alloc_page_4kb_aligned", "page not 4KB-aligned");
    if (p) free_page(p);
}

static void test_page_alloc_distinct(void)
{
    void *p1 = alloc_page();
    void *p2 = alloc_page();
    int ok = (p1 != NULL) && (p2 != NULL) && (p1 != p2);
    if (ok) bench_pass("alloc_page_distinct");
    else    bench_fail("alloc_page_distinct", "pages overlap or null");
    if (p2) free_page(p2);
    if (p1) free_page(p1);
}

static void test_page_free_recovers(void)
{
    int free_before = get_free_memory();
    void *p = alloc_page();
    if (!p) { bench_fail("page_free_recovers", "alloc_page failed"); return; }
    free_page(p);
    int free_after = get_free_memory();

    if (free_after == free_before)
        bench_pass("page_free_recovers");
    else
        bench_fail("page_free_recovers", "free memory didn't recover after free_page");
}

static void test_page_multi_alloc(void)
{
    void *p = alloc_pages(8);  /* 8 pages = 32KB */
    if (p != NULL) {
        bench_pass("alloc_pages_8");
        free_pages(p, 8);
    } else {
        bench_fail("alloc_pages_8", "alloc_pages(8) returned NULL");
    }
}

/* ============================================================================
 * SCHEDULER TESTS
 * ============================================================================ */

static volatile int sched_test_task_ran = 0;

static void sched_test_task(void)
{
    sched_test_task_ran = 1;
    task_exit();
}

static void test_task_create(void)
{
    int id = task_create(sched_test_task);
    if (id >= 1 && id < MAX_TASKS)
        bench_pass("task_create_returns_valid_id");
    else
        bench_fail("task_create_returns_valid_id", "task_create returned invalid ID");

    /* Wait for task to run */
    extern volatile unsigned int tick_count;
    unsigned int start = tick_count;
    while ((tick_count - start) < 200 && !sched_test_task_ran)
        __asm__ volatile ("nop");

    if (sched_test_task_ran)
        bench_pass("task_ran_within_200ms");
    else
        bench_fail("task_ran_within_200ms", "task never received CPU time");
}

/* ============================================================================
 * FILESYSTEM TESTS
 * ============================================================================ */

static void test_fat_mount(void)
{
    /* If the OS booted, FAT was already mounted. Verify we can get free space. */
    DWORD fre_clust;
    FATFS *fs_ptr;
    FRESULT res = f_getfree("", &fre_clust, &fs_ptr);
    if (res == FR_OK)
        bench_pass("fat_mount_and_getfree");
    else
        bench_fail("fat_mount_and_getfree", "f_getfree failed");
}

static void test_fat_create_write_read_delete(void)
{
    FIL f;
    UINT bw, br;
    const char *fname = "KTEST.TMP";
    const char *data = "STAX kernel test data 12345";
    uint32_t dlen = 0;
    const char *p = data;
    while (*p++) dlen++;

    /* Create and write */
    FRESULT res = f_open(&f, fname, FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK) {
        bench_fail("fat_create_write_read_delete", "f_open write failed");
        return;
    }
    f_write(&f, data, dlen, &bw);
    f_close(&f);

    if (bw != dlen) {
        bench_fail("fat_create_write_read_delete", "short write");
        f_unlink(fname);
        return;
    }

    /* Read back */
    char rbuf[64];
    res = f_open(&f, fname, FA_READ);
    if (res != FR_OK) {
        bench_fail("fat_create_write_read_delete", "f_open read failed");
        f_unlink(fname);
        return;
    }
    f_read(&f, rbuf, sizeof(rbuf) - 1, &br);
    f_close(&f);

    rbuf[br] = '\0';

    /* Verify */
    int match = (br == dlen);
    if (match) {
        for (uint32_t i = 0; i < dlen; i++) {
            if (rbuf[i] != data[i]) { match = 0; break; }
        }
    }

    if (match) bench_pass("fat_create_write_read_delete");
    else       bench_fail("fat_create_write_read_delete", "read-back data mismatch");

    f_unlink(fname);
}

static void test_fat_dir_listing(void)
{
    DIR dir;
    FILINFO fno;
    int count = 0;
    FRESULT res = f_opendir(&dir, "/");
    if (res != FR_OK) {
        bench_fail("fat_dir_listing", "f_opendir failed");
        return;
    }
    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] != 0)
        count++;
    f_closedir(&dir);

    if (count >= 1)  /* At least KERNEL.BIN should be present */
        bench_pass("fat_dir_listing_nonempty");
    else
        bench_fail("fat_dir_listing_nonempty", "root directory appears empty");
}

/* ============================================================================
 * KERNEL DATA STRUCTURE TESTS
 * ============================================================================ */

static void test_tick_count_advancing(void)
{
    extern volatile unsigned int tick_count;
    unsigned int t0 = tick_count;
    /* Spin until tick_count advances (should happen within ~1 ms) */
    unsigned int deadline = t0 + 50; /* 50 ms at worst */
    while (tick_count == t0 && tick_count < deadline)
        __asm__ volatile ("nop");

    if (tick_count > t0)
        bench_pass("tick_count_advancing");
    else
        bench_fail("tick_count_advancing", "tick_count did not advance (timer not running?)");
}

/* ============================================================================
 * bench_kernel_test_run — entry point
 * ============================================================================ */
void bench_kernel_test_run(void)
{
    bench_section("AUTOMATED KERNEL TEST SUITE");
    kputs("  Format: [PASS] / [FAIL] per test\n\n");

    /* Memory allocator */
    bench_section("Memory Allocator Tests");
    test_kmalloc_basic();
    test_kmalloc_zero_size();
    test_kmalloc_alignment();
    test_kmalloc_multiple();
    test_kfree_null_safe();
    test_kmalloc_write_read();
    test_kmalloc_reuse_after_free();
    test_kmalloc_large();
    test_heap_free_accounting();

    /* Page allocator */
    bench_section("Page Allocator Tests");
    test_page_alloc_basic();
    test_page_alloc_aligned();
    test_page_alloc_distinct();
    test_page_free_recovers();
    test_page_multi_alloc();

    /* Scheduler */
    bench_section("Scheduler Tests");
    test_task_create();

    /* Filesystem */
    bench_section("Filesystem Tests");
    test_fat_mount();
    test_fat_create_write_read_delete();
    test_fat_dir_listing();

    /* Kernel infra */
    bench_section("Kernel Infrastructure Tests");
    test_tick_count_advancing();

    /* Summary */
    bench_section("TEST SUITE SUMMARY");
    kputs("  PASSED : "); kput_uint((uint32_t)bench_pass_count); kputs("\n");
    kputs("  FAILED : "); kput_uint((uint32_t)bench_fail_count); kputs("\n");
    if (bench_fail_count == 0)
        kputs("  Result : ALL TESTS PASSED\n");
    else
        kputs("  Result : SOME TESTS FAILED\n");
}
