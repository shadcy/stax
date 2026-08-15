#include <stdio.h>
#include <string.h>
#include "../../firmware/image_format/firmware_format.h"
#include "../../crypto/crc32/crc32.h"
#include "../../crypto/sha256/sha256.h"

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT_EQ(actual, expected) do { \
    tests_run++; \
    if ((actual) == (expected)) { \
        tests_passed++; \
    } else { \
        printf("%s:%d: Assertion failed: %s == %s (Actual: %d, Expected: %d)\n", \
               __FILE__, __LINE__, #actual, #expected, (int)(actual), (int)(expected)); \
    } \
} while (0)

int main() {
    firmware_header_t header;
    memset(&header, 0, sizeof(header));
    uint8_t payload[256];
    memset(payload, 0xAA, sizeof(payload));

    header.magic = FIRMWARE_MAGIC;
    header.format_ver = FIRMWARE_FORMAT_VERSION;
    header.image_ver = 1;
    header.image_size = sizeof(payload);
    header.load_addr = 0x100000;
    header.entry_point = 0x100000;
    header.flags = 0;

    // Calculate CRC32 of first 28 bytes
    header.crc32 = crc32((const uint8_t *)&header, 28);

    // Calculate payload hash
    sha256(payload, sizeof(payload), header.payload_hash);

    printf("Running firmware_format tests...\n");

    // Test 1: Valid header
    ASSERT_EQ(firmware_validate(&header, payload), 0);

    // Test 2: Invalid magic
    header.magic = 0;
    ASSERT_EQ(firmware_validate(&header, payload), -2);
    header.magic = FIRMWARE_MAGIC;

    // Test 3: Invalid CRC32
    header.crc32 = 0;
    ASSERT_EQ(firmware_validate(&header, payload), -4);
    header.crc32 = crc32((const uint8_t *)&header, 28);

    // Test 4: Invalid Payload Hash
    header.payload_hash[0] ^= 0xFF;
    ASSERT_EQ(firmware_validate(&header, payload), -6);
    header.payload_hash[0] ^= 0xFF;

    printf("Tests run: %d, Passed: %d\n", tests_run, tests_passed);
    return tests_run == tests_passed ? 0 : 1;
}
