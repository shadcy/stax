/* ============================================================================
 * STAX — bench/bench.h
 * Lightweight benchmark framework for bare-metal ARM
 *
 * TIMING SOURCE: SP804 Timer1 (TIMER_BASE + 0x20), free-running 32-bit
 * countdown at 1 MHz reference clock. Gives 1 µs resolution.
 * Timer0 is untouched — the scheduler at 1000 Hz keeps running.
 *
 * QEMU DISCLAIMER:
 *   All measurements are QEMU virtual time, not physical hardware cycles.
 *   Values are valid for relative algorithm comparisons within STAX only.
 *   They do NOT represent real ARM926EJ-S silicon performance.
 * ============================================================================ */

#ifndef BENCH_H
#define BENCH_H

#include <stdint.h>

/* ---- SP804 Timer1 register offsets from TIMER_BASE (0x101E2000) ---- */
#define BENCH_TIMER_BASE  0x101E2000UL
#define BENCH_T1_LOAD   (*(volatile uint32_t *)(BENCH_TIMER_BASE + 0x20))
#define BENCH_T1_VALUE  (*(volatile uint32_t *)(BENCH_TIMER_BASE + 0x24))
#define BENCH_T1_CTRL   (*(volatile uint32_t *)(BENCH_TIMER_BASE + 0x28))
#define BENCH_T1_INTCLR (*(volatile uint32_t *)(BENCH_TIMER_BASE + 0x2C))

/* Timer control bits */
#define BENCH_T1_CTRL_ENABLE   (1U << 7)
#define BENCH_T1_CTRL_32BIT    (1U << 1)
/* Free-running: no PERIODIC bit, no INTEN */

/* Maximum load value gives ~4295 second range at 1 MHz */
#define BENCH_TIMER_MAXVAL  0xFFFFFFFFUL

/* ---- Result structure ---- */
#define BENCH_MAX_SAMPLES  512

typedef struct {
    const char *name;        /* test name (e.g. "kmalloc_64b") */
    const char *unit;        /* unit string (e.g. "us", "bytes/s") */
    uint32_t    count;       /* number of iterations measured */
    uint32_t    min_us;      /* minimum latency in µs */
    uint32_t    max_us;      /* maximum latency in µs */
    uint32_t    mean_us;     /* arithmetic mean in µs */
    uint32_t    median_us;   /* median in µs */
    uint32_t    total_us;    /* total time for all iterations */
    uint32_t    throughput;  /* ops/sec or bytes/sec */
} bench_result_t;

/* ---- API ---- */

/* Configure Timer1 as a free-running countdown. Call once before any bench. */
void bench_timer_init(void);

/* Read elapsed microseconds since bench_timer_init() or bench_timer_reset(). */
uint32_t bench_timer_read_us(void);

/* Reset the timer (restart countdown from BENCH_TIMER_MAXVAL). */
void bench_timer_reset(void);

/*
 * Compute statistics from an array of raw µs samples.
 * Fills in result->min/max/mean/median/total.
 */
void bench_compute(bench_result_t *result,
                   uint32_t *samples,
                   uint32_t count);

/*
 * Print a benchmark result:
 * - Human-readable line to kputs
 * - Machine-readable CSV line prefixed with "BENCH:"
 *   Format: BENCH:<name>,<count>,<min>,<max>,<mean>,<median>,<throughput>
 */
void bench_report(const bench_result_t *result);

/* Print the QEMU environment disclaimer banner. */
void bench_print_env(void);

/* Print a section header. */
void bench_section(const char *title);

/* Print pass/fail for unit tests. */
void bench_pass(const char *name);
void bench_fail(const char *name, const char *reason);

/* Utility: simple integer sqrt for stddev (not used in result, but available) */
uint32_t bench_isqrt(uint32_t n);

/* Utility: format a uint32 with comma separators into a buffer */
void bench_fmt_uint(uint32_t n, char *buf, int buflen);

/* Global counters for [PASS]/[FAIL] totals */
extern int bench_pass_count;
extern int bench_fail_count;

/* ---- Convenience macros ---- */

/*
 * BENCH_RUN(result_ptr, iters, body)
 * Runs body iters times, samples each µs cost, fills result_ptr.
 * Example:
 *   uint32_t samples[256];
 *   BENCH_RUN(&r, 256, {
 *       void *p = kmalloc(64);
 *       kfree(p);
 *   });
 */
#define BENCH_RUN(result_ptr, iters, body)              \
    do {                                                 \
        uint32_t _samples[(iters)];                      \
        uint32_t _n = (uint32_t)(iters);                 \
        for (uint32_t _i = 0; _i < _n; _i++) {          \
            bench_timer_reset();                          \
            { body }                                      \
            _samples[_i] = bench_timer_read_us();        \
        }                                                \
        bench_compute((result_ptr), _samples, _n);       \
    } while (0)

/*
 * BENCH_RUN_WARMUP(result_ptr, warmup, iters, body)
 * Like BENCH_RUN but discards the first `warmup` samples.
 */
#define BENCH_RUN_WARMUP(result_ptr, warmup, iters, body)       \
    do {                                                          \
        uint32_t _total = (uint32_t)((warmup) + (iters));        \
        uint32_t _samples[(iters)];                               \
        uint32_t _si = 0;                                         \
        for (uint32_t _i = 0; _i < _total; _i++) {               \
            bench_timer_reset();                                   \
            { body }                                               \
            uint32_t _t = bench_timer_read_us();                  \
            if (_i >= (uint32_t)(warmup))                         \
                _samples[_si++] = _t;                             \
        }                                                         \
        bench_compute((result_ptr), _samples, (uint32_t)(iters)); \
    } while (0)

#endif /* BENCH_H */
