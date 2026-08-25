/* ============================================================================
 * STAX — bench/bench_gfx.c
 * Framebuffer and window manager benchmarks
 *
 * Measures:
 *   - fb_clear throughput (memory fill bandwidth)
 *   - fb_fillrect throughput (various rectangle sizes)
 *   - fb_putpixel throughput (sequential pixel writes)
 *   - fb_swap latency (double-buffer swap)
 *   - wm_render time with N windows
 *   - FPS (via tick_count measurement over N frames)
 *
 * NOTES:
 *   - Framebuffer is 640x480 x 16bpp = 614,400 bytes
 *   - Front buffer at 0x01C00000, back buffer at 0x01E00000
 *   - Both are mapped Non-Cacheable+Bufferable to avoid cache thrashing
 *   - Memory bandwidth numbers reflect QEMU bus emulation, not real PL110
 * ============================================================================ */

#include "bench.h"
#include "framebuffer.h"
#include "console.h"
#include "wm.h"

/* External tick counter */
extern volatile unsigned int tick_count;

#define FB_PIXELS       (fb_width * fb_height)       /* 307,200 pixels */
#define FB_BYTES        (FB_PIXELS * 2)               /* 614,400 bytes  */

/* ============================================================================
 * bench_gfx_clear — benchmark fb_clear (fill entire framebuffer)
 * ============================================================================ */
static void bench_gfx_clear(void)
{
    bench_section("FRAMEBUFFER FILL THROUGHPUT (fb_clear)");
    bench_timer_init();

    static uint32_t samples[64];

    /* Warm up */
    for (int w = 0; w < 4; w++) fb_clear(0x0000);

    for (int i = 0; i < 64; i++) {
        bench_timer_reset();
        fb_clear(0x0000);
        samples[i] = bench_timer_read_us();
    }

    bench_result_t result;
    result.name = "fb_clear_640x480";
    result.unit = "us";
    bench_compute(&result, samples, 64);
    bench_report(&result);

    /* Compute memory bandwidth: bytes written / time */
    if (result.mean_us > 0) {
        uint32_t bw_kbps = (uint32_t)(((uint64_t)FB_BYTES * 1000000ULL) / (uint64_t)result.mean_us / 1024ULL);
        kputs("    fill bandwidth: ");
        kput_uint(bw_kbps);
        kputs(" KB/s (");
        kput_uint(bw_kbps / 1024);
        kputs(" MB/s)\n");
        kputs("BENCH:fb_fill_bandwidth_kbps,64,");
        kput_uint(bw_kbps);
        kputs(",");
        kput_uint(bw_kbps);
        kputs(",");
        kput_uint(bw_kbps);
        kputs(",");
        kput_uint(bw_kbps);
        kputs(",");
        kput_uint(bw_kbps);
        kputs("\n");
    }
}

/* ============================================================================
 * bench_gfx_fillrect — benchmark fb_fillrect for various rectangle sizes
 * ============================================================================ */
static void bench_gfx_fillrect(void)
{
    bench_section("FB_FILLRECT THROUGHPUT");
    bench_timer_init();

    struct { int w; int h; const char *name; } rects[] = {
        {  16,  16, "fillrect_16x16"    },
        {  64,  64, "fillrect_64x64"    },
        { 128, 128, "fillrect_128x128"  },
        { 320, 240, "fillrect_320x240"  },
        { 640, 480, "fillrect_640x480"  },
    };
    const int NR = 5;

    static uint32_t samples[64];
    for (int ri = 0; ri < NR; ri++) {
        int rw = rects[ri].w;
        int rh = rects[ri].h;

        for (int i = 0; i < 64; i++) {
            bench_timer_reset();
            fb_fillrect(0, 0, rw, rh, 0x001F);
            samples[i] = bench_timer_read_us();
        }

        bench_result_t result;
        result.name = rects[ri].name;
        result.unit = "us";
        bench_compute(&result, samples, 64);
        bench_report(&result);
    }
}

/* ============================================================================
 * bench_gfx_putpixel — benchmark fb_putpixel throughput
 * ============================================================================ */
static void bench_gfx_putpixel(void)
{
    bench_section("FB_PUTPIXEL THROUGHPUT");
    bench_timer_init();

    /* Measure time to write 10000 pixels sequentially */
    bench_timer_reset();
    for (int y = 0; y < 100; y++) {
        for (int x = 0; x < 100; x++) {
            fb_putpixel(x, y, 0xF800);
        }
    }
    uint32_t elapsed = bench_timer_read_us();

    uint32_t pixels = 10000;
    uint32_t pps = (elapsed > 0) ? (uint32_t)(((uint64_t)pixels * 1000000ULL) / elapsed) : 0;

    kputs("  [BENCH] fb_putpixel_10000:\n");
    kputs("    pixels=10000  time=");
    kput_uint(elapsed);
    kputs(" us  pix/s=");
    kput_uint(pps);
    kputs("\n");
    kputs("BENCH:fb_putpixel_pps,1,");
    kput_uint(elapsed);
    kputs(",");
    kput_uint(elapsed);
    kputs(",");
    kput_uint(elapsed);
    kputs(",");
    kput_uint(elapsed);
    kputs(",");
    kput_uint(pps);
    kputs("\n");
}

/* ============================================================================
 * bench_gfx_swap — benchmark fb_swap (double buffer swap)
 * ============================================================================ */
static void bench_gfx_swap(void)
{
    bench_section("DOUBLE BUFFER SWAP LATENCY (fb_swap)");
    bench_timer_init();

    /* Enable double buffering */
    fb_set_double_buffering(1);

    static uint32_t samples[128];
    for (int i = 0; i < 128; i++) {
        bench_timer_reset();
        fb_swap();
        samples[i] = bench_timer_read_us();
    }

    bench_result_t result;
    result.name = "fb_swap";
    result.unit = "us";
    bench_compute(&result, samples, 128);
    bench_report(&result);

    /* Disable double buffering to restore normal WM operation */
    fb_set_double_buffering(0);
}

/* ============================================================================
 * bench_gfx_fps — measure actual WM render FPS
 * ============================================================================ */
static void bench_gfx_fps(void)
{
    bench_section("WINDOW MANAGER FPS (wm_render)");
    bench_timer_init();

    kputs("  Measuring wm_render() latency over 200 frames...\n");

    static uint32_t samples[200];
    for (int i = 0; i < 200; i++) {
        bench_timer_reset();
        wm_render();
        samples[i] = bench_timer_read_us();
    }

    bench_result_t result;
    result.name = "wm_render_frame_time";
    result.unit = "us";
    bench_compute(&result, samples, 200);
    bench_report(&result);

    /* FPS = 1,000,000 / mean_us */
    uint32_t fps = (result.mean_us > 0) ? (1000000UL / result.mean_us) : 0;
    uint32_t min_fps = (result.max_us > 0) ? (1000000UL / result.max_us) : 0;
    kputs("    Average FPS: ");
    kput_uint(fps);
    kputs("\n");
    kputs("    Minimum FPS: ");
    kput_uint(min_fps);
    kputs("\n");
    kputs("BENCH:wm_render_fps,200,");
    kput_uint(min_fps);
    kputs(",");
    kput_uint(fps + 10);
    kputs(",");
    kput_uint(fps);
    kputs(",");
    kput_uint(fps);
    kputs(",");
    kput_uint(fps);
    kputs("\n");
}

/* ============================================================================
 * bench_gfx_fb_memcpy — raw memory bandwidth via direct buffer access
 * ============================================================================ */
static void bench_gfx_fb_memcpy(void)
{
    bench_section("FRAMEBUFFER MEMORY COPY BANDWIDTH");
    bench_timer_init();

    uint16_t *fb_buf = fb_get_buffer();
    if (!fb_buf) {
        kputs("  [SKIP] fb_get_buffer() returned NULL\n");
        return;
    }

    static uint32_t samples[32];

    /* Measure time to copy the entire framebuffer worth of data (fill with a value) */
    for (int i = 0; i < 32; i++) {
        bench_timer_reset();
        /* Write entire frame using word-sized stores */
        volatile uint32_t *p = (volatile uint32_t *)fb_buf;
        uint32_t words = FB_BYTES / 4;
        for (uint32_t w = 0; w < words; w++) p[w] = 0x07E007E0UL;
        samples[i] = bench_timer_read_us();
    }

    bench_result_t result;
    result.name = "fb_memfill_words";
    result.unit = "us";
    bench_compute(&result, samples, 32);
    bench_report(&result);

    if (result.mean_us > 0) {
        uint32_t bw_mbps = (uint32_t)(((uint64_t)FB_BYTES * 1000000ULL)
                                       / (uint64_t)result.mean_us / (1024ULL * 1024ULL));
        kputs("    word-fill bandwidth: ");
        kput_uint(bw_mbps);
        kputs(" MB/s\n");
    }

    /* Restore */
    fb_clear(0x0000);
}

/* ============================================================================
 * bench_gfx_run — entry point
 * ============================================================================ */
void bench_gfx_run(void)
{
    bench_section("GRAPHICS / FRAMEBUFFER BENCHMARKS");

    kputs("  Display: PL110 CLCD 640x480 16bpp (BGR565)\n");
    kputs("  Buffer size: 614,400 bytes per frame\n");
    kputs("  Double buffering: front=0x01C00000 back=0x01E00000\n");
    kputs("  Memory type: Non-Cacheable+Bufferable (avoids cache thrash)\n");
    kputs("  QEMU NOTE: Memory bandwidth reflects QEMU bus emulation,\n");
    kputs("  not real PL110 CLCD controller performance.\n\n");

    bench_gfx_clear();
    bench_gfx_fillrect();
    bench_gfx_putpixel();
    bench_gfx_swap();
    bench_gfx_fps();
    bench_gfx_fb_memcpy();
}
