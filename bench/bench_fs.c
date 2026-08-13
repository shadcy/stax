/* ============================================================================
 * STAX — bench/bench_fs.c
 * FAT16 filesystem benchmarks using FatFs
 *
 * Measures:
 *   - File open/close latency
 *   - File creation latency (f_open with FA_CREATE_ALWAYS)
 *   - File deletion latency (f_unlink)
 *   - Sequential read throughput (bytes/sec)
 *   - Sequential write throughput (bytes/sec)
 *   - Multiple file sizes: 1KB, 4KB, 16KB, 64KB
 *
 * QEMU DISCLAIMER:
 *   All storage I/O is emulated by QEMU's PL181 SD controller model.
 *   Results reflect QEMU I/O emulation performance, NOT physical SD card
 *   or eMMC performance. Physical hardware would show completely different
 *   (typically lower) throughput due to actual SD bus timing.
 *
 * SAFETY:
 *   All test files use the name "BNCH.TMP" and are deleted after each test.
 *   If a previous run crashed, stale BNCH.TMP files may exist — we unlink
 *   them at the start of each test.
 * ============================================================================ */

#include "bench.h"
#include "fat.h"
#include "console.h"
#include "heap.h"

/* FatFs functions are available globally through fat.h -> fatfs/ff.h */

#define BENCH_FILENAME  "BNCH.TMP"
#define BENCH_FILENAME2 "BNCH2.TMP"

/* I/O buffer size for throughput tests */
#define IO_CHUNK_SIZE   512

/* Test file sizes */
#define NUM_FILE_SIZES  4
static const uint32_t file_sizes[NUM_FILE_SIZES] = {
    1024,           /* 1 KB  */
    4096,           /* 4 KB  */
    16384,          /* 16 KB */
    65536           /* 64 KB */
};
static const char *fsize_names[NUM_FILE_SIZES] = {
    "1kb", "4kb", "16kb", "64kb"
};

/* ============================================================================
 * fill_pattern — fill buffer with a deterministic pattern for verification
 * ============================================================================ */
static void fill_pattern(uint8_t *buf, uint32_t size, uint32_t seed)
{
    for (uint32_t i = 0; i < size; i++) {
        buf[i] = (uint8_t)((seed + i * 0x6D) & 0xFF);
    }
}

static int verify_pattern(const uint8_t *buf, uint32_t size, uint32_t seed)
{
    for (uint32_t i = 0; i < size; i++) {
        if (buf[i] != (uint8_t)((seed + i * 0x6D) & 0xFF))
            return 0;
    }
    return 1;
}

/* ============================================================================
 * bench_fs_open_close — measure f_open + f_close latency on existing file
 * ============================================================================ */
static void bench_fs_open_close(void)
{
    bench_section("FILESYSTEM OPEN/CLOSE LATENCY");
    bench_timer_init();

    /* Create a small file to open */
    FIL f;
    FRESULT res;
    UINT bw;

    static uint8_t wbuf[IO_CHUNK_SIZE];
    fill_pattern(wbuf, IO_CHUNK_SIZE, 0xAA);

    res = f_open(&f, BENCH_FILENAME, FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK) {
        kputs("  [WARN] Cannot create test file for open/close bench\n");
        return;
    }
    f_write(&f, wbuf, IO_CHUNK_SIZE, &bw);
    f_close(&f);

    /* Measure f_open latency */
    static uint32_t open_samples[64];
    for (int i = 0; i < 64; i++) {
        bench_timer_reset();
        res = f_open(&f, BENCH_FILENAME, FA_READ);
        open_samples[i] = bench_timer_read_us();
        if (res == FR_OK) f_close(&f);
    }

    bench_result_t result;
    result.name = "fat_fopen";
    result.unit = "us";
    bench_compute(&result, open_samples, 64);
    bench_report(&result);

    /* Measure f_close latency */
    static uint32_t close_samples[64];
    for (int i = 0; i < 64; i++) {
        f_open(&f, BENCH_FILENAME, FA_READ);
        bench_timer_reset();
        f_close(&f);
        close_samples[i] = bench_timer_read_us();
    }

    result.name = "fat_fclose";
    result.unit = "us";
    bench_compute(&result, close_samples, 64);
    bench_report(&result);

    f_unlink(BENCH_FILENAME);
}

/* ============================================================================
 * bench_fs_create_delete — measure file creation and deletion latency
 * ============================================================================ */
static void bench_fs_create_delete(void)
{
    bench_section("FILE CREATE/DELETE LATENCY");
    bench_timer_init();

    FIL f;
    FRESULT res;
    static uint32_t create_samples[32];
    static uint32_t delete_samples[32];

    for (int i = 0; i < 32; i++) {
        bench_timer_reset();
        res = f_open(&f, BENCH_FILENAME, FA_CREATE_ALWAYS | FA_WRITE);
        create_samples[i] = bench_timer_read_us();
        if (res == FR_OK) f_close(&f);

        bench_timer_reset();
        f_unlink(BENCH_FILENAME);
        delete_samples[i] = bench_timer_read_us();
    }

    bench_result_t result;
    result.name = "fat_create";
    result.unit = "us";
    bench_compute(&result, create_samples, 32);
    bench_report(&result);

    result.name = "fat_delete";
    result.unit = "us";
    bench_compute(&result, delete_samples, 32);
    bench_report(&result);
}

/* ============================================================================
 * bench_fs_sequential_write — measure write throughput for each file size
 * ============================================================================ */
static void bench_fs_seq_write(uint32_t file_size, const char *size_name)
{
    FIL f;
    FRESULT res;
    UINT bw;

    /* Allocate I/O buffer from heap */
    uint8_t *buf = (uint8_t *)kmalloc(IO_CHUNK_SIZE);
    if (!buf) {
        kputs("  [WARN] Cannot allocate I/O buffer\n");
        return;
    }
    fill_pattern(buf, IO_CHUNK_SIZE, 0x5A);

    /* Remove stale file */
    f_unlink(BENCH_FILENAME);

    /* Create and write */
    bench_timer_reset();
    res = f_open(&f, BENCH_FILENAME, FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK) {
        kputs("  [WARN] Cannot open for write\n");
        kfree(buf);
        return;
    }

    uint32_t written = 0;
    while (written < file_size) {
        uint32_t chunk = IO_CHUNK_SIZE;
        if (chunk > file_size - written) chunk = file_size - written;
        res = f_write(&f, buf, chunk, &bw);
        if (res != FR_OK || bw == 0) break;
        written += bw;
    }
    f_close(&f);
    uint32_t elapsed = bench_timer_read_us();

    kfree(buf);
    f_unlink(BENCH_FILENAME);

    if (elapsed == 0) elapsed = 1;

    /* throughput in bytes/sec */
    uint32_t throughput_bps = (uint32_t)(((uint64_t)written * 1000000ULL) / elapsed);
    uint32_t throughput_kbps = throughput_bps / 1024;

    char rname[32];
    int ri = 0;
    const char *prefix = "fat_write_seq_";
    while (*prefix) rname[ri++] = *prefix++;
    const char *n = size_name;
    while (*n) rname[ri++] = *n++;
    rname[ri] = '\0';

    kputs("  [BENCH] ");
    kputs(rname);
    kputs(":\n");
    kputs("    written=");
    kput_uint(written);
    kputs(" bytes  time=");
    kput_uint(elapsed);
    kputs(" us  throughput=");
    kput_uint(throughput_kbps);
    kputs(" KB/s\n");
    kputs("BENCH:");
    kputs(rname);
    kputs(",1,");
    kput_uint(elapsed);
    kputs(",");
    kput_uint(elapsed);
    kputs(",");
    kput_uint(elapsed);
    kputs(",");
    kput_uint(elapsed);
    kputs(",");
    kput_uint(throughput_bps);
    kputc('\n');
}

/* ============================================================================
 * bench_fs_sequential_read — write then read back, measure read throughput
 * ============================================================================ */
static void bench_fs_seq_read(uint32_t file_size, const char *size_name)
{
    FIL f;
    FRESULT res;
    UINT br, bw;

    uint8_t *wbuf = (uint8_t *)kmalloc(IO_CHUNK_SIZE);
    uint8_t *rbuf = (uint8_t *)kmalloc(IO_CHUNK_SIZE);
    if (!wbuf || !rbuf) {
        if (wbuf) kfree(wbuf);
        if (rbuf) kfree(rbuf);
        kputs("  [WARN] Cannot allocate I/O buffers\n");
        return;
    }
    fill_pattern(wbuf, IO_CHUNK_SIZE, 0x3C);

    /* Write test file */
    f_unlink(BENCH_FILENAME);
    res = f_open(&f, BENCH_FILENAME, FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK) {
        kfree(wbuf); kfree(rbuf);
        return;
    }
    uint32_t written = 0;
    while (written < file_size) {
        uint32_t chunk = IO_CHUNK_SIZE;
        if (chunk > file_size - written) chunk = file_size - written;
        f_write(&f, wbuf, chunk, &bw);
        written += bw;
    }
    f_close(&f);

    /* Measure read */
    bench_timer_reset();
    res = f_open(&f, BENCH_FILENAME, FA_READ);
    if (res != FR_OK) {
        kfree(wbuf); kfree(rbuf);
        f_unlink(BENCH_FILENAME);
        return;
    }

    uint32_t total_read = 0;
    int verify_ok = 1;
    while (1) {
        res = f_read(&f, rbuf, IO_CHUNK_SIZE, &br);
        if (res != FR_OK || br == 0) break;
        /* Verify pattern for first chunk only to avoid benchmark overhead */
        if (total_read == 0 && !verify_pattern(rbuf, br, 0x3C))
            verify_ok = 0;
        total_read += br;
    }
    f_close(&f);
    uint32_t elapsed = bench_timer_read_us();

    kfree(wbuf);
    kfree(rbuf);
    f_unlink(BENCH_FILENAME);

    if (elapsed == 0) elapsed = 1;
    uint32_t throughput_bps = (uint32_t)(((uint64_t)total_read * 1000000ULL) / elapsed);
    uint32_t throughput_kbps = throughput_bps / 1024;

    char rname[32];
    int ri = 0;
    const char *prefix = "fat_read_seq_";
    while (*prefix) rname[ri++] = *prefix++;
    const char *n = size_name;
    while (*n) rname[ri++] = *n++;
    rname[ri] = '\0';

    kputs("  [BENCH] ");
    kputs(rname);
    kputs(":\n");
    kputs("    read=");
    kput_uint(total_read);
    kputs(" bytes  time=");
    kput_uint(elapsed);
    kputs(" us  throughput=");
    kput_uint(throughput_kbps);
    kputs(" KB/s");
    if (!verify_ok) kputs("  [WARN: verify failed]");
    kputs("\n");

    kputs("BENCH:");
    kputs(rname);
    kputs(",1,");
    kput_uint(elapsed);
    kputs(",");
    kput_uint(elapsed);
    kputs(",");
    kput_uint(elapsed);
    kputs(",");
    kput_uint(elapsed);
    kputs(",");
    kput_uint(throughput_bps);
    kputc('\n');
}

/* ============================================================================
 * bench_fs_dir_traversal — measure directory listing performance
 * ============================================================================ */
static void bench_fs_dir_traversal(void)
{
    bench_section("DIRECTORY TRAVERSAL");
    bench_timer_init();

    static uint32_t samples[16];
    for (int i = 0; i < 16; i++) {
        DIR dir;
        FILINFO fno;
        int count = 0;

        bench_timer_reset();
        FRESULT res = f_opendir(&dir, "/");
        if (res == FR_OK) {
            while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] != 0)
                count++;
            f_closedir(&dir);
        }
        samples[i] = bench_timer_read_us();
        (void)count;
    }

    bench_result_t result;
    result.name = "fat_readdir_root";
    result.unit = "us";
    bench_compute(&result, samples, 16);
    bench_report(&result);
}

/* ============================================================================
 * bench_fs_run — entry point
 * ============================================================================ */
void bench_fs_run(void)
{
    bench_section("FILESYSTEM BENCHMARKS (FAT16 via FatFs + PL181)");

    kputs("  QEMU DISCLAIMER: Results reflect QEMU PL181 SD emulation\n");
    kputs("  speed, NOT physical SD card or eMMC performance.\n");
    kputs("  Physical hardware will show significantly different numbers.\n\n");

    bench_fs_open_close();
    bench_fs_create_delete();
    bench_fs_dir_traversal();

    bench_section("SEQUENTIAL WRITE THROUGHPUT");
    for (int i = 0; i < NUM_FILE_SIZES; i++)
        bench_fs_seq_write(file_sizes[i], fsize_names[i]);

    bench_section("SEQUENTIAL READ THROUGHPUT");
    for (int i = 0; i < NUM_FILE_SIZES; i++)
        bench_fs_seq_read(file_sizes[i], fsize_names[i]);
}
