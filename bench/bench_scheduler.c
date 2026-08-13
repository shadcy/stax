/* ============================================================================
 * STAX — bench/bench_scheduler.c
 * Scheduler benchmarks
 *
 * Measures:
 *   - Context-switch latency (inter-task timing)
 *   - Timer interrupt overhead
 *   - Scheduling round-trip cost
 *   - Context switches per second (wall-clock measured)
 *   - Fairness across equal tasks
 *   - Scheduler overhead as % of CPU time
 *
 * DESIGN NOTE:
 *   The scheduler fires from the Timer0 IRQ (1000 Hz = every 1000 µs).
 *   We use Timer1 (free-running, no IRQ) as the measurement clock.
 *
 *   Context-switch latency is measured as:
 *     Task A records Timer1 value at last instruction before yield-by-preemption
 *     Task B records Timer1 value at first instruction after resume
 *     Difference = context switch latency
 *   Since there's no explicit yield syscall, we measure by letting
 *   preemption occur naturally and observing the timing gap.
 *
 *   MAX_TASKS = 4, so we can run 1 kernel + 3 bench tasks simultaneously.
 * ============================================================================ */

#include "bench.h"
#include "scheduler.h"
#include "console.h"

/* External tick counter from kernel.c */
extern volatile unsigned int tick_count;

/* ============================================================================
 * Shared communication between bench tasks
 * Using volatile to prevent compiler optimization.
 * ============================================================================ */

/* Records the timer value at last preemption point for each task slot */
static volatile uint32_t sched_exit_time[4] = {0, 0, 0, 0};

/* Records the timer value at first instruction after resume */
static volatile uint32_t sched_entry_time[4] = {0, 0, 0, 0};

/* Latency samples collected across all task pairs */
static volatile uint32_t switch_samples[256];
static volatile uint32_t switch_sample_count = 0;

/* Completion counters for fairness measurement */
static volatile uint32_t task_iters[4] = {0, 0, 0, 0};

/* Signal to bench tasks to stop */
static volatile int bench_sched_stop = 0;

/* Number of context switches observed */
static volatile uint32_t bench_switch_count = 0;

/* Bench task IDs */
static volatile int bench_task_ids[3] = {-1, -1, -1};
static volatile int bench_tasks_done[3] = {0, 0, 0};

/* ============================================================================
 * Benchmark task functions
 * Each task records when it gets the CPU and when it last ran,
 * to measure context-switch latency.
 * ============================================================================ */

static void bench_sched_task_a(void)
{
    uint32_t local_count = 0;
    /* Record entry time immediately */
    sched_entry_time[1] = BENCH_TIMER_MAXVAL - BENCH_T1_VALUE;

    while (!bench_sched_stop) {
        /* Do a small amount of work */
        local_count++;
        task_iters[1] = local_count;
        bench_switch_count++;

        /* Record exit time just before preemption will occur */
        sched_exit_time[1] = BENCH_TIMER_MAXVAL - BENCH_T1_VALUE;

        /* Spin for a bit so preemption is likely on next timer tick */
        for (volatile int i = 0; i < 500; i++) {
            __asm__ volatile ("nop");
        }

        /* Record when we resume */
        uint32_t resume_t = BENCH_TIMER_MAXVAL - BENCH_T1_VALUE;
        sched_entry_time[1] = resume_t;

        /* Compute latency since last exit if we have a valid exit time
           and the switch count limit hasn't been hit */
        if (switch_sample_count < 256 && sched_exit_time[1] < resume_t) {
            uint32_t lat = resume_t - sched_exit_time[1];
            /* Sanity: context switch should be < 10ms (10000 us) */
            if (lat < 10000 && lat > 0) {
                switch_samples[switch_sample_count++] = lat;
            }
        }
    }
    bench_tasks_done[0] = 1;
    task_exit();
}

static void bench_sched_task_b(void)
{
    uint32_t local_count = 0;
    sched_entry_time[2] = BENCH_TIMER_MAXVAL - BENCH_T1_VALUE;

    while (!bench_sched_stop) {
        local_count++;
        task_iters[2] = local_count;
        sched_exit_time[2] = BENCH_TIMER_MAXVAL - BENCH_T1_VALUE;

        for (volatile int i = 0; i < 500; i++) {
            __asm__ volatile ("nop");
        }

        uint32_t resume_t = BENCH_TIMER_MAXVAL - BENCH_T1_VALUE;
        sched_entry_time[2] = resume_t;

        if (switch_sample_count < 256 && sched_exit_time[2] < resume_t) {
            uint32_t lat = resume_t - sched_exit_time[2];
            if (lat < 10000 && lat > 0) {
                switch_samples[switch_sample_count++] = lat;
            }
        }
    }
    bench_tasks_done[1] = 1;
    task_exit();
}

static void bench_sched_task_c(void)
{
    uint32_t local_count = 0;
    sched_entry_time[3] = BENCH_TIMER_MAXVAL - BENCH_T1_VALUE;

    while (!bench_sched_stop) {
        local_count++;
        task_iters[3] = local_count;
        sched_exit_time[3] = BENCH_TIMER_MAXVAL - BENCH_T1_VALUE;

        for (volatile int i = 0; i < 500; i++) {
            __asm__ volatile ("nop");
        }

        uint32_t resume_t = BENCH_TIMER_MAXVAL - BENCH_T1_VALUE;
        sched_entry_time[3] = resume_t;

        if (switch_sample_count < 256 && sched_exit_time[3] < resume_t) {
            uint32_t lat = resume_t - sched_exit_time[3];
            if (lat < 10000 && lat > 0) {
                switch_samples[switch_sample_count++] = lat;
            }
        }
    }
    bench_tasks_done[2] = 1;
    task_exit();
}

/* ============================================================================
 * bench_timer_overhead — measure timer ISR latency
 *
 * Approach: wait for tick_count to change, measure how long that took
 * relative to the 1000 µs expected period. The excess = IRQ + dispatch overhead.
 * ============================================================================ */
static void bench_timer_overhead(void)
{
    bench_section("TIMER INTERRUPT OVERHEAD");
    bench_timer_init();

    static uint32_t samples[64];
    uint32_t last_tick = tick_count;

    for (int i = 0; i < 64; i++) {
        /* Wait for the tick to advance */
        while (tick_count == last_tick) {
            __asm__ volatile ("nop");
        }
        /* Capture time immediately after tick */
        samples[i] = bench_timer_read_us();
        bench_timer_reset();
        last_tick = tick_count;
    }

    /* Each sample is the time from just-after-tick to just-after-next-tick.
     * Expected: ~1000 us (1 ms timer period).
     * Excess over 1000 us = overhead of IRQ entry/dispatch + ISR body. */
    bench_result_t result;
    result.name = "timer_tick_period";
    result.unit = "us";
    bench_compute(&result, samples, 64);
    bench_report(&result);

    kputs("  Expected period: 1000 us\n");
    if (result.mean_us >= 1000) {
        kputs("  Measured overhead: ");
        kput_uint(result.mean_us - 1000);
        kputs(" us above nominal\n");
    } else {
        kputs("  Measured period slightly below 1000 us (QEMU timing jitter)\n");
    }
}

/* ============================================================================
 * bench_context_switch — create N tasks, measure switch latency
 * ============================================================================ */
static void bench_context_switch(int num_extra_tasks)
{
    bench_section("CONTEXT-SWITCH LATENCY");
    bench_timer_init();

    /* Reset shared state */
    bench_sched_stop = 0;
    switch_sample_count = 0;
    bench_switch_count = 0;
    for (int i = 0; i < 3; i++) {
        task_iters[i+1] = 0;
        bench_tasks_done[i] = 0;
    }

    kputs("  Spawning ");
    kput_uint((uint32_t)num_extra_tasks);
    kputs(" bench task(s)...\n");

    /* Create tasks based on requested count (max 3 due to MAX_TASKS=4) */
    if (num_extra_tasks >= 1)
        bench_task_ids[0] = task_create(bench_sched_task_a);
    if (num_extra_tasks >= 2)
        bench_task_ids[1] = task_create(bench_sched_task_b);
    if (num_extra_tasks >= 3)
        bench_task_ids[2] = task_create(bench_sched_task_c);

    /* Run for ~2 seconds of wall time collecting samples */
    unsigned int start_tick = tick_count;
    unsigned int run_ms = 2000;  /* 2 seconds */

    while ((tick_count - start_tick) < run_ms) {
        /* Main task does nothing, lets others run */
        __asm__ volatile ("nop");
    }

    /* Signal tasks to stop */
    bench_sched_stop = 1;

    /* Wait for tasks to exit (poll their done flags) */
    unsigned int wait_start = tick_count;
    while ((tick_count - wait_start) < 500) {
        int all_done = 1;
        for (int i = 0; i < num_extra_tasks; i++) {
            if (!bench_tasks_done[i]) { all_done = 0; break; }
        }
        if (all_done) break;
        __asm__ volatile ("nop");
    }

    /* Report context switch latency samples */
    uint32_t sc = switch_sample_count;
    kputs("  Switch samples collected: ");
    kput_uint(sc);
    kputs("\n");

    if (sc > 0) {
        bench_result_t result;
        result.name = "context_switch_latency";
        result.unit = "us";
        /* switch_samples is volatile, copy to non-volatile buffer */
        static uint32_t local_samples[256];
        for (uint32_t i = 0; i < sc && i < 256; i++)
            local_samples[i] = switch_samples[i];
        bench_compute(&result, local_samples, sc);
        bench_report(&result);
    } else {
        kputs("  [WARN] No switch samples collected. Tasks may not have\n");
        kputs("         been preempted. Try with longer bench duration.\n");
    }

    /* Fairness: compare iteration counts across tasks */
    if (num_extra_tasks >= 2) {
        bench_section("SCHEDULER FAIRNESS");
        kputs("  Task iteration counts after 2s:\n");
        for (int i = 0; i < num_extra_tasks; i++) {
            kputs("    task_");
            kput_uint((uint32_t)(i+1));
            kputs(": ");
            kput_uint(task_iters[i+1]);
            kputs(" iters\n");
        }

        /* Fairness metric: coefficient of variation.
         * Perfect fairness = all tasks have same count.
         * We compute max/min ratio. Ideal = 1.0 */
        uint32_t min_iters = task_iters[1];
        uint32_t max_iters = task_iters[1];
        for (int i = 1; i < num_extra_tasks; i++) {
            if (task_iters[i+1] < min_iters) min_iters = task_iters[i+1];
            if (task_iters[i+1] > max_iters) max_iters = task_iters[i+1];
        }
        kputs("  min_iters=");  kput_uint(min_iters);
        kputs("  max_iters=");  kput_uint(max_iters);
        if (min_iters > 0) {
            uint32_t unfairness_pct = ((max_iters - min_iters) * 100) / max_iters;
            kputs("  unfairness=");
            kput_uint(unfairness_pct);
            kputs("%\n");
            kputs("BENCH:scheduler_fairness_unfairness_pct,1,");
            kput_uint(unfairness_pct);
            kputs(",");
            kput_uint(unfairness_pct);
            kputs(",");
            kput_uint(unfairness_pct);
            kputs(",");
            kput_uint(unfairness_pct);
            kputs(",0\n");
        } else {
            kputs("\n");
        }
    }

    /* Context switches per second */
    uint32_t elapsed_ms = tick_count - start_tick;
    if (elapsed_ms > 0) {
        bench_section("CONTEXT SWITCHES PER SECOND");
        uint32_t switches_ps = (bench_switch_count * 1000) / elapsed_ms;
        kputs("  Total switch events (approx): ");
        kput_uint(bench_switch_count);
        kputs("\n");
        kputs("  Elapsed: ");
        kput_uint(elapsed_ms);
        kputs(" ms\n");
        kputs("  Context switches/sec (approx): ");
        kput_uint(switches_ps);
        kputs("\n");
        kputs("BENCH:ctx_switches_per_sec,1,");
        kput_uint(switches_ps);
        kputs(",");
        kput_uint(switches_ps);
        kputs(",");
        kput_uint(switches_ps);
        kputs(",");
        kput_uint(switches_ps);
        kputs(",0\n");
    }
}

/* ============================================================================
 * bench_scheduler_overhead — estimate % of CPU time in scheduler
 *
 * Strategy:
 *   - Count NOP iterations in 1 second with 0 extra tasks
 *   - Count NOP iterations in 1 second with 3 extra tasks
 *   - Overhead = (baseline - loaded) / baseline * 100
 * ============================================================================ */
static uint32_t count_nops_in_1s(void)
{
    volatile uint32_t count = 0;
    unsigned int start = tick_count;
    while ((tick_count - start) < 1000) {
        __asm__ volatile ("nop");
        count++;
    }
    return count;
}

static void bench_scheduler_overhead(void)
{
    bench_section("SCHEDULER OVERHEAD MEASUREMENT");
    bench_timer_init();

    kputs("  Measuring NOP throughput with 1 task (baseline)...\n");
    uint32_t baseline = count_nops_in_1s();
    kputs("  Baseline NOPs/sec: ");
    kput_uint(baseline);
    kputs("\n");

    /* Create 3 extra tasks that also spin */
    bench_sched_stop = 0;
    for (int i = 0; i < 3; i++) bench_tasks_done[i] = 0;
    task_create(bench_sched_task_a);
    task_create(bench_sched_task_b);
    task_create(bench_sched_task_c);

    kputs("  Measuring NOP throughput with 4 tasks (loaded)...\n");
    uint32_t loaded = count_nops_in_1s();

    bench_sched_stop = 1;
    unsigned int ws = tick_count;
    while ((tick_count - ws) < 200) __asm__ volatile ("nop");

    kputs("  Loaded NOPs/sec (main task): ");
    kput_uint(loaded);
    kputs("\n");

    if (baseline > 0) {
        /* Main task gets 1/4 of CPU in round-robin with 4 tasks.
         * Expected loaded = baseline / 4 (minus overhead).
         * Overhead = (baseline/4 - loaded) / (baseline/4) * 100  if loaded < baseline/4
         * Or we can just report: main task CPU share */
        uint32_t expected_share = baseline / 4;
        kputs("  Expected share (1/4 of baseline): ");
        kput_uint(expected_share);
        kputs("\n");

        if (expected_share > 0 && loaded <= expected_share) {
            uint32_t overhead_pct = ((expected_share - loaded) * 100) / expected_share;
            kputs("  Scheduler overhead estimate: ");
            kput_uint(overhead_pct);
            kputs("% of task time-slice\n");
            kputs("BENCH:scheduler_overhead_pct,1,");
            kput_uint(overhead_pct);
            kputs(",");
            kput_uint(overhead_pct);
            kputs(",");
            kput_uint(overhead_pct);
            kputs(",");
            kput_uint(overhead_pct);
            kputs(",0\n");
        } else {
            kputs("  Overhead: <1% (within measurement noise)\n");
        }
    }
}

/* ============================================================================
 * bench_scheduler_run — entry point
 * ============================================================================ */
void bench_scheduler_run(void)
{
    bench_section("SCHEDULER BENCHMARKS");

    kputs("  Scheduler type: Preemptive round-robin\n");
    kputs("  Timer quantum : 10 ms (every 10 Timer0 IRQs at 1000 Hz)\n");
    kputs("  MAX_TASKS     : 4 (1 kernel + 3 user tasks max)\n");
    kputs("  Context-switch: Assembly in kernel/vectors.s\n");
    kputs("  Saved context : r4-r11 + SVC sp/lr + IRQ stack r0-r3,r12,pc,cpsr\n\n");

    bench_timer_overhead();

    kputs("\nRunning with 2 extra tasks (3 total including main):\n");
    bench_context_switch(2);

    /* Reset state for overhead measurement */
    bench_sched_stop = 0;
    switch_sample_count = 0;
    bench_switch_count = 0;
    for (int i = 0; i < 4; i++) task_iters[i] = 0;
    for (int i = 0; i < 3; i++) bench_tasks_done[i] = 0;

    kputs("\nRunning with 3 extra tasks (4 total — MAX_TASKS limit):\n");
    bench_context_switch(3);

    bench_scheduler_overhead();
}
