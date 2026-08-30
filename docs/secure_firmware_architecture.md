# STAX Secure Firmware Architecture

## 1. Current STAX Architecture
STAX OS is a bare-metal OS running on an ARM926EJ-S processor (simulated via QEMU VersatilePB).
- **Bootloader (`bootloader.c`)**: Loaded directly into memory at `0x10000` by QEMU using the `-kernel` flag. It initializes the PL181 SD Card controller, parses the FAT16 filesystem on the SD card (`os.bin`), finds `KERNEL.BIN`, loads it to `0x100000`, verifies a magic number (`STAX`), and jumps to it.
- **Kernel (`kernel.c`)**: Linked to run at `0x100000`. It initializes MMU, exception vectors, scheduler, and FatFs for filesystem access.
- **Storage**: `os.bin` is attached as an SD card. It is currently formatted purely as FAT16.

## 2. Proposed Secure-Boot Architecture
```text
                    Simulated Boot ROM (QEMU -kernel)
                           │
                           ▼
                  Secure STAX Bootloader
                           │
                 Read Boot Metadata (Redundant)
                           │
                    Verify Slot State
                           │
                ┌──────────┴──────────┐
                │                     │
                ▼                     ▼
             Slot A                Slot B
          Firmware vN           Firmware vN-1
                │                     │
                └──────────┬──────────┘
                           │
                     Select Valid
                        Firmware
                           │
                     Verify Image
                           │
                  Verify Signature (Ed25519)
                           │
                  Check Version (Anti-rollback)
                           │
                     Boot Kernel
                           │
                     STAX OS
```

## 3. Proposed Flash Memory Map
The SD Card (`os.bin`) will be partitioned to emulate raw flash for the firmware slots, leaving the remainder for the FAT16 filesystem.
- **Sector Size**: 512 bytes.
- **Total Size**: 32 MB.

| Region | LBA Range | Size | Description |
|---|---|---|---|
| Metadata A | LBA 0 | 512 B | Boot metadata copy A |
| Metadata B | LBA 1 | 512 B | Boot metadata copy B (redundant) |
| Slot A | LBA 2 - 2049 | 1 MB | Firmware Slot A |
| Slot B | LBA 2050 - 4097 | 1 MB | Firmware Slot B |
| FAT16 FS | LBA 4098 - 65535 | ~30 MB | User filesystem |

*Note: The FatFs mount will be modified to use an offset to skip the firmware regions.*

## 4. Firmware Image Format
A custom header prepended to the STAX OS binary.

| Offset | Field | Size | Description |
|---|---|---|---|
| 0x00 | `magic` | 4 bytes | `STXF` |
| 0x04 | `format_ver`| 4 bytes | Version of this header format (1) |
| 0x08 | `image_ver` | 4 bytes | Monotonic version for anti-rollback |
| 0x0C | `image_size`| 4 bytes | Size of the payload |
| 0x10 | `load_addr` | 4 bytes | Target execution address (`0x100000`) |
| 0x14 | `entry_point`| 4 bytes | Usually same as `load_addr` |
| 0x18 | `flags` | 4 bytes | Additional metadata |
| 0x1C | `crc32` | 4 bytes | CRC32 of header |
| 0x20 | `payload_hash`| 32 bytes | SHA-256 hash of the payload |
| 0x40 | `signature` | 64 bytes | Ed25519 signature of the hash |

## 5. Update State Machine
Each slot has an associated state in the metadata:
1. **EMPTY**: Slot is empty or contains garbage.
2. **DOWNLOADING**: OS is currently writing to the slot.
3. **VERIFIED**: OS has finished writing and verified the local hash.
4. **PENDING**: Bootloader will attempt to boot this slot on next reboot.
5. **BOOTING**: Bootloader has jumped to this slot. Watchdog armed.
6. **CONFIRMED**: OS has successfully booted and called `stax_firmware_confirm()`.
7. **FAILED**: Slot failed to boot (watchdog reset or manual fallback).

## 6. Threat Model
- **Interrupted Update**: Power loss during writing. *Mitigation*: Active slot untouched, state transitions are atomic, redundant metadata.
- **Corrupted Firmware**: Bit flips in storage. *Mitigation*: SHA-256 payload hash and CRC32 header check before boot.
- **Modified/Malicious Firmware**: *Mitigation*: Ed25519 signature verification using a hardcoded public key in the bootloader.
- **Rollback Attack**: Flashing an older, vulnerable but properly signed image. *Mitigation*: Monotonic version counter in metadata.
- **Metadata Corruption**: *Mitigation*: Two copies of metadata (A & B) with generation counters and CRC32.

## 7. QEMU Assumptions & Limitations
- **Hardware Root of Trust**: QEMU does not provide a true hardware root of trust. The Bootloader is trusted implicitly because it is loaded via QEMU's `-kernel` flag.
- **Flash Erase**: PL181 SD cards don't have true "erase-before-write" semantics like NOR/NAND flash. We will simulate this limitation in the fault-injection framework.
- **Power Loss**: Simulated via QEMU termination or fault-injection logic triggering early resets.

## 8. Test Strategy
- **Unit Tests**: Test SHA-256, Ed25519, Image Parsing.
- **Integration Tests**: Verify successful update cycle.
- **Fault Injection Framework**: A Python wrapper around QEMU that injects resets at specific block write numbers (e.g., `--fail-after 173`).
- **10,000+ Iterations**: Automate 10,000 updates with deterministic random failure points to prove the "zero unrecoverable failure" invariant.
