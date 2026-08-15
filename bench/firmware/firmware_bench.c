#include "firmware_bench.h"

void bench_firmware_run(void)
{
    bench_section("FIRMWARE BENCHMARKS");

    bench_metadata_test_run();
    bench_image_test_run();
    bench_rollback_test_run();
    bench_secure_boot_run();
    bench_update_run();
    bench_fault_injection_run();
    bench_fault_campaign_run();
}
