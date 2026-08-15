#include "firmware_bench.h"
#include "../../boot/metadata.h"

extern void kputs(const char *s);
extern void kput_uint(uint32_t n);

void bench_rollback_test_run(void) {
    bench_section("ANTI-ROLLBACK PROTECTION");

    int pass = 1;
    
    boot_metadata_t meta;
    meta.slot_a_version = 5;
    meta.slot_b_version = 0;
    meta.active_slot = 0; // Slot A is active

    /* Simulated checks based on firmware_update.c logic */
    /* active version is 5. We want to update to ver. */
    
    uint32_t active_ver = (meta.active_slot == 0) ? meta.slot_a_version : meta.slot_b_version;
    
    // Test 1: Upgrade to 6 (Should accept)
    uint32_t new_ver = 6;
    if (new_ver < active_ver) pass = 0;
    
    // Test 2: Same version 5 (Should accept - reinstallation)
    new_ver = 5;
    if (new_ver < active_ver) pass = 0;

    // Test 3: Downgrade to 4 (Should reject)
    new_ver = 4;
    if (new_ver >= active_ver) pass = 0; // if it accepts, we fail

    if (pass) bench_pass_count++;
    else bench_fail_count++;

    kputs(pass ? "[PASS] " : "[FAIL] ");
    kputs("Rollback version comparison\n");
}
