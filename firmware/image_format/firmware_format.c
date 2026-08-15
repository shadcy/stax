#include "firmware_format.h"
#include "../../crypto/sha256/sha256.h"
#include "../../crypto/crc32/crc32.h"

// Memcmp implementation since we might not have string.h linked in the bootloader exactly as we expect,
// though bootloader does link string.o. We can just implement a simple one or use string.h.
static int fw_memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = s1, *p2 = s2;
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i])
            return p1[i] - p2[i];
    }
    return 0;
}

int firmware_validate(const firmware_header_t *header, const uint8_t *payload) {
    if (!header || !payload) return -1;

    // 1. Check Magic
    if (header->magic != FIRMWARE_MAGIC) {
        return -2; // Magic mismatch
    }

    // 2. Check Format Version
    if (header->format_ver != FIRMWARE_FORMAT_VERSION) {
        return -3; // Unsupported format version
    }

    // 3. Verify Header CRC32 (calculated over the first 28 bytes)
    uint32_t calc_crc = crc32((const uint8_t *)header, 28);
    if (calc_crc != header->crc32) {
        return -4; // Header CRC mismatch
    }

    // 4. Validate Payload Size (basic bounds check, prevent overflow or excessive size)
    // Assuming max slot size is 1MB - sizeof(header)
    if (header->image_size == 0 || header->image_size > (1024 * 1024 - 512)) {
        return -5; // Invalid size
    }

    // 5. Verify Payload Hash
    uint8_t hash[32];
    sha256(payload, header->image_size, hash);

    if (fw_memcmp(hash, header->payload_hash, 32) != 0) {
        return -6; // Hash mismatch
    }

    return 0;
}
