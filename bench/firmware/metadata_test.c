#include "firmware_bench.h"
#include "../../boot/metadata.h"
#include "../../crypto/crc32/crc32.h"

extern void kputs(const char *s);
extern void kput_uint(uint32_t n);
extern void* memset(void* dest, int c, size_t count);

static boot_metadata_t meta_a;
static boot_metadata_t meta_b;

/* Returns 0 if active slot is A, 1 if B, -1 if fallback to default */
static int evaluate_slots(void) {
    int a_valid = 0, b_valid = 0;
    
    if (meta_a.magic == METADATA_MAGIC && crc32((const uint8_t *)&meta_a, sizeof(boot_metadata_t) - 4) == meta_a.crc32) a_valid = 1;
    if (meta_b.magic == METADATA_MAGIC && crc32((const uint8_t *)&meta_b, sizeof(boot_metadata_t) - 4) == meta_b.crc32) b_valid = 1;

    if (a_valid && b_valid) {
        if (meta_a.generation >= meta_b.generation) return meta_a.active_slot;
        else return meta_b.active_slot;
    } else if (a_valid) {
        return meta_a.active_slot;
    } else if (b_valid) {
        return meta_b.active_slot;
    } else {
        return -1; // defaults init
    }
}

static void update_crc(boot_metadata_t *m) {
    m->crc32 = crc32((const uint8_t *)m, sizeof(boot_metadata_t) - 4);
}

void bench_metadata_test_run(void) {
    bench_section("METADATA RECOVERY");

    int pass = 1;

    memset(&meta_a, 0, sizeof(boot_metadata_t));
    memset(&meta_b, 0, sizeof(boot_metadata_t));

    /* Test 1: Both invalid -> Default fallback */
    if (evaluate_slots() != -1) pass = 0;

    /* Test 2: A valid, B invalid -> Pick A's active slot */
    meta_a.magic = METADATA_MAGIC;
    meta_a.generation = 2;
    meta_a.active_slot = 0;
    update_crc(&meta_a);
    if (evaluate_slots() != 0) pass = 0;

    /* Test 3: A invalid, B valid -> Pick B's active slot */
    meta_a.magic = 0; // corrupt A
    meta_b.magic = METADATA_MAGIC;
    meta_b.generation = 5;
    meta_b.active_slot = 1;
    update_crc(&meta_b);
    if (evaluate_slots() != 1) pass = 0;

    /* Test 4: Both valid, B higher generation -> Pick B's active slot */
    meta_a.magic = METADATA_MAGIC;
    meta_a.generation = 3;
    meta_a.active_slot = 0;
    update_crc(&meta_a);
    if (evaluate_slots() != 1) pass = 0;

    if (pass) bench_pass_count++;
    else bench_fail_count++;

    kputs(pass ? "[PASS] " : "[FAIL] ");
    kputs("Metadata fallback evaluation\n");
}
