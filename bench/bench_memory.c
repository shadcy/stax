/* ============================================================================
 * STAX — bench/bench_memory.c
 * kmalloc / kfree / page allocator benchmarks
 *
 * Measures:
 *   - Allocation latency per size class
 *   - Free latency per size class
 *   - Round-trip (alloc+free) throughput
 *   - Fragmentation metric
 *   - Randomized stress (alloc/free pattern)
 *   - Leak detection via page allocator free count
 * ============================================================================ */

#include "bench.h"
#include "heap.h"
#include "page.h"
#include "console.h"

/* ---- Size classes to benchmark ---- */
#define NUM_SIZES 8
static const uint32_t test_sizes[NUM_SIZES] = {
    16, 32, 64, 128, 256, 512, 1024, 4096
};
static const char *size_names[NUM_SIZES] = {
    "16b", "32b", "64b", "128b", "256b", "512b", "1kb", "4kb"
};

/* ---- Number of iterations per size ---- */
#define ALLOC_ITERS  256

/* ---- Max simultaneous live allocations for fragmentation test ---- */
#define FRAG_SLOTS   32

/* ============================================================================
 * Pseudo-random number generator (LCG — deterministic, reproducible)
 * ============================================================================ */
static uint32_t rng_state = 0xDEADBEEFUL;

static uint32_t rng_next(void)
{
    rng_state = rng_state * 1664525UL + 1013904223UL;
    return rng_state;
}

/* ============================================================================
 * Walk the heap free list to find the largest contiguous free block.
 * This lets us compute: fragmentation = 1 - (largest_free / total_free)
 *
 * We access block_t directly since heap.h exposes the struct.
 * ============================================================================ */
static uint32_t heap_largest_free_block(void)
{
    /* We cannot access free_list directly (it is static in heap.c).
     * Instead we perform a proxy measurement:
     * try to kmalloc increasingly large sizes until failure.
     * The largest successful size is a lower bound on the largest free block. */
    static const uint32_t probe_sizes[] = {
        4096, 8192, 16384, 32768, 65536, 131072, 0
    };
    uint32_t largest = 0;
    for (int i = 0; probe_sizes[i] != 0; i++) {
        void *p = kmalloc(probe_sizes[i]);
        if (p) {
            if (probe_sizes[i] > largest) largest = probe_sizes[i];
            kfree(p);
        }
    }
    return largest;
}

/* ============================================================================
 * run_alloc_latency — benchmark allocation latency for a single size class
 * ============================================================================ */
static void run_alloc_latency(uint32_t size, const char *name)
{
    bench_result_t result;
    static uint32_t samples[ALLOC_ITERS];
    void *ptrs[ALLOC_ITERS];

    /* Pre-warm: allocate all, then free all so the heap is exercised */
    for (int w = 0; w < 4; w++) {
        void *tmp = kmalloc(size);
        if (tmp) kfree(tmp);
    }

    /* Measure individual alloc cost */
    for (uint32_t i = 0; i < ALLOC_ITERS; i++) {
        bench_timer_reset();
        ptrs[i] = kmalloc(size);
        samples[i] = bench_timer_read_us();
        if (!ptrs[i]) {
            /* Out of memory — stop early, free what we have */
            for (uint32_t j = 0; j < i; j++) kfree(ptrs[j]);
            kputs("  [WARN] OOM at iter ");
            kput_uint(i);
            kputs(" for size ");
            kput_uint(size);
            kputs("\n");
            return;
        }
    }
    /* Free all allocations */
    for (uint32_t i = 0; i < ALLOC_ITERS; i++) kfree(ptrs[i]);

    /* Build result name */
    char rname[32];
    int ri = 0;
    const char *prefix = "kmalloc_";
    while (*prefix) rname[ri++] = *prefix++;
    const char *n = name;
    while (*n) rname[ri++] = *n++;
    rname[ri] = '\0';

    result.name = rname;
    result.unit = "us";
    bench_compute(&result, samples, ALLOC_ITERS);
    bench_report(&result);
}

/* ============================================================================
 * run_free_latency — benchmark free latency for a single size class
 * ============================================================================ */
static void run_free_latency(uint32_t size, const char *name)
{
    bench_result_t result;
    static uint32_t samples[ALLOC_ITERS];
    void *ptrs[ALLOC_ITERS];

    /* Allocate all first */
    for (uint32_t i = 0; i < ALLOC_ITERS; i++) {
        ptrs[i] = kmalloc(size);
        if (!ptrs[i]) {
            /* Free what we have and abort */
            for (uint32_t j = 0; j < i; j++) kfree(ptrs[j]);
            return;
        }
    }

    /* Measure individual free cost */
    for (uint32_t i = 0; i < ALLOC_ITERS; i++) {
        bench_timer_reset();
        kfree(ptrs[i]);
        samples[i] = bench_timer_read_us();
        ptrs[i] = NULL;
    }

    char rname[32];
    int ri = 0;
    const char *prefix = "kfree_";
    while (*prefix) rname[ri++] = *prefix++;
    const char *n = name;
    while (*n) rname[ri++] = *n++;
    rname[ri] = '\0';

    result.name = rname;
    result.unit = "us";
    bench_compute(&result, samples, ALLOC_ITERS);
    bench_report(&result);
}

/* ============================================================================
 * run_roundtrip_throughput — alloc+free in a tight loop, report ops/sec
 * ============================================================================ */
static void run_roundtrip_throughput(uint32_t size, const char *name)
{
    bench_result_t result;
    static uint32_t samples[ALLOC_ITERS];

    for (uint32_t i = 0; i < ALLOC_ITERS; i++) {
        bench_timer_reset();
        void *p = kmalloc(size);
        kfree(p);
        samples[i] = bench_timer_read_us();
    }

    char rname[32];
    int ri = 0;
    const char *prefix = "kmalloc_kfree_";
    while (*prefix) rname[ri++] = *prefix++;
    const char *n = name;
    while (*n) rname[ri++] = *n++;
    rname[ri] = '\0';

    result.name = rname;
    result.unit = "us";
    bench_compute(&result, samples, ALLOC_ITERS);
    bench_report(&result);
}

/* ============================================================================
 * run_page_alloc_latency — benchmark alloc_page / free_page
 * ============================================================================ */
static void run_page_alloc_latency(void)
{
    bench_result_t result;
    static uint32_t samples[128];
    void *ptrs[128];

    for (int i = 0; i < 128; i++) {
        bench_timer_reset();
        ptrs[i] = alloc_page();
        samples[i] = bench_timer_read_us();
        if (!ptrs[i]) {
            for (int j = 0; j < i; j++) if (ptrs[j]) free_page(ptrs[j]);
            kputs("  [WARN] page alloc OOM at iter ");
            kput_uint(i);
            kputs("\n");
            return;
        }
    }
    for (int i = 0; i < 128; i++) free_page(ptrs[i]);

    result.name = "alloc_page";
    result.unit = "us";
    bench_compute(&result, samples, 128);
    bench_report(&result);
}

static void run_page_free_latency(void)
{
    bench_result_t result;
    static uint32_t samples[128];
    void *ptrs[128];

    for (int i = 0; i < 128; i++) {
        ptrs[i] = alloc_page();
        if (!ptrs[i]) { return; }
    }
    for (int i = 0; i < 128; i++) {
        bench_timer_reset();
        free_page(ptrs[i]);
        samples[i] = bench_timer_read_us();
        ptrs[i] = NULL;
    }

    result.name = "free_page";
    result.unit = "us";
    bench_compute(&result, samples, 128);
    bench_report(&result);
}

/* ============================================================================
 * run_fragmentation — measure heap fragmentation after random alloc/free
 *
 * Definition used:
 *   fragmentation = 1 - (largest_free_block / total_heap_free)
 * Range: 0 = no fragmentation, 1 = fully fragmented (no usable block)
 * ============================================================================ */
static void run_fragmentation(void)
{
    void *ptrs[FRAG_SLOTS];
    uint32_t sizes[FRAG_SLOTS];
    int live[FRAG_SLOTS];
    rng_state = 0xC0FFEE42UL;

    for (int i = 0; i < FRAG_SLOTS; i++) {
        ptrs[i] = NULL;
        live[i] = 0;
    }

    /* Perform 500 random alloc/free operations */
    for (int op = 0; op < 500; op++) {
        uint32_t r = rng_next();
        int slot = (int)(r % FRAG_SLOTS);

        if (live[slot]) {
            kfree(ptrs[slot]);
            ptrs[slot] = NULL;
            live[slot] = 0;
        } else {
            /* Random size: 16–1024 bytes */
            uint32_t sz = 16 + (rng_next() % 1009);
            ptrs[slot] = kmalloc(sz);
            if (ptrs[slot]) {
                sizes[slot] = sz;
                live[slot] = 1;
            }
        }
    }

    /* Measure fragmentation */
    uint32_t total_free = heap_get_free();
    uint32_t largest    = heap_largest_free_block();

    /* Free remaining live allocations */
    for (int i = 0; i < FRAG_SLOTS; i++) {
        if (live[i]) { kfree(ptrs[i]); live[i] = 0; }
    }
    (void)sizes; /* suppress unused warning */

    kputs("  [BENCH] fragmentation:\n");
    kputs("    total_free=");
    char buf[24];
    bench_fmt_uint(total_free, buf, sizeof(buf));
    kputs(buf);
    kputs(" bytes\n");

    kputs("    largest_free_block>=");
    bench_fmt_uint(largest, buf, sizeof(buf));
    kputs(buf);
    kputs(" bytes\n");

    if (total_free > 0) {
        /* fragmentation % = (1 - largest/total) * 100 */
        uint32_t frag_pct = (total_free > largest)
            ? ((total_free - largest) * 100) / total_free
            : 0;
        kputs("    fragmentation<=");
        kput_uint(frag_pct);
        kputs("%\n");
        kputs("BENCH:fragmentation,1,");
        kput_uint(frag_pct);
        kputs(",");
        kput_uint(frag_pct);
        kputs(",");
        kput_uint(frag_pct);
        kputs(",");
        kput_uint(frag_pct);
        kputs(",0\n");
    } else {
        kputs("    total_free=0 (heap exhausted)\n");
    }
}

/* ============================================================================
 * run_stress_alloc — randomized stress test with canary verification
 *
 * Strategy:
 *   Allocate buffers, write canary pattern, free in random order.
 *   Verify canary before free. Check page allocator free count.
 * ============================================================================ */
#define STRESS_SLOTS   16
#define CANARY_PATTERN 0xA5A5A5A5UL

static int run_stress_alloc(int iterations)
{
    void    *ptrs[STRESS_SLOTS];
    uint32_t szs[STRESS_SLOTS];
    int      live[STRESS_SLOTS];
    int      errors = 0;
    rng_state = 0xFEEDF00DUL;

    for (int i = 0; i < STRESS_SLOTS; i++) { ptrs[i] = NULL; live[i] = 0; }

    int free_before = get_free_memory();

    for (int op = 0; op < iterations; op++) {
        uint32_t r = rng_next();
        int slot = (int)(r % STRESS_SLOTS);

        if (live[slot]) {
            /* Verify canary before freeing */
            volatile uint32_t *p = (volatile uint32_t *)ptrs[slot];
            uint32_t nwords = szs[slot] / 4;
            for (uint32_t w = 0; w < nwords; w++) {
                if (p[w] != CANARY_PATTERN) {
                    errors++;
                    break;
                }
            }
            kfree(ptrs[slot]);
            ptrs[slot] = NULL;
            live[slot] = 0;
        } else {
            uint32_t sz = 16 + ((rng_next() % 4) * 64); /* 16, 80, 144, 208 bytes */
            sz &= ~3UL; /* word-align */
            ptrs[slot] = kmalloc(sz);
            if (ptrs[slot]) {
                szs[slot] = sz;
                live[slot] = 1;
                /* Write canary */
                volatile uint32_t *p = (volatile uint32_t *)ptrs[slot];
                for (uint32_t w = 0; w < sz / 4; w++) p[w] = CANARY_PATTERN;
            }
        }
    }

    /* Free survivors */
    for (int i = 0; i < STRESS_SLOTS; i++) {
        if (live[i]) {
            volatile uint32_t *p = (volatile uint32_t *)ptrs[i];
            for (uint32_t w = 0; w < szs[i] / 4; w++) {
                if (p[w] != CANARY_PATTERN) { errors++; break; }
            }
            kfree(ptrs[i]); live[i] = 0;
        }
    }

    int free_after = get_free_memory();

    kputs("  [BENCH] stress_alloc_");
    kput_uint((uint32_t)iterations);
    kputs(":\n");
    kputs("    memory_errors=");
    kput_uint((uint32_t)errors);
    kputs("\n");
    kputs("    free_before=");
    kput_uint((uint32_t)free_before);
    kputs(" free_after=");
    kput_uint((uint32_t)free_after);
    kputs(" leak=");
    kput_uint((uint32_t)(free_before - free_after));
    kputs(" bytes\n");

    return errors;
}

/* ============================================================================
 * bench_memory_run — run all memory benchmarks
 * ============================================================================ */
void bench_memory_run(void)
{
    bench_section("MEMORY ALLOCATOR BENCHMARKS");
    bench_timer_init();

    kputs("Benchmarking kmalloc latency by size class...\n");
    for (int i = 0; i < NUM_SIZES; i++)
        run_alloc_latency(test_sizes[i], size_names[i]);

    kputs("\nBenchmarking kfree latency by size class...\n");
    for (int i = 0; i < NUM_SIZES; i++)
        run_free_latency(test_sizes[i], size_names[i]);

    kputs("\nBenchmarking alloc+free round-trip throughput...\n");
    for (int i = 0; i < NUM_SIZES; i++)
        run_roundtrip_throughput(test_sizes[i], size_names[i]);

    bench_section("PAGE ALLOCATOR BENCHMARKS");
    kputs("Benchmarking alloc_page latency...\n");
    run_page_alloc_latency();
    kputs("Benchmarking free_page latency...\n");
    run_page_free_latency();

    bench_section("HEAP FRAGMENTATION");
    kputs("Definition: frag = 1 - (largest_free_block / total_free)\n");
    kputs("After 500 random alloc/free operations (16-1024 bytes):\n");
    run_fragmentation();

    bench_section("MEMORY STRESS TESTS");
    kputs("Running 1,000 iteration randomized alloc/free stress test...\n");
    int e1 = run_stress_alloc(1000);
    if (e1 == 0) bench_pass("stress_alloc_1000_no_corruption");
    else         bench_fail("stress_alloc_1000_no_corruption", "canary overwrite detected");

    kputs("Running 5,000 iteration randomized alloc/free stress test...\n");
    int e2 = run_stress_alloc(5000);
    if (e2 == 0) bench_pass("stress_alloc_5000_no_corruption");
    else         bench_fail("stress_alloc_5000_no_corruption", "canary overwrite detected");
}
