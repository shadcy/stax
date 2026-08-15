#include "firmware_bench.h"
#include "../../fs/fatfs/ff.h"

extern void kputs(const char *s);
extern void kput_uint(uint32_t n);

#define MOCK_FW_SIZE (128 * 1024) /* 128 KB */
static uint8_t write_buf[4096];

void bench_update_run(void) {
    bench_section("FIRMWARE UPDATE LATENCY");

    bench_result_t r_write;
    
    for (int i = 0; i < 4096; i++) write_buf[i] = 0xAA;

    BENCH_RUN(&r_write, 2, {
        FIL file;
        FRESULT fr = f_open(&file, "fwbench.tmp", FA_WRITE | FA_CREATE_ALWAYS);
        if (fr == FR_OK) {
            UINT bw;
            for (int i = 0; i < MOCK_FW_SIZE / 4096; i++) {
                f_write(&file, write_buf, 4096, &bw);
            }
            f_close(&file);
            f_unlink("fwbench.tmp");
        }
    });

    r_write.name = "fw_stage_128kb";
    r_write.unit = "us";
    bench_report(&r_write);
}
