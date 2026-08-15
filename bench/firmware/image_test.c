#include "firmware_bench.h"
#include "../../firmware/image_format/firmware_format.h"

extern void kputs(const char *s);
extern void kput_uint(uint32_t n);

static uint8_t dummy_payload[256];
static firmware_header_t valid_header;

static void setup_valid_header(void) {
    valid_header.magic = FIRMWARE_MAGIC;
    valid_header.format_ver = FIRMWARE_FORMAT_VERSION;
    valid_header.image_ver = 1;
    valid_header.image_size = sizeof(dummy_payload);
    valid_header.load_addr = 0;
    valid_header.entry_point = 0;
    valid_header.flags = 0;
    
    /* We skip calculating actual SHA256 and CRC32 here because we mock the validation
       or we expect validation to fail at specific steps. */
}

void bench_image_test_run(void) {
    bench_section("FIRMWARE IMAGE VALIDATION (FUZZING)");
    
    bench_result_t res;
    int pass = 1;
    int ret;

    /* 1. Magic mismatch */
    setup_valid_header();
    valid_header.magic = 0xDEADBEEF;
    ret = firmware_validate(&valid_header, dummy_payload);
    if (ret != -2) pass = 0;

    /* 2. Format Version mismatch */
    setup_valid_header();
    valid_header.format_ver = 999;
    ret = firmware_validate(&valid_header, dummy_payload);
    if (ret != -3) pass = 0;

    /* 3. Header CRC mismatch */
    setup_valid_header();
    valid_header.crc32 = 0x12345678;
    ret = firmware_validate(&valid_header, dummy_payload);
    if (ret != -4) pass = 0;

    /* 4. Invalid size */
    setup_valid_header();
    /* recalculate crc manually for the size test so we pass CRC check */
    valid_header.image_size = 0xFFFFFFFF;
    extern uint32_t crc32(const uint8_t *data, size_t length);
    valid_header.crc32 = crc32((const uint8_t *)&valid_header, 28);
    ret = firmware_validate(&valid_header, dummy_payload);
    if (ret != -5) pass = 0;

    if (pass) bench_pass_count++;
    else bench_fail_count++;

    kputs(pass ? "[PASS] " : "[FAIL] ");
    kputs("Image validation rejection logic\n");
}
