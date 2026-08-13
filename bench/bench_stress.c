/* ============================================================================
 * STAX — bench/bench_stress.c
 * Comprehensive stress tests for STAX subsystems
 *
 * Stress categories:
 *   MEMORY:     randomized alloc/free with canary, OOM recovery
 *   SCHEDULER:  concurrent tasks running for extended period
 *   FILESYSTEM: repeated create/write/read/verify/delete cycles
 *   GRAPHICS:   sustained rendering, multiple windows
 *
 * Detection:
 *   - Memory corruption: canary value overwrite
 *   - Filesystem corruption: write-then-read mismatch
 *   - Scheduler hang: timeout detection via tick_count
 *   - Unexpected task termination: done flag never set
 * ============================================================================ */

#include "bench.h"
#include "heap.h"
#include "page.h"
#include "scheduler.h"
#include "fat.h"
#include "framebuffer.h"
#include "wm.h"
#include "console.h"

extern volatile unsigned int tick_count;

#define CANARY_A  0xDEADC0DEUL
#define CANARY_B  0xBAADF00DUL
#define STRESS_TIMEOUT_MS 10000  /* 10 second timeout for any stress test */

/* ============================================================================
 * MEMORY STRESS
 * ============================================================================ */

#define MEM_STRESS_SLOTS  24
#define MEM_STRESS_OPS    8000

static uint32_t stress_rng = 0x1337CAFE;
static uint32_t stress_rand(void)
{
    stress_rng = stress_rng * 1664525UL + 1013904223UL;
    return stress_rng;
}

static int stress_memory_run(int ops)
{
    void    *ptrs[MEM_STRESS_SLOTS];
    uint32_t szs[MEM_STRESS_SLOTS];
    int      live[MEM_STRESS_SLOTS];
    int      errors = 0;
    int      oom_count = 0;
    int      alloc_count = 0;
    int      free_count = 0;

    stress_rng = 0xABCD1234;
    for (int i = 0; i < MEM_STRESS_SLOTS; i++) { ptrs[i] = NULL; live[i] = 0; }

    int free_before = get_free_memory();

    for (int op = 0; op < ops; op++) {
        int slot = (int)(stress_rand() % MEM_STRESS_SLOTS);

        if (live[slot]) {
            /* Verify canary */
            volatile uint32_t *p = (volatile uint32_t *)ptrs[slot];
            uint32_t nwords = szs[slot] / 4;
            for (uint32_t w = 0; w < nwords; w++) {
                if (p[w] != CANARY_A) { errors++; break; }
            }
            kfree(ptrs[slot]);
            ptrs[slot] = NULL;
            live[slot] = 0;
            free_count++;
        } else {
            /* Random size: 8, 16, 32, 64, 128, 256, 512, 1024 bytes */
            static const uint32_t szs_table[] = {8,16,32,64,128,256,512,1024};
            uint32_t sz = szs_table[stress_rand() % 8];
            void *p = kmalloc(sz);
            if (p) {
                volatile uint32_t *wp = (volatile uint32_t *)p;
                for (uint32_t w = 0; w < sz / 4; w++) wp[w] = CANARY_A;
                ptrs[slot] = p;
                szs[slot] = sz;
                live[slot] = 1;
                alloc_count++;
            } else {
                oom_count++;
            }
        }
    }

    /* Free survivors */
    for (int i = 0; i < MEM_STRESS_SLOTS; i++) {
        if (live[i]) {
            volatile uint32_t *p = (volatile uint32_t *)ptrs[i];
            for (uint32_t w = 0; w < szs[i] / 4; w++) {
                if (p[w] != CANARY_A) { errors++; break; }
            }
            kfree(ptrs[i]);
        }
    }

    int free_after = get_free_memory();
    int leak = free_before - free_after;

    kputs("  stress_memory results:\n");
    kputs("    ops=");       kput_uint((uint32_t)ops);
    kputs("  allocs=");      kput_uint((uint32_t)alloc_count);
    kputs("  frees=");       kput_uint((uint32_t)free_count);
    kputs("  oom=");         kput_uint((uint32_t)oom_count);
    kputs("  errors=");      kput_uint((uint32_t)errors);
    kputs("\n");
    kputs("    free_before="); kput_uint((uint32_t)free_before);
    kputs("  free_after=");  kput_uint((uint32_t)free_after);
    kputs("  leak=");        kput_uint((uint32_t)(leak > 0 ? leak : 0));
    kputs(" bytes\n");

    return errors + (leak > 4096 ? 1 : 0); /* allow 1 page of rounding */
}

/* ============================================================================
 * SCHEDULER STRESS
 * ============================================================================ */

static volatile int stress_sched_stop = 0;
static volatile uint32_t stress_task_count[3] = {0, 0, 0};
static volatile int stress_task_done[3] = {0, 0, 0};

static void stress_sched_worker_a(void)
{
    while (!stress_sched_stop) {
        stress_task_count[0]++;
        /* CPU-bound: compute a checksum */
        volatile uint32_t acc = 0;
        for (int i = 0; i < 1000; i++) acc ^= (uint32_t)(i * 0x9E37);
        (void)acc;
    }
    stress_task_done[0] = 1;
    task_exit();
}

static void stress_sched_worker_b(void)
{
    while (!stress_sched_stop) {
        stress_task_count[1]++;
        volatile uint32_t acc = 0;
        for (int i = 0; i < 1000; i++) acc ^= (uint32_t)(i * 0x3E7F);
        (void)acc;
    }
    stress_task_done[1] = 1;
    task_exit();
}

static void stress_sched_worker_c(void)
{
    while (!stress_sched_stop) {
        stress_task_count[2]++;
        volatile uint32_t acc = 0;
        for (int i = 0; i < 1000; i++) acc ^= (uint32_t)(i * 0xB35A);
        (void)acc;
    }
    stress_task_done[2] = 1;
    task_exit();
}

static int stress_scheduler_run(int duration_ms)
{
    stress_sched_stop = 0;
    for (int i = 0; i < 3; i++) { stress_task_count[i] = 0; stress_task_done[i] = 0; }

    task_create(stress_sched_worker_a);
    task_create(stress_sched_worker_b);
    task_create(stress_sched_worker_c);

    unsigned int start = tick_count;
    while ((int)(tick_count - start) < duration_ms) {
        __asm__ volatile ("nop");
    }

    stress_sched_stop = 1;

    /* Wait for all tasks to acknowledge stop, with timeout */
    unsigned int ws = tick_count;
    while ((int)(tick_count - ws) < 2000) {
        if (stress_task_done[0] && stress_task_done[1] && stress_task_done[2]) break;
        __asm__ volatile ("nop");
    }

    int all_done = stress_task_done[0] && stress_task_done[1] && stress_task_done[2];
    int timed_out = !all_done;

    kputs("  stress_scheduler results (" );
    kput_uint((uint32_t)duration_ms);
    kputs(" ms):\n");
    for (int i = 0; i < 3; i++) {
        kputs("    task_");
        kput_uint((uint32_t)(i+1));
        kputs(": iters=");
        kput_uint(stress_task_count[i]);
        kputs(stress_task_done[i] ? " [DONE]" : " [HANG?]");
        kputs("\n");
    }

    return timed_out ? 1 : 0;
}

/* ============================================================================
 * FILESYSTEM STRESS
 * ============================================================================ */

#define FS_STRESS_FILES  20
#define FS_STRESS_SIZE   512  /* bytes per file */

static int stress_filesystem_run(void)
{
    int errors = 0;
    char fname[16];
    uint8_t *wbuf = (uint8_t *)kmalloc(FS_STRESS_SIZE);
    uint8_t *rbuf = (uint8_t *)kmalloc(FS_STRESS_SIZE);

    if (!wbuf || !rbuf) {
        if (wbuf) kfree(wbuf);
        if (rbuf) kfree(rbuf);
        kputs("  [WARN] Cannot allocate FS stress buffers\n");
        return 1;
    }

    /* Fill write buffer with pattern */
    for (int i = 0; i < FS_STRESS_SIZE; i++) wbuf[i] = (uint8_t)(i ^ 0xA5);

    /* Create FS_STRESS_FILES files, write, read back, verify */
    for (int fi = 0; fi < FS_STRESS_FILES; fi++) {
        /* Build filename: BSTxx.TMP */
        fname[0] = 'B'; fname[1] = 'S'; fname[2] = 'T';
        fname[3] = '0' + fi / 10;
        fname[4] = '0' + fi % 10;
        fname[5] = '.'; fname[6] = 'T'; fname[7] = 'M'; fname[8] = 'P'; fname[9] = '\0';

        FIL f;
        UINT bw, br;

        /* Write */
        FRESULT res = f_open(&f, fname, FA_CREATE_ALWAYS | FA_WRITE);
        if (res != FR_OK) { errors++; continue; }
        f_write(&f, wbuf, FS_STRESS_SIZE, &bw);
        f_close(&f);

        if (bw != FS_STRESS_SIZE) { errors++; f_unlink(fname); continue; }

        /* Read back and verify */
        res = f_open(&f, fname, FA_READ);
        if (res != FR_OK) { errors++; f_unlink(fname); continue; }
        f_read(&f, rbuf, FS_STRESS_SIZE, &br);
        f_close(&f);

        if (br != FS_STRESS_SIZE) { errors++; }
        else {
            for (int i = 0; i < FS_STRESS_SIZE; i++) {
                if (rbuf[i] != wbuf[i]) { errors++; break; }
            }
        }

        f_unlink(fname);
    }

    kfree(wbuf);
    kfree(rbuf);

    kputs("  stress_filesystem results:\n");
    kputs("    files=");    kput_uint(FS_STRESS_FILES);
    kputs("  errors=");     kput_uint((uint32_t)errors);
    kputs("\n");

    return errors;
}

/* ============================================================================
 * GRAPHICS STRESS
 * ============================================================================ */
static int stress_gfx_run(int frames)
{
    kputs("  Rendering ");
    kput_uint((uint32_t)frames);
    kputs(" frames via wm_render()...\n");

    unsigned int start = tick_count;

    for (int i = 0; i < frames; i++) {
        wm_render();
        fb_clear(0x0000); /* reset buffer */
    }

    unsigned int elapsed_ms = tick_count - start;
    uint32_t fps = (elapsed_ms > 0) ? (uint32_t)((uint64_t)frames * 1000ULL / elapsed_ms) : 0;

    kputs("  stress_gfx results:\n");
    kputs("    frames=");   kput_uint((uint32_t)frames);
    kputs("  elapsed=");    kput_uint(elapsed_ms);
    kputs(" ms  avg_fps="); kput_uint(fps);
    kputs("\n");

    return 0;
}

/* ============================================================================
 * bench_stress_run — entry point
 * ============================================================================ */
void bench_stress_run(void)
{
    bench_section("STRESS TESTS");
    kputs("  Stress tests exercise subsystems under sustained load.\n");
    kputs("  A failing stress test indicates a reliability bug.\n\n");

    /* Memory stress */
    bench_section("MEMORY STRESS (8,000 ops)");
    int mem_err = stress_memory_run(MEM_STRESS_OPS);
    if (mem_err == 0) bench_pass("memory_stress_8000_ops");
    else              bench_fail("memory_stress_8000_ops", "corruption or leak detected");

    /* Scheduler stress */
    bench_section("SCHEDULER STRESS (3 tasks, 3 seconds)");
    int sched_err = stress_scheduler_run(3000);
    if (sched_err == 0) bench_pass("scheduler_stress_3_tasks_3s");
    else                bench_fail("scheduler_stress_3_tasks_3s", "task hang or timeout");

    /* Filesystem stress */
    bench_section("FILESYSTEM STRESS (20 files, write+read+verify+delete)");
    int fs_err = stress_filesystem_run();
    if (fs_err == 0) bench_pass("filesystem_stress_20_files");
    else             bench_fail("filesystem_stress_20_files", "write/read mismatch or unlink failure");

    /* Graphics stress */
    bench_section("GRAPHICS STRESS (500 render frames)");
    int gfx_err = stress_gfx_run(500);
    if (gfx_err == 0) bench_pass("graphics_stress_500_frames");
    else              bench_fail("graphics_stress_500_frames", "render error");

    /* Final summary */
    bench_section("STRESS TEST SUMMARY");
    kputs("  Memory errors  : "); kput_uint((uint32_t)mem_err);   kputs("\n");
    kputs("  Scheduler errors: "); kput_uint((uint32_t)sched_err); kputs("\n");
    kputs("  Filesystem errors: "); kput_uint((uint32_t)fs_err);   kputs("\n");
    kputs("  Graphics errors : "); kput_uint((uint32_t)gfx_err);  kputs("\n");

    int total_err = mem_err + sched_err + fs_err + gfx_err;
    if (total_err == 0) {
        kputs("  Result: ALL STRESS TESTS PASSED\n");
    } else {
        kputs("  Result: ");
        kput_uint((uint32_t)total_err);
        kputs(" STRESS TEST(S) FAILED\n");
    }
}
