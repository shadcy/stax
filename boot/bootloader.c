#include "memory_map.h"
#include <stdint.h>

#define UART0_BASE   0x101f1000UL
#define UART_DR      (*(volatile unsigned int *)(UART0_BASE + 0x000))
#define UART_FR      (*(volatile unsigned int *)(UART0_BASE + 0x018))
#define UART_IBRD    (*(volatile unsigned int *)(UART0_BASE + 0x024))
#define UART_FBRD    (*(volatile unsigned int *)(UART0_BASE + 0x028))
#define UART_LCRH    (*(volatile unsigned int *)(UART0_BASE + 0x02C))
#define UART_CR      (*(volatile unsigned int *)(UART0_BASE + 0x030))

static void uart_init(void) {
    UART_CR = 0; UART_IBRD = 13; UART_FBRD = 1;
    UART_LCRH = (0x3 << 5) | (1 << 4);
    UART_CR = (1 << 0) | (1 << 8) | (1 << 9);
}

static void uart_puts(const char *s) {
    while (*s) {
        if (*s == '\n') {
            while (UART_FR & (1 << 5));
            UART_DR = '\r';
        }
        while (UART_FR & (1 << 5));
        UART_DR = *s++;
    }
}

static void uart_put_hex(uint32_t n) {
    char hex[] = "0123456789ABCDEF";
    uart_puts("0x");
    for (int i = 28; i >= 0; i -= 4) {
        UART_DR = hex[(n >> i) & 0xF];
    }
}

/* ============================================================================
 * PL181 PrimeCell MMC/SD Controller — ARM VersatilePB @ 0x10005000
 * Proper SD-card protocol (NOT MMC): CMD0 → CMD8 → ACMD41 → CMD2 → CMD3
 *   → CMD7 → CMD16 → CMD17 (read blocks)
 * ============================================================================ */
#define PL181_BASE   0x10005000UL
#define MCI_POWER    (*(volatile uint32_t *)(PL181_BASE + 0x00))
#define MCI_CLOCK    (*(volatile uint32_t *)(PL181_BASE + 0x04))
#define MCI_ARG      (*(volatile uint32_t *)(PL181_BASE + 0x08))
#define MCI_CMD      (*(volatile uint32_t *)(PL181_BASE + 0x0C))
#define MCI_RESP0    (*(volatile uint32_t *)(PL181_BASE + 0x14))
#define MCI_DTIMER   (*(volatile uint32_t *)(PL181_BASE + 0x24))
#define MCI_DLEN     (*(volatile uint32_t *)(PL181_BASE + 0x28))
#define MCI_DCTRL    (*(volatile uint32_t *)(PL181_BASE + 0x2C))
#define MCI_STATUS   (*(volatile uint32_t *)(PL181_BASE + 0x34))
#define MCI_CLEAR    (*(volatile uint32_t *)(PL181_BASE + 0x38))
#define MCI_FIFO     (*(volatile uint32_t *)(PL181_BASE + 0x80))

/* MCI_CMD register flag bits */
#define CMD_WAITRESP (1 << 6)   /* expect short response */
#define CMD_LONGRSP  (1 << 7)   /* expect long (136-bit) response */
#define CMD_ENABLE   (1 << 10)  /* CPSMEN — clock out the command */

/* MCI_STATUS bits we care about */
#define ST_CMDCRCFAIL (1 << 0)
#define ST_CMDTIMEOUT (1 << 2)
#define ST_CMDRESPEND (1 << 6)
#define ST_CMDSENT    (1 << 7)

/* ----- low-level helpers -------------------------------------------------- */

static void sd_cmd_norsp(uint32_t idx, uint32_t arg) {
    MCI_CLEAR = 0xFFF;
    MCI_ARG   = arg;
    MCI_CMD   = (idx & 0x3F) | CMD_ENABLE;
    while (!(MCI_STATUS & (ST_CMDSENT | ST_CMDTIMEOUT)));
}

static uint32_t sd_cmd_short(uint32_t idx, uint32_t arg) {
    MCI_CLEAR = 0xFFF;
    MCI_ARG   = arg;
    MCI_CMD   = (idx & 0x3F) | CMD_WAITRESP | CMD_ENABLE;
    while (!(MCI_STATUS & (ST_CMDRESPEND | ST_CMDCRCFAIL | ST_CMDTIMEOUT)));
    return MCI_RESP0;
}

static void sd_cmd_long(uint32_t idx, uint32_t arg) {
    MCI_CLEAR = 0xFFF;
    MCI_ARG   = arg;
    MCI_CMD   = (idx & 0x3F) | CMD_WAITRESP | CMD_LONGRSP | CMD_ENABLE;
    while (!(MCI_STATUS & (ST_CMDRESPEND | ST_CMDCRCFAIL | ST_CMDTIMEOUT)));
}

/* ----- SD card initialisation --------------------------------------------- */

static uint32_t sd_rca;    /* Relative Card Address */
static int      sd_sdhc;   /* 1 = SDHC/SDXC (sector-addressed), 0 = SDSC (byte-addressed) */

static void sd_init(void) {
    /* 1. Power ramp-up sequence required by PL181 */
    MCI_POWER = 0x02;                          /* power-up phase */
    for (volatile int i = 0; i < 10000; i++);
    MCI_POWER = 0x03;                          /* power-on */
    MCI_CLOCK = (1 << 8) | 118;               /* enable, ~400 kHz init clock */
    for (volatile int i = 0; i < 10000; i++);

    // https://olof-astrand.medium.com/sd-card-emulation-and-initialization-in-qemu-when-used-by-u-boot-and-qnx-cd8c1267d8f4
    /*
    this link above will help a lot unserstanding this better;
    */
    /* 2. CMD0 — GO_IDLE_STATE (no response expected) */
    sd_cmd_norsp(0, 0);
    uart_puts("  - CMD0: Card Reset... OK\n");

    /* 3. CMD8 — SEND_IF_COND (SD v2 probe; ignore timeout on v1 cards) */
    sd_cmd_short(8, 0x000001AA);
    /*
    purpose : is to check if the card support the [supplied] voltage range; and is SD version 2.0+
    Arg 0x1AA voltage supplied [2.7-3.6V] indicated by 0x1 in bits [11;8]

    https://www.labcenter.com/blog/sim-sd-cards/

    Sd cards support 2 communication protocols
    - Protocol A; which is the base SD card protocol
    - Protocol B; which is the SPI protocol

    Cards start in SD bus mode by default, but can be changed to SPI mode by sending CMD0 with MISO disconnected

    Reading from SD Cards and Writing to SD Cards is done in blocks 
    usually 512 bytes in size;

    FAT16 fs is much simpler and was popular until, sd card of 2gb or smaller started becoming scarce, as 2gb is the
    max size fat16 can handle efficiently.

    FAT32 improvements:
    - Can handle larger volumes (up to 2? 16? TB)
    - Uses 32-bit cluster addresses
    - Supports larger file sizes (up to 4 GB)

    sd cards use flash memory internally; and as such data is erased in blocks
    usually size of kB or larger

    read https://miro.medium.com/v2/resize:fit:720/format:webp/1*s5HC3tOc21X75bQjoUPwmQ.png
    read this and understand 
    */
    /* 4. ACMD41 loop — wait for card to leave busy state (OCR bit31 = power-up done)
     *    ACMD = CMD55 (with RCA=0) followed by the application command */
    uint32_t ocr = 0;
    for (int tries = 0; tries < 2000 && !(ocr & 0x80000000); tries++) {
        sd_cmd_short(55, 0);                   /* CMD55 APP_CMD, RCA=0 */
        ocr = sd_cmd_short(41, 0x40FF8000);    /* ACMD41 HCS=1, full voltage */
        for (volatile int d = 0; d < 500; d++);
    }
    uart_puts("  - ACMD41 (card initialization): Ready. OCR=");
    uart_put_hex(ocr);
    
    /* CCS bit[30]: 1 = SDHC/SDXC (sector address), 0 = SDSC (byte address) */
    sd_sdhc = (ocr >> 30) & 1;
    uart_puts(sd_sdhc ? " [SDHC]\n" : " [SDSC]\n");

    /* 5. CMD2 — ALL_SEND_CID (long response: 128-bit card ID) */
    sd_cmd_long(2, 0);
    uart_puts("  - CMD2: CID received\n");

    /* 6. CMD3 — SEND_RELATIVE_ADDR (card picks its RCA) */
    sd_rca = sd_cmd_short(3, 0) & 0xFFFF0000;
    uart_puts("  - CMD3: RCA set to ");
    uart_put_hex(sd_rca >> 16);
    uart_puts("\n");

    /* 7. CMD7 — SELECT_CARD → Transfer state */
    sd_cmd_short(7, sd_rca);
    uart_puts("  - CMD7: Card Selected\n");

    /* 8. CMD16 — SET_BLOCKLEN = 512 bytes */
    sd_cmd_short(16, SECTOR_SIZE);

    /* Full-speed clock once card is selected */
    MCI_CLOCK = (1 << 8);                      /* enable, divider=0 (max) */
}

/* ----- read one 512-byte sector from SD card ------------------------------ */

/*
 * PL181 DCTRL register layout:
 *   [0]   DTEN      — data transfer enable
 *   [1]   DTDIR     — 0=controller→card(write), 1=card→controller(READ)
 *   [2]   DTMODE    — 0=block, 1=stream
 *   [3]   DMAEN     — DMA enable (leave 0)
 *   [7:4] DBLOCKSIZE — 2^n bytes per block (9 = 512 bytes)
 *
 * MCI_STATUS data-transfer bits:
 *   [1]  DATACRCFAIL
 *   [3]  DATATIMEOUT
 *   [8]  DATAEND
 *   [10] DATABLOCKEND
 *   [19] RXFIFOEMPTY
 *   [21] RXDATAAVLBL
 */
#define ST_DATACRCFAIL   (1 << 1)
#define ST_DATATIMEOUT   (1 << 3)
#define ST_DATAEND       (1 << 8)
#define ST_DATABLOCKEND  (1 << 10)
#define ST_RXFIFOEMPTY   (1 << 19)
#define ST_RXDATAAVAIL   (1 << 21)

/* DCTRL: 512-byte block, card→host (read), no DMA, enable */
#define DCTRL_READ_512   ((9 << 4) | (1 << 1) | (1 << 0))

static void sd_read_sector(uint32_t lba, uint8_t *buf) {
    MCI_CLEAR  = 0x1DC07FF;
    MCI_DTIMER = 0xFFFFFFFF;
    MCI_DLEN   = SECTOR_SIZE;

    /* Arm the data path BEFORE the command (per PL181 spec) */
    MCI_DCTRL  = DCTRL_READ_512;

    /* CMD17 — READ_SINGLE_BLOCK
     * SDSC: byte address = LBA × 512
     * SDHC/SDXC: sector address = LBA directly */
    uint32_t addr = sd_sdhc ? lba : (lba * SECTOR_SIZE);
    sd_cmd_short(17, addr);

    /* Drain RX FIFO one 32-bit word at a time */
    int words_read = 0;
    for (int i = 0; i < SECTOR_SIZE; ) {
        uint32_t st = MCI_STATUS;
        if (st & ST_RXDATAAVAIL) {
            uint32_t w = MCI_FIFO;
            buf[i++] = (uint8_t)(w);
            buf[i++] = (uint8_t)(w >> 8);
            buf[i++] = (uint8_t)(w >> 16);
            buf[i++] = (uint8_t)(w >> 24);
            words_read++;
        } else if (st & (ST_DATATIMEOUT | ST_DATACRCFAIL)) {
            break; /* read failed; buf will contain zeros from BSS */
        }
    }
    /* Wait for data state machine to finish */
    while (!(MCI_STATUS & (ST_DATAEND | ST_DATATIMEOUT | ST_DATACRCFAIL)));
    MCI_CLEAR = 0x1DC07FF;
}

static void sd_write_sector(uint32_t lba, const uint8_t *buf) {
#ifdef FAULT_INJECTION
    uart_puts("FI_HOOK_WRITE\n");
    // Wait for python harness to respond
    uint32_t rx;
    do { rx = UART0_FR; } while (rx & (1 << 4)); // wait for RXFE to clear
    char c = UART0_DR & 0xFF;
    if (c == 'K') {
        uart_puts("FI_POWER_LOSS\n");
        while(1); // Halt forever to simulate power loss
    }
#endif
    MCI_CLEAR  = 0x1DC07FF;
    MCI_DTIMER = 0xFFFFFFFF;
    MCI_DLEN   = SECTOR_SIZE;
    MCI_DCTRL  = ((9 << 4) | (0 << 1) | (1 << 0)); // DCTRL_WRITE_512

    uint32_t addr = sd_sdhc ? lba : (lba * SECTOR_SIZE);
    sd_cmd_short(24, addr); // CMD24 WRITE_BLOCK

    for (int i = 0; i < SECTOR_SIZE; ) {
        uint32_t st = MCI_STATUS;
        if (!(st & (1 << 16))) { // ST_TXFIFOFULL not set
            uint32_t w = buf[i++];
            w |= (uint32_t)buf[i++] << 8;
            w |= (uint32_t)buf[i++] << 16;
            w |= (uint32_t)buf[i++] << 24;
            MCI_FIFO = w;
        } else if (st & (ST_DATATIMEOUT | ST_DATACRCFAIL)) {
            break;
        }
    }
    while (!(MCI_STATUS & (ST_DATAEND | ST_DATATIMEOUT | ST_DATACRCFAIL)));
    MCI_CLEAR = 0x1DC07FF;
}

#include "metadata.h"
#include "../firmware/image_format/firmware_format.h"
#include "../crypto/sha256/sha256.h"
#include "../crypto/crc32/crc32.h"
#include "monocypher.h"
#include "../stax_key.pub.h" // STAX_PUBLIC_KEY

void *memset(void *s, int c, size_t n) {
    unsigned char *p = s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

static int boot_slot(uint32_t start_lba, uint32_t min_version) {
    static uint8_t sector_buf[512];
    sd_read_sector(start_lba, sector_buf);
    
    firmware_header_t header_copy;
    for(uint32_t i=0; i<sizeof(firmware_header_t); i++) {
        ((uint8_t*)&header_copy)[i] = sector_buf[i];
    }
    firmware_header_t *hdr = &header_copy;

    if (hdr->magic != FIRMWARE_MAGIC) {
        uart_puts("Error: Invalid Firmware Magic\n");
        return -1;
    }

    if (hdr->format_ver != FIRMWARE_FORMAT_VERSION) {
        uart_puts("Error: Unsupported format version\n");
        return -1;
    }
    
    uint32_t calc_crc = crc32(sector_buf, 28);
    if (calc_crc != hdr->crc32) {
        uart_puts("Error: Header CRC mismatch\n");
        return -1;
    }

    if (hdr->image_ver < min_version) {
        uart_puts("Error: Rollback protection triggered (version < min_version)\n");
        return -1;
    }

    uart_puts("Firmware header valid. Loading payload... ");
    uint8_t *dst = (uint8_t *)hdr->load_addr;
    uint32_t size = hdr->image_size;
    uint32_t total_size_on_disk = sizeof(firmware_header_t) + size;
    uint32_t sectors = (total_size_on_disk + 511) / 512;
    
    // Copy the payload part from the first sector
    uint32_t payload_in_first = 512 - sizeof(firmware_header_t);
    uint32_t to_copy = size < payload_in_first ? size : payload_in_first;
    for(uint32_t i=0; i<to_copy; i++) {
        dst[i] = sector_buf[sizeof(firmware_header_t) + i];
    }
    
    // Load remaining sectors
    for (uint32_t i = 1; i < sectors; i++) {
        sd_read_sector(start_lba + i, sector_buf);
        uint32_t remaining = size - (payload_in_first + (i-1)*512);
        uint32_t to_copy_now = remaining < 512 ? remaining : 512;
        for(uint32_t j=0; j<to_copy_now; j++) {
            dst[payload_in_first + (i-1)*512 + j] = sector_buf[j];
        }
        if (i % 128 == 0) uart_puts(".");
    }
    uart_puts(" OK\n");
    
    uart_puts("Verifying payload hash... ");
    uint8_t hash[32];
    sha256(dst, size, hash);
    
    uart_puts("\nDEBUG bootloader: Payload[0..3] = ");
    uart_put_hex((dst[0] << 24) | (dst[1] << 16) | (dst[2] << 8) | dst[3]);
    uart_puts("\nDEBUG bootloader: Hash[0..3] = ");
    uart_put_hex((hash[0] << 24) | (hash[1] << 16) | (hash[2] << 8) | hash[3]);
    uart_puts("\nDEBUG bootloader: Expected Hash[0..3] = ");
    uart_put_hex((hdr->payload_hash[0] << 24) | (hdr->payload_hash[1] << 16) | (hdr->payload_hash[2] << 8) | hdr->payload_hash[3]);
    uart_puts("\n");

    for(int i=0; i<32; i++) {
        if (hash[i] != hdr->payload_hash[i]) {
            uart_puts("FAILED\n");
            return -1;
        }
    }
    uart_puts("OK\n");

    uart_puts("Verifying Ed25519 signature... ");
    if (crypto_eddsa_check(hdr->signature, STAX_PUBLIC_KEY, hash, 32) != 0) {
        uart_puts("FAILED (Invalid Signature)\n");
        return -1;
    }
    uart_puts("OK\n");

    uart_puts("Jumping to kernel...\n\n");
    void (*kernel_entry)(void) = (void (*)(void))hdr->entry_point;
    kernel_entry();
    return 0; // Should never reach here
}

void bootloader_main(void) {
    uart_init();
    uart_puts("--------------------------------------------------\n");
    uart_puts("  STAX Secure Bootloader v1.0\n");
    uart_puts("--------------------------------------------------\n");
    
    uart_puts("Initializing PL181 SD Card Controller...\n");
    sd_init();
    
    uart_puts("Reading Metadata...\n");
    static uint8_t meta_a_buf[512];
    static uint8_t meta_b_buf[512];
    sd_read_sector(1, meta_a_buf); // Metadata A at LBA 1
    sd_read_sector(2, meta_b_buf); // Metadata B at LBA 2
    
    boot_metadata_t *meta_a = (boot_metadata_t *)meta_a_buf;
    boot_metadata_t *meta_b = (boot_metadata_t *)meta_b_buf;
    
    int a_valid = 0, b_valid = 0;
    if (meta_a->magic == METADATA_MAGIC && crc32(meta_a_buf, sizeof(boot_metadata_t) - 4) == meta_a->crc32) a_valid = 1;
    if (meta_b->magic == METADATA_MAGIC && crc32(meta_b_buf, sizeof(boot_metadata_t) - 4) == meta_b->crc32) b_valid = 1;

    boot_metadata_t active_meta;
    int need_init = 0;

    if (a_valid && b_valid) {
        if (meta_a->generation >= meta_b->generation) active_meta = *meta_a;
        else active_meta = *meta_b;
    } else if (a_valid) {
        active_meta = *meta_a;
    } else if (b_valid) {
        active_meta = *meta_b;
    } else {
        uart_puts("Warning: Metadata invalid or missing. Initializing defaults.\n");
        memset(&active_meta, 0, sizeof(boot_metadata_t));
        active_meta.magic = METADATA_MAGIC;
        active_meta.generation = 1;
        active_meta.active_slot = 0;
        active_meta.slot_a_state = SLOT_STATE_CONFIRMED;
        active_meta.slot_b_state = SLOT_STATE_EMPTY;
        need_init = 1;
    }

    int state_changed = need_init;
    
    // Evaluate active slot
    uint32_t slot = active_meta.active_slot;
    uint32_t original_slot = slot;
    uint32_t state = (slot == 0) ? active_meta.slot_a_state : active_meta.slot_b_state;
    uint32_t attempts = (slot == 0) ? active_meta.slot_a_boot_attempts : active_meta.slot_b_boot_attempts;
    uint32_t slot_lba = (slot == 0) ? 3 : 2051;

    if (state == SLOT_STATE_PENDING) {
        state = SLOT_STATE_BOOTING;
        attempts = 1;
        state_changed = 1;
    } else if (state == SLOT_STATE_BOOTING) {
        attempts++;
        if (attempts > 3) {
            uart_puts("Watchdog triggered: Boot attempts exceeded limit.\n");
            state = SLOT_STATE_FAILED;
            // Rollback to other slot
            active_meta.active_slot = (slot == 0) ? 1 : 0;
            slot = active_meta.active_slot;
            slot_lba = (slot == 0) ? 3 : 2051;
            uart_puts("Rolling back to fallback slot.\n");
            state_changed = 1;
        } else {
            state_changed = 1;
        }
    }
    
    if (state_changed) {
        if (original_slot == 0) {
            active_meta.slot_a_state = state;
            active_meta.slot_a_boot_attempts = attempts;
        } else {
            active_meta.slot_b_state = state;
            active_meta.slot_b_boot_attempts = attempts;
        }
    }

    if (state_changed) {
        active_meta.generation++;
        active_meta.crc32 = crc32((const uint8_t *)&active_meta, sizeof(boot_metadata_t) - 4);
        static uint8_t write_buf[512];
        memset(write_buf, 0, 512);
        for(uint32_t i=0; i<sizeof(boot_metadata_t); i++) write_buf[i] = ((uint8_t*)&active_meta)[i];
        sd_write_sector(1, write_buf);
        sd_write_sector(2, write_buf);
    }
    uint32_t min_version = 0;
    if (state == SLOT_STATE_PENDING || state == SLOT_STATE_BOOTING) {
        if (slot == 0) {
            if (active_meta.slot_b_state == SLOT_STATE_CONFIRMED) min_version = active_meta.slot_b_version;
        } else {
            if (active_meta.slot_a_state == SLOT_STATE_CONFIRMED) min_version = active_meta.slot_a_version;
        }
    }

    if (boot_slot(slot_lba, min_version) != 0) {
        uart_puts("Failed to boot active slot. Trying fallback slot...\n");
        // Mark failed
        if (slot == 0) active_meta.slot_a_state = SLOT_STATE_FAILED;
        else active_meta.slot_b_state = SLOT_STATE_FAILED;
        
        active_meta.active_slot = (slot == 0) ? 1 : 0;
        active_meta.generation++;
        active_meta.crc32 = crc32((const uint8_t *)&active_meta, sizeof(boot_metadata_t) - 4);
        static uint8_t write_buf2[512];
        memset(write_buf2, 0, 512);
        for(uint32_t i=0; i<sizeof(boot_metadata_t); i++) write_buf2[i] = ((uint8_t*)&active_meta)[i];
        sd_write_sector(1, write_buf2);
        sd_write_sector(2, write_buf2);

        slot_lba = (active_meta.active_slot == 0) ? 3 : 2051;
        if (boot_slot(slot_lba, 0) != 0) {
            uart_puts("Critical Error: Both slots failed to boot. System Halted.\n");
            while(1);
        }
    }
}
