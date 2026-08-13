/* ============================================================================
 * STAX — bench/bench_main.c
 * Unified benchmark dispatcher
 *
 * Called from shell/command.c via cmd_bench() / cmd_stress() / cmd_test()
 *
 * Sub-benchmark entry points declared here.
 * ============================================================================ */

#include "bench.h"
#include "console.h"

/* Declarations of sub-benchmark entry points */
void bench_memory_run(void);
void bench_vm_run(void);
void bench_scheduler_run(void);
void bench_fs_run(void);
void bench_gfx_run(void);
void bench_stress_run(void);
void bench_kernel_test_run(void);

/* ============================================================================
 * bench_run_all — run every benchmark suite in sequence
 * ============================================================================ */
void bench_run_all(void)
{
    /* Reset counters */
    bench_pass_count = 0;
    bench_fail_count = 0;

    bench_timer_init();
    bench_print_env();

    bench_kernel_test_run();
    bench_memory_run();
    bench_vm_run();
    bench_scheduler_run();
    bench_fs_run();
    bench_gfx_run();

    /* Final summary */
    bench_section("BENCHMARK SUITE COMPLETE");
    kputs("  Tests PASSED : "); kput_uint((uint32_t)bench_pass_count); kputs("\n");
    kputs("  Tests FAILED : "); kput_uint((uint32_t)bench_fail_count); kputs("\n");
    kputs("\n");
    kputs("  To capture CSV output:\n");
    kputs("    make qemu 2>&1 | grep '^BENCH:' > bench/results.csv\n");
    kputs("========================================\n");
}

/* ============================================================================
 * bench_run_sub — run a single named benchmark suite
 * name: "memory", "vm", "scheduler", "fs", "gfx", "stress", "test"
 * ============================================================================ */
void bench_run_sub(const char *name)
{
    bench_timer_init();
    bench_print_env();

    /* Simple string compare without strcmp (in case string.h is minimal) */
#define STREQ(a,b) (bench_streq((a),(b)))
    /* inline streq */
    int matched = 0;

    /* memory */
    if (!matched) {
        const char *a = name, *b = "memory";
        while (*a && *b && *a == *b) { a++; b++; }
        if (*a == '\0' && *b == '\0') { bench_memory_run(); matched = 1; }
    }
    /* vm */
    if (!matched) {
        const char *a = name, *b = "vm";
        while (*a && *b && *a == *b) { a++; b++; }
        if (*a == '\0' && *b == '\0') { bench_vm_run(); matched = 1; }
    }
    /* scheduler */
    if (!matched) {
        const char *a = name, *b = "scheduler";
        while (*a && *b && *a == *b) { a++; b++; }
        if (*a == '\0' && *b == '\0') { bench_scheduler_run(); matched = 1; }
    }
    /* fs */
    if (!matched) {
        const char *a = name, *b = "fs";
        while (*a && *b && *a == *b) { a++; b++; }
        if (*a == '\0' && *b == '\0') { bench_fs_run(); matched = 1; }
    }
    /* gfx */
    if (!matched) {
        const char *a = name, *b = "gfx";
        while (*a && *b && *a == *b) { a++; b++; }
        if (*a == '\0' && *b == '\0') { bench_gfx_run(); matched = 1; }
    }
    /* stress */
    if (!matched) {
        const char *a = name, *b = "stress";
        while (*a && *b && *a == *b) { a++; b++; }
        if (*a == '\0' && *b == '\0') { bench_stress_run(); matched = 1; }
    }
    /* test */
    if (!matched) {
        const char *a = name, *b = "test";
        while (*a && *b && *a == *b) { a++; b++; }
        if (*a == '\0' && *b == '\0') { bench_kernel_test_run(); matched = 1; }
    }
#undef STREQ

    if (!matched) {
        kputs("Unknown benchmark: ");
        kputs(name);
        kputs("\nAvailable: memory, vm, scheduler, fs, gfx, stress, test\n");
    }
}

/* Internal string equality for bench_main (avoids external dependency) */
int bench_streq(const char *a, const char *b)
{
    while (*a && *b && *a == *b) { a++; b++; }
    return (*a == '\0' && *b == '\0');
}
