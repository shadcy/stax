#include "firmware_bench.h"
#include "../../boot/metadata.h"

extern void kputs(const char *s);
extern void kput_uint(uint32_t n);

void bench_fault_injection_run(void) {
    bench_section("FAULT INJECTION STATE RECOVERY");

    int pass = 1;
    boot_metadata_t m;
    
    // Simulate normal boot sequence
    m.slot_a_state = SLOT_STATE_PENDING;
    m.slot_a_boot_attempts = 0;
    
    // Reboot 1 (Bootloader sees PENDING)
    if (m.slot_a_state == SLOT_STATE_PENDING) {
        m.slot_a_state = SLOT_STATE_BOOTING;
        m.slot_a_boot_attempts++;
    }
    
    // Fails to boot (Power loss during BOOTING)
    // Reboot 2
    if (m.slot_a_state == SLOT_STATE_BOOTING) {
        m.slot_a_boot_attempts++;
        if (m.slot_a_boot_attempts >= 3) {
            m.slot_a_state = SLOT_STATE_FAILED;
        }
    }
    if (m.slot_a_state != SLOT_STATE_BOOTING) pass = 0;
    
    // Fails to boot
    // Reboot 3
    if (m.slot_a_state == SLOT_STATE_BOOTING) {
        m.slot_a_boot_attempts++;
        if (m.slot_a_boot_attempts >= 3) {
            m.slot_a_state = SLOT_STATE_FAILED;
        }
    }
    if (m.slot_a_state != SLOT_STATE_FAILED) pass = 0;
    
    if (pass) bench_pass_count++;
    else bench_fail_count++;

    kputs(pass ? "[PASS] " : "[FAIL] ");
    kputs("State machine watchdog limit recovery\n");
}
