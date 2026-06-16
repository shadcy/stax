#include "smc91c111.h"
#include "console.h"

#define SMC_PACKET_DEBUG 1

static inline void smc_set_bank(uint16_t bank) {
    SMC_BSR = bank;
}

static inline void smc_mmu_wait(void) {
    smc_set_bank(2);
    while (SMC_MMU_CMD & 0x01) {
        __asm__ volatile ("nop");
    }
}

void smc_free_tx_packets(void) {
    smc_set_bank(2);
    /* Drain the TX-done FIFO. MMU_CMD_REMOVE (0x60) pops the top entry
     * from the TX completion FIFO AND frees the associated memory page.
     * QEMU executes it synchronously so no smc_mmu_wait() is needed. */
    for (int i = 0; i < 8; i++) {
        uint8_t tx_pkt = SMC_REG8(0x04);  /* peek TX-done FIFO */
        if (tx_pkt & 0x80)
            break;                         /* FIFO empty */
        (void)tx_pkt;
        SMC_MMU_CMD = MMU_CMD_REMOVE;      /* pop + free top of TX done FIFO */
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

    /* Aggressively reclaim all completed TX packets before attempting alloc */
    smc_free_tx_packets();

#if SMC_PACKET_DEBUG
    /* Print ethernet frame details */
    if (len >= 14) {
        uint16_t eth_type = (data[12] << 8) | data[13];
        if (eth_type == 0x0800 && len >= 14 + 20) {
            uint8_t *ip = (uint8_t *)data + 14;
            uint8_t proto = ip[9];
            uint8_t ihl = (ip[0] & 0x0f) * 4;
            uint8_t *src = ip + 12;
            uint8_t *dst = ip + 16;
            if (proto == 17 && len >= 14 + ihl + 8) {
                uint8_t *udp = ip + ihl;
                uint16_t sport = (udp[0] << 8) | udp[1];
                uint16_t dport = (udp[2] << 8) | udp[3];
                kprintf("[TX] IP UDP %d.%d.%d.%d:%u -> %d.%d.%d.%d:%u (eth dst %02x:%02x:%02x:%02x:%02x:%02x)\n",
                        src[0], src[1], src[2], src[3], sport,
                        dst[0], dst[1], dst[2], dst[3], dport,
                        data[0], data[1], data[2], data[3], data[4], data[5]);
            } else {
                kprintf("[TX] IP proto:%u %d.%d.%d.%d -> %d.%d.%d.%d (eth dst %02x:%02x:%02x:%02x:%02x:%02x)\n",
                        proto,
                        src[0], src[1], src[2], src[3],
                        dst[0], dst[1], dst[2], dst[3],
                        data[0], data[1], data[2], data[3], data[4], data[5]);
            }
        } else {
            kprintf("[TX] Len:%d DST:%02x:%02x:%02x:%02x:%02x:%02x SRC:%02x:%02x:%02x:%02x:%02x:%02x Type:0x%04x\n",
                    (int)len,
                    data[0], data[1], data[2], data[3], data[4], data[5],
                    data[6], data[7], data[8], data[9], data[10], data[11],
                    eth_type);
        }
    } else {
        kprintf("[TX] Tiny frame len: %d\n", (int)len);
    }
#endif

    int packet_length = (int)len + 6;
    int pages = (packet_length + 255) / 256;

    /* Attempt a single allocation. smc_free_tx_packets() (called above)   *
     * already drained all completed TX buffers using MMU_CMD_REMOVE, so   *
     * if this still fails, we return immediately and rely on lwIP's own    *
     * retransmission timer to retry — no blocking spin here.              */
    smc_set_bank(2);
    SMC_MMU_CMD = (uint8_t)(MMU_CMD_ALLOC_TX | pages);
    smc_mmu_wait();

    uint8_t pkt_num = SMC_ARR;
    if (pkt_num & SMC_ARR_FAILED) {
        return -1;  /* silent: lwIP will retransmit via its timer */
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
    uint16_t hw_len = SMC_DATA & 0x7FF;

    if (hw_len < 6) {
        SMC_MMU_CMD = MMU_CMD_RELEASE; /* Pop and Release (0x80) */
        smc_mmu_wait();
        return 0;
    }

    size_t payload_len = (size_t)(hw_len - 6);
    if (payload_len > max_len)
        payload_len = max_len;

    uint16_t *buf16 = (uint16_t *)buf;
    /* We must read the exact number of words left in the packet memory buffer.
     * The hardware buffer contains hw_len bytes. We already read 4 bytes.
     * So we must read (hw_len - 4 + 1) / 2 words from the data register.
     * But we only store up to payload_len.
     */
    int words_to_read = (hw_len - 4 + 1) / 2;
    int words_stored = 0;
    
    for (int i = 0; i < words_to_read; i++) {
        uint16_t data = SMC_DATA;
        if (words_stored < (int)(payload_len / 2)) {
            buf16[words_stored++] = data;
        } else if (words_stored == (int)(payload_len / 2) && (payload_len & 1)) {
            buf[payload_len - 1] = (uint8_t)data;
            words_stored++;
        }
    }

    SMC_MMU_CMD = MMU_CMD_RELEASE; /* Pop and Release (0x80) */
    smc_mmu_wait();

#if SMC_PACKET_DEBUG
    /* Print ethernet frame details */
    if (payload_len >= 14) {
        uint16_t eth_type = (buf[12] << 8) | buf[13];
        if (eth_type == 0x0800 && payload_len >= 14 + 20) {
            uint8_t *ip = buf + 14;
            uint8_t proto = ip[9];
            uint8_t ihl = (ip[0] & 0x0f) * 4;
            uint8_t *src = ip + 12;
            uint8_t *dst = ip + 16;
            if (proto == 17 && payload_len >= 14 + ihl + 8) {
                uint8_t *udp = ip + ihl;
                uint16_t sport = (udp[0] << 8) | udp[1];
                uint16_t dport = (udp[2] << 8) | udp[3];
                kprintf("[RX] IP UDP %d.%d.%d.%d:%u -> %d.%d.%d.%d:%u (eth src %02x:%02x:%02x:%02x:%02x:%02x)\n",
                        src[0], src[1], src[2], src[3], sport,
                        dst[0], dst[1], dst[2], dst[3], dport,
                        buf[6], buf[7], buf[8], buf[9], buf[10], buf[11]);
            } else {
                kprintf("[RX] IP proto:%u %d.%d.%d.%d -> %d.%d.%d.%d (eth src %02x:%02x:%02x:%02x:%02x:%02x)\n",
                        proto,
                        src[0], src[1], src[2], src[3],
                        dst[0], dst[1], dst[2], dst[3],
                        buf[6], buf[7], buf[8], buf[9], buf[10], buf[11]);
            }
        } else {
            kprintf("[RX] Len:%d DST:%02x:%02x:%02x:%02x:%02x:%02x SRC:%02x:%02x:%02x:%02x:%02x:%02x Type:0x%04x\n",
                    (int)payload_len,
                    buf[0], buf[1], buf[2], buf[3], buf[4], buf[5],
                    buf[6], buf[7], buf[8], buf[9], buf[10], buf[11],
                    eth_type);
        }
    } else {
        kprintf("[RX] Tiny frame len: %d\n", (int)payload_len);
    }
#endif

    (void)status;
    return payload_len;
}
