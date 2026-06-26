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

void bootloader_main(void) {
    uart_init();
    uart_puts("--------------------------------------------------\n");
    uart_puts("  STAX Bootloader v0.3 [Bare-Metal PL181 SD]\n");
    uart_puts("--------------------------------------------------\n");
    
    uart_puts("Initializing PL181 SD Card Controller...\n");
    sd_init();
    
    uart_puts("Parsing FAT16 filesystem to find KERNEL.BIN...\n");
    
    static uint8_t sector_buf[512];
    sd_read_sector(0, sector_buf);
    
    uint16_t bytes_per_sec = *(uint16_t *)(sector_buf + 11);
    uint8_t  sec_per_clus  = *(uint8_t  *)(sector_buf + 13);
    uint16_t rsvd_sec_cnt  = *(uint16_t *)(sector_buf + 14);
    uint8_t  num_fats      = *(uint8_t  *)(sector_buf + 16);
    uint16_t root_ent_cnt  = *(uint16_t *)(sector_buf + 17);
    uint16_t fat_size_16   = *(uint16_t *)(sector_buf + 22);

    if (bytes_per_sec != 512) {
        uart_puts("Error: Sector size is not 512 bytes.\n");
        while(1);
    }

    uint32_t fat_start      = rsvd_sec_cnt;
    uint32_t root_dir_start = fat_start + (num_fats * fat_size_16);
    uint32_t root_dir_sects = (root_ent_cnt * 32 + 511) / 512;
    uint32_t data_start     = root_dir_start + root_dir_sects;

    uint16_t kernel_cluster = 0;
    
    for (uint32_t s = 0; s < root_dir_sects; s++) {
        sd_read_sector(root_dir_start + s, sector_buf);
        for (int i = 0; i < 512; i += 32) {
            uint8_t *entry = sector_buf + i;
            if (entry[0] == 0x00) break;
            if (entry[0] == 0xE5) continue;
            
            int match = 1;
            const char *target = "KERNEL  BIN";
            for (int j = 0; j < 11; j++) {
                if (entry[j] != target[j]) { match = 0; break; }
            }
            if (match) {
                kernel_cluster = *(uint16_t *)(entry + 26);
                break;
            }
        }
        if (kernel_cluster != 0) break;
    }

    if (kernel_cluster == 0) {
        uart_puts("Error: KERNEL.BIN not found in root directory.\n");
        while(1);
    }
    
    uart_puts("Found KERNEL.BIN, loading clusters");
    
    uint8_t *dst = (uint8_t *)KERNEL_EXEC_ADDR;
    uint32_t current_cluster = kernel_cluster;
    
    while (current_cluster >= 0x0002 && current_cluster <= 0xFFEF) {
        uint32_t lba = data_start + (current_cluster - 2) * sec_per_clus;
        for (int i = 0; i < sec_per_clus; i++) {
            sd_read_sector(lba + i, dst);
            dst += 512;
        }
        uart_puts(".");
        
        uint32_t fat_offset = current_cluster * 2;
        uint32_t fat_sec = fat_start + (fat_offset / 512);
        uint32_t ent_offset = fat_offset % 512;
        
        sd_read_sector(fat_sec, sector_buf);
        current_cluster = *(uint16_t *)(sector_buf + ent_offset);
    }
    uart_puts("\nKernel loaded.\n");

    /* -------------------------------------------------------------------------
     * Kernel Verification Phase
     * ------------------------------------------------------------------------- */
    uart_puts("Verifying kernel integrity... ");
    
    /* The magic number "STAX" is at offset 4 of the kernel binary */
    const char *kernel_magic = (const char *)(KERNEL_EXEC_ADDR + 4);
    
    if (kernel_magic[0] == 'T' && kernel_magic[1] == 'I' && 
        kernel_magic[2] == 'O' && kernel_magic[3] == 'S') {
        uart_puts("OK\n");
    } else {
        uart_puts("FAILED!\n");
        uart_puts("Error: Kernel magic number mismatch. Expected 'STAX'.\n");
        uart_puts("Possible reasons: SD card not mapped, incorrect KERNEL_LBA, or build failure.\n");
        while (1); /* Halt here; do NOT jump to an invalid kernel */
    }

    uart_puts("Jumping to kernel...\n\n");
    void (*kernel_entry)(void) = (void (*)(void))KERNEL_EXEC_ADDR;
    kernel_entry();
}
