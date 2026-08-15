#ifndef FIRMWARE_FORMAT_H
#define FIRMWARE_FORMAT_H

#include <stdint.h>
#include <stddef.h>

#define FIRMWARE_MAGIC 0x46585453 // "STXF" (little endian)
#define FIRMWARE_FORMAT_VERSION 1

typedef struct {
    uint32_t magic;           // Must be FIRMWARE_MAGIC
    uint32_t format_ver;      // Must be FIRMWARE_FORMAT_VERSION
    uint32_t image_ver;       // Monotonic version
    uint32_t image_size;      // Size of payload
    uint32_t load_addr;       // Execution address
    uint32_t entry_point;     // Entry point
    uint32_t flags;           // Additional flags
    uint32_t crc32;           // CRC32 of header (excluding this and following fields, but wait, usually CRC is of the header up to the CRC field)
                              // Let's say CRC32 is calculated over the first 28 bytes (magic to flags).
    uint8_t  payload_hash[32]; // SHA-256 of payload
    uint8_t  signature[64];    // Ed25519 signature
} __attribute__((packed)) firmware_header_t;

// Validates the firmware header and payload hash
// Returns 0 on success, or negative error code.
int firmware_validate(const firmware_header_t *header, const uint8_t *payload);

#endif
