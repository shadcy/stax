#ifndef FIRMWARE_BENCH_H
#define FIRMWARE_BENCH_H

#include "../bench.h"

void bench_secure_boot_run(void);
void bench_update_run(void);
void bench_fault_injection_run(void);
void bench_metadata_test_run(void);
void bench_image_test_run(void);
void bench_rollback_test_run(void);

void bench_firmware_run(void);

#endif
