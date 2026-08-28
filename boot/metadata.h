/**
 * @file    metadata.h
 * @author  shadcy
 * @brief   A/B dual-slot firmware metadata structures and slot definitions.
 *
 * Part of the STAX Operating System.
 *
 * @license GPL-3.0-or-later
 * Copyright (c) 2026 Shreyash Wanjari (Shadcy)
 */

#ifndef METADATA_H
#define METADATA_H

#include <stdint.h>

#define METADATA_MAGIC 0x4D455441 // "META"

// Firmware Slot States
#define SLOT_STATE_EMPTY       0
#define SLOT_STATE_DOWNLOADING 1
#define SLOT_STATE_VERIFIED    2
#define SLOT_STATE_PENDING     3
#define SLOT_STATE_BOOTING     4
#define SLOT_STATE_CONFIRMED   5
#define SLOT_STATE_FAILED      6

typedef struct {
    uint32_t magic;
    uint32_t generation;      // Monotonically increasing counter for A/B sync
    uint32_t active_slot;     // 0 for Slot A, 1 for Slot B
    
    // Slot A info
    uint32_t slot_a_state;
    uint32_t slot_a_version;
    uint32_t slot_a_boot_attempts;
    
    // Slot B info
    uint32_t slot_b_state;
    uint32_t slot_b_version;
    uint32_t slot_b_boot_attempts;
    
    uint32_t crc32;           // CRC32 of everything above
} __attribute__((packed)) boot_metadata_t;

#endif
