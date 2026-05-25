#include "smc91c111.h"
#include "console.h"

static inline void smc_set_bank(uint16_t bank) {
    SMC_BSR = bank;
}

static inline void smc_mmu_wait(void) {
    smc_set_bank(2);
    /* Poll bit 0 (BUSY) of SMC_MMU_CMD until it becomes 0 (idle) */
    while (SMC_MMU_CMD & 0x01) {
        __asm__ volatile ("nop");
    }
}

void smc_free_tx_packets(void) {
    smc_set_bank(2);
    while (1) {
        /* Read 16-bit FIFO port to correctly pop TX FIFO in QEMU */
        uint16_t fifos = SMC_REG16(0x04);
        uint8_t tx_pkt = fifos & 0xFF;
        if (tx_pkt & 0x80)
            break;
        
        SMC_PKT_NUM = tx_pkt;
        SMC_MMU_CMD = MMU_CMD_FREEPKT; /* Release specific packet (0xA0) */
        smc_mmu_wait();
    }
}

void smc91c111_init(void) {
    smc_set_bank(2);
    SMC_MMU_CMD = MMU_CMD_RESET;
    smc_mmu_wait();

    smc_set_bank(0);
    SMC_TCR = TCR_ENABLE | TCR_PAD_EN;
    SMC_RCR = RCR_RXEN | RCR_STRIP_CRC | RCR_PRMS;

    smc_set_bank(1);
    SMC_CR = 0x1000 | 0x0100 | 0x0080;
}

int smc91c111_tx(const uint8_t *data, size_t len) {
    if (len > 1536 || len == 0)
        return -1;

    /* Reclaim memory of previously completed packets first */
    // smc_free_tx_packets();

    /* Print ethernet frame details */
    if (len >= 14) {
        uint16_t eth_type = (data[12] << 8) | data[13];
        kprintf("[TX] Len: %d | DST: %02x:%02x:%02x:%02x:%02x:%02x | SRC: %02x:%02x:%02x:%02x:%02x:%02x | Type: 0x%04x\n",
                (int)len,
                data[0], data[1], data[2], data[3], data[4], data[5],
                data[6], data[7], data[8], data[9], data[10], data[11],
                eth_type);
    } else {
        kprintf("[TX] Tiny frame len: %d\n", (int)len);
    }

    int packet_length = (int)len + 6;
    int pages = (packet_length + 255) / 256;

    smc_set_bank(2);
    SMC_MMU_CMD = (uint8_t)(MMU_CMD_ALLOC_TX | pages);
    smc_mmu_wait();

    uint8_t pkt_num = SMC_ARR;
    if (pkt_num & SMC_ARR_FAILED) {
        kputs("[TX] MMU Allocation failed!\n");
        return -1;
    }

    SMC_PKT_NUM = pkt_num;
    SMC_PTR = PTR_AUTO_INC;

    SMC_DATA = 0;
    SMC_DATA = (uint16_t)(packet_length & 0x7FF);

    const uint16_t *data16 = (const uint16_t *)data;
    for (size_t i = 0; i < len / 2; i++)
        SMC_DATA = data16[i];

    if (len & 1)
        SMC_DATA = (uint16_t)(data[len - 1] | (0x20 << 8));
    else
        SMC_DATA = 0;

    SMC_PKT_NUM = pkt_num;
    SMC_MMU_CMD = MMU_CMD_ENQUEUE;
    smc_mmu_wait();

    return 0;
}

size_t smc91c111_rx(uint8_t *buf, size_t max_len) {
    smc_set_bank(2);

    /* Read 8-bit RX FIFO port (does not pop TX FIFO) */
    uint8_t pkt_num = SMC_RXFIFO;
    if (pkt_num & SMC_RXFIFO_EMPTY)
        return 0;

    SMC_PKT_NUM = pkt_num;
    SMC_PTR = PTR_RCV | PTR_READ | PTR_AUTO_INC;

    uint16_t status = SMC_DATA;
    uint16_t len = SMC_DATA & 0x7FF;

    if (len < 6) {
        SMC_MMU_CMD = MMU_CMD_RELEASE; /* Pop and Release (0x80) */
        smc_mmu_wait();
        return 0;
    }

    size_t payload_len = (size_t)len - 6;
    if (payload_len > max_len)
        payload_len = max_len;

    uint16_t *buf16 = (uint16_t *)buf;
    for (size_t i = 0; i < payload_len / 2; i++)
        buf16[i] = SMC_DATA;

    if (payload_len & 1)
        buf[payload_len - 1] = (uint8_t)SMC_DATA;

    SMC_MMU_CMD = MMU_CMD_RELEASE; /* Pop and Release (0x80) */
    smc_mmu_wait();

    /* Print ethernet frame details */
    if (payload_len >= 14) {
        uint16_t eth_type = (buf[12] << 8) | buf[13];
        kprintf("[RX] Len: %d | DST: %02x:%02x:%02x:%02x:%02x:%02x | SRC: %02x:%02x:%02x:%02x:%02x:%02x | Type: 0x%04x\n",
                (int)payload_len,
                buf[0], buf[1], buf[2], buf[3], buf[4], buf[5],
                buf[6], buf[7], buf[8], buf[9], buf[10], buf[11],
                eth_type);
    } else {
        kprintf("[RX] Tiny frame len: %d\n", (int)payload_len);
    }

    (void)status;
    return payload_len;
}
