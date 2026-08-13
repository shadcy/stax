/* ============================================================================
 * STAX — bench/bench.c
 * Benchmark framework implementation
 * ============================================================================ */

#include "bench.h"
#include "console.h"
#include "string.h"

/* ---- Global pass/fail counters ---- */
int bench_pass_count = 0;
int bench_fail_count = 0;

/* ============================================================================
 * Timer1 control
 * ============================================================================ */

void bench_timer_init(void)
{
    /* Disable timer while configuring */
    BENCH_T1_CTRL = 0;
    /* Load max value — counts down from 0xFFFFFFFF */
    BENCH_T1_LOAD = BENCH_TIMER_MAXVAL;
    /* 32-bit, free-running (no PERIODIC, no INTEN), enable */
    BENCH_T1_CTRL = BENCH_T1_CTRL_32BIT | BENCH_T1_CTRL_ENABLE;
}

void bench_timer_reset(void)
{
    /* Writing to LOAD restarts the countdown */
    BENCH_T1_LOAD = BENCH_TIMER_MAXVAL;
}

uint32_t bench_timer_read_us(void)
{
    /* Elapsed = MAXVAL - current_value (timer counts down) */
    return BENCH_TIMER_MAXVAL - BENCH_T1_VALUE;
}

/* ============================================================================
 * Utility functions
 * ============================================================================ */

/* Quick integer sort (insertion sort — fine for ≤512 samples) */
static void sort_u32(uint32_t *arr, uint32_t n)
{
    for (uint32_t i = 1; i < n; i++) {
        uint32_t key = arr[i];
        int j = (int)i - 1;
        while (j >= 0 && arr[j] > key) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

uint32_t bench_isqrt(uint32_t n)
{
    if (n == 0) return 0;
    uint32_t x = n;
    uint32_t y = (x + 1) / 2;
    while (y < x) {
        x = y;
        y = (x + n / x) / 2;
    }
    return x;
}

/* ============================================================================
 * bench_compute — fill result statistics from raw sample array
 * NOTE: This function SORTS the samples array in-place (for median).
 * ============================================================================ */
void bench_compute(bench_result_t *result,
                   uint32_t *samples,
                   uint32_t count)
{
    if (!result || !samples || count == 0) return;

    result->count = count;

    /* Find min/max and sum */
    uint32_t min_v = samples[0];
    uint32_t max_v = samples[0];
    uint64_t total = 0;

    for (uint32_t i = 0; i < count; i++) {
        if (samples[i] < min_v) min_v = samples[i];
        if (samples[i] > max_v) max_v = samples[i];
        total += samples[i];
    }

    result->min_us   = min_v;
    result->max_us   = max_v;
    result->total_us = (uint32_t)(total & 0xFFFFFFFFUL);
    result->mean_us  = (count > 0) ? (uint32_t)(total / count) : 0;

    /* Median: sort then pick middle */
    sort_u32(samples, count);
    if (count % 2 == 1)
        result->median_us = samples[count / 2];
    else
        result->median_us = (samples[count / 2 - 1] + samples[count / 2]) / 2;

    /* Throughput: ops/sec = 1,000,000 / mean_us */
    if (result->mean_us > 0)
        result->throughput = 1000000UL / result->mean_us;
    else
        result->throughput = 0; /* sub-microsecond — report 0, not inf */
}

/* ============================================================================
 * bench_fmt_uint — format with comma separators
 * e.g. 1234567 -> "1,234,567"
 * ============================================================================ */
void bench_fmt_uint(uint32_t n, char *buf, int buflen)
{
    char tmp[16];
    int ti = 0;

    if (n == 0) {
        tmp[ti++] = '0';
    } else {
        while (n > 0) {
            tmp[ti++] = '0' + (n % 10);
            n /= 10;
        }
    }

    /* tmp is reversed digits */
    int bi = 0;
    for (int i = ti - 1; i >= 0 && bi < buflen - 1; i--) {
        buf[bi++] = tmp[i];
        /* Insert comma every 3 digits from the left */
        int from_end = i;
        if (from_end > 0 && from_end % 3 == 0 && bi < buflen - 1)
            buf[bi++] = ',';
    }
    buf[bi] = '\0';
}

/* ============================================================================
 * bench_report — print human-readable + BENCH: CSV line
 * ============================================================================ */
void bench_report(const bench_result_t *result)
{
    if (!result) return;

    char buf[24];

    /* Human-readable */
    kputs("  [BENCH] ");
    kputs(result->name);
    kputs(":\n");

    kputs("    count=");
    bench_fmt_uint(result->count, buf, sizeof(buf));
    kputs(buf);

    kputs("  min=");
    bench_fmt_uint(result->min_us, buf, sizeof(buf));
    kputs(buf);
    kputs("us");

    kputs("  max=");
    bench_fmt_uint(result->max_us, buf, sizeof(buf));
    kputs(buf);
    kputs("us");

    kputs("  mean=");
    bench_fmt_uint(result->mean_us, buf, sizeof(buf));
    kputs(buf);
    kputs("us");

    kputs("  median=");
    bench_fmt_uint(result->median_us, buf, sizeof(buf));
    kputs(buf);
    kputs("us");

    kputs("  ops/s=");
    bench_fmt_uint(result->throughput, buf, sizeof(buf));
    kputs(buf);
    kputs("\n");

    /* Machine-readable CSV: BENCH:<name>,<count>,<min>,<max>,<mean>,<median>,<ops_s> */
    kputs("BENCH:");
    kputs(result->name);
    kputc(',');
    kput_uint(result->count);
    kputc(',');
    kput_uint(result->min_us);
    kputc(',');
    kput_uint(result->max_us);
    kputc(',');
    kput_uint(result->mean_us);
    kputc(',');
    kput_uint(result->median_us);
    kputc(',');
    kput_uint(result->throughput);
    kputc('\n');
}

/* ============================================================================
 * bench_print_env — print QEMU environment disclaimer
 * ============================================================================ */
void bench_print_env(void)
{
    kputs("========================================\n");
    kputs("  STAX Benchmark Environment\n");
    kputs("========================================\n");
    kputs("  Emulator  : QEMU qemu-system-arm\n");
    kputs("  Machine   : versatilepb (ARM VersatilePB)\n");
    kputs("  CPU       : ARM926EJ-S (ARMv5TE)\n");
    kputs("  RAM       : 32 MB\n");
    kputs("  Timer src : SP804 Timer1 @ 1 MHz (1 us resolution)\n");
    kputs("  Display   : PL110 640x480 16bpp\n");
    kputs("  Storage   : PL181 emulated SD card\n");
    kputs("  Network   : NOT IMPLEMENTED\n");
    kputs("  Compiler  : arm-none-eabi-gcc -O2 -mcpu=arm926ej-s\n");
    kputs("----------------------------------------\n");
    kputs("  NOTE: All timings are QEMU virtual time.\n");
    kputs("  Values are valid for relative comparisons\n");
    kputs("  within STAX but do NOT represent physical\n");
    kputs("  ARM926EJ-S silicon performance.\n");
    kputs("========================================\n");
}

/* ============================================================================
 * bench_section — section separator
 * ============================================================================ */
void bench_section(const char *title)
{
    kputs("\n--- ");
    kputs(title);
    kputs(" ---\n");
}

/* ============================================================================
 * bench_pass / bench_fail — unit test reporting
 * ============================================================================ */
void bench_pass(const char *name)
{
    bench_pass_count++;
    kputs("  [PASS] ");
    kputs(name);
    kputc('\n');
}

void bench_fail(const char *name, const char *reason)
{
    bench_fail_count++;
    kputs("  [FAIL] ");
    kputs(name);
    kputs(" -- ");
    kputs(reason);
    kputc('\n');
}
