/* ============================================================================
 * TIOS — disk_pl181.c
 * PL181 MCI (SD/MMC) sector reader for QEMU VersatilePB
 *
 * VersatilePB PL181 base: 0x10005000
 * Reference: ARM PL181 TRM, QEMU hw/sd/pl181.c
 *
 * Provides: int disk_read(uint32_t lba, uint8_t *buf)
 * ============================================================================ */

#include "fat.h"
#include <stdint.h>

/* ---- PL181 register offsets ---- */
#define MCI_BASE        0x10005000u
#define MCI_POWER       (*(volatile uint32_t*)(MCI_BASE + 0x000))
#define MCI_CLOCK       (*(volatile uint32_t*)(MCI_BASE + 0x004))
#define MCI_ARGUMENT    (*(volatile uint32_t*)(MCI_BASE + 0x008))
#define MCI_COMMAND     (*(volatile uint32_t*)(MCI_BASE + 0x00C))
#define MCI_RESPCMD     (*(volatile uint32_t*)(MCI_BASE + 0x010))
#define MCI_RESPONSE0   (*(volatile uint32_t*)(MCI_BASE + 0x014))
#define MCI_RESPONSE1   (*(volatile uint32_t*)(MCI_BASE + 0x018))
#define MCI_RESPONSE2   (*(volatile uint32_t*)(MCI_BASE + 0x01C))
#define MCI_RESPONSE3   (*(volatile uint32_t*)(MCI_BASE + 0x020))
#define MCI_DATATIMER   (*(volatile uint32_t*)(MCI_BASE + 0x024))
#define MCI_DATALENGTH  (*(volatile uint32_t*)(MCI_BASE + 0x028))
#define MCI_DATACTRL    (*(volatile uint32_t*)(MCI_BASE + 0x02C))
#define MCI_DATACNT     (*(volatile uint32_t*)(MCI_BASE + 0x030))
#define MCI_STATUS      (*(volatile uint32_t*)(MCI_BASE + 0x034))
#define MCI_CLEAR       (*(volatile uint32_t*)(MCI_BASE + 0x038))
#define MCI_MASK0       (*(volatile uint32_t*)(MCI_BASE + 0x03C))
#define MCI_MASK1       (*(volatile uint32_t*)(MCI_BASE + 0x040))
#define MCI_FIFOCNT     (*(volatile uint32_t*)(MCI_BASE + 0x048))
#define MCI_FIFO        (*(volatile uint32_t*)(MCI_BASE + 0x080))

/* Status bits */
#define MCI_ST_CMDSENT  (1u << 7)
#define MCI_ST_CMDRESP  (1u << 6)
#define MCI_ST_RXDAVL   (1u << 21)
#define MCI_ST_RXFIFOE  (1u << 19)
#define MCI_ST_DATEND   (1u << 8)
#define MCI_ST_DATAERR  ((1u << 1) | (1u << 3))  /* DATACRCFAIL (1) | DATATIMEOUT (3) */
#define MCI_ST_RXOVERR  (1u << 5)

/* Command bits */
#define MCI_CMD_ENABLE  (1u << 10)
#define MCI_CMD_RESP    (1u << 6)
#define MCI_CMD_LONGRESP (1u << 7)
#define MCI_CMD_WAIT    (1u << 8)

/* Data control */
#define MCI_DCTRL_ENABLE   (1u << 0)
#define MCI_DCTRL_TOCARD   (0u << 1)
#define MCI_DCTRL_FROMCARD (1u << 1)
#define MCI_DCTRL_BLK512   (9u << 4)   /* blocksize = 2^9 = 512 */

static int mci_initialized = 0;
static int mci_sdhc = 0;   /* 1 = SDHC/SDXC card (block addressing) */

/* ---- Send a command and wait ---- */
static int mci_send_cmd(uint32_t cmd, uint32_t arg, uint32_t *resp)
{
    MCI_CLEAR    = 0x7FF;
    MCI_ARGUMENT = arg;
    /* Disable data state machine when sending generic commands */
    if (cmd != 17 && cmd != 18 && cmd != 24 && cmd != 25) MCI_DATACTRL = 0;

    MCI_COMMAND  = (cmd & 0x3Fu) | MCI_CMD_ENABLE |
                   (resp ? MCI_CMD_RESP : 0u);

    /* Wait for response or send */
    uint32_t mask = resp ? MCI_ST_CMDRESP : MCI_ST_CMDSENT;
    int timeout = 100000;
    while (timeout-- > 0) {
        uint32_t st = MCI_STATUS;
        if (st & mask) break;
        if (st & (1u << 1)) return -1;  /* CmdCRCFail */
    }
    if (timeout <= 0) return -2;        /* timeout */

    if (resp) *resp = MCI_RESPONSE0;
    MCI_CLEAR = 0x7FF;
    return 0;
}

/* ---- Initialize PL181 + SD card ---- */
static int mci_init(void)
{
    if (mci_initialized) return 0;

    /* Power on, clock at ~400 kHz init speed */
    MCI_POWER = 0x83;                   /* PowerOn */
    MCI_CLOCK = 0x61;                   /* ~400 kHz */

    /* Spin a bit for card power-up */
    for (volatile int i = 0; i < 100000; i++) __asm__ volatile("nop");

    uint32_t resp;

    /* CMD0: GO_IDLE_STATE */
    mci_send_cmd(0, 0, 0);

    /* CMD8: SEND_IF_COND (SDHC check) */
    mci_sdhc = 0;
    if (mci_send_cmd(8, 0x000001AAu, &resp) == 0 && (resp & 0x1FF) == 0x1AA)
        mci_sdhc = 1;

    /* ACMD41: send operating condition */
    int retry = 1000;
    uint32_t ocr;
    do {
        mci_send_cmd(55, 0, &resp);  /* APP_CMD */
        uint32_t arg = 0x40FF8000u;   /* HCS bit for SDHC */
        if (mci_send_cmd(41, arg, &ocr) != 0) { ocr = 0; break; }
        if (--retry == 0) break;
    } while (!(ocr & (1u << 31)));    /* busy bit */

    if (ocr & (1u << 30)) mci_sdhc = 1;  /* CCS bit */

    /* CMD2: ALL_SEND_CID */
    mci_send_cmd(2, 0, &resp);

    /* CMD3: SEND_RELATIVE_ADDR */
    uint32_t rca = 0;
    mci_send_cmd(3, 0, &resp);
    rca = resp & 0xFFFF0000u;

    /* CMD7: SELECT_CARD */
    mci_send_cmd(7, rca, &resp);

    /* CMD16: SET_BLOCKLEN to 512 (only for SDSC) */
    if (!mci_sdhc) mci_send_cmd(16, 512, &resp);

    /* Raise clock to ~20 MHz */
    MCI_CLOCK = 0x02;

    mci_initialized = 1;
    return 0;
}

int disk_read(uint32_t lba, uint8_t *buf)
{
    MCI_CLEAR      = 0x1DC07FF;
    MCI_DATATIMER  = 0xFFFFFFFFu;
    MCI_DATALENGTH = 512;

    /* Arm the data path BEFORE the command */
    MCI_DATACTRL   = (9 << 4) | (1 << 1) | (1 << 0);

    uint32_t arg = mci_sdhc ? lba : (lba * 512);
    
    /* Send CMD17 */
    MCI_CLEAR = 0xFFF;
    MCI_ARGUMENT = arg;
    MCI_COMMAND = (17 & 0x3F) | (1 << 6) | (1 << 10); /* CMD_WAITRESP | CMD_ENABLE */
    
    while (!(MCI_STATUS & ((1 << 6) | (1 << 0) | (1 << 2)))); /* ST_CMDRESPEND | ST_CMDCRCFAIL | ST_CMDTIMEOUT */

    /* Drain RX FIFO one 32-bit word at a time */
    int words_read = 0;
    for (int i = 0; i < 512; ) {
        uint32_t st = MCI_STATUS;
        if (st & (1 << 21)) { /* ST_RXDATAAVAIL */
            uint32_t w = MCI_FIFO;
            buf[i++] = (uint8_t)(w);
            buf[i++] = (uint8_t)(w >> 8);
            buf[i++] = (uint8_t)(w >> 16);
            buf[i++] = (uint8_t)(w >> 24);
            words_read++;
        } else if (st & ((1 << 3) | (1 << 1))) { /* ST_DATATIMEOUT | ST_DATACRCFAIL */
            break;
        }
    }

    /* Wait for data state machine to finish */
    while (!(MCI_STATUS & ((1 << 8) | (1 << 3) | (1 << 1)))); /* ST_DATAEND | ST_DATATIMEOUT | ST_DATACRCFAIL */
    
    MCI_CLEAR = 0x1DC07FF;
    return 0;
}
