#include "smc91c111.h"
#include "console.h"

/* 0 = silent, 1 = show IPv4/TCP/UDP frames only (no ARP spam) */
#define SMC_IP_DEBUG 1

static inline void smc_set_bank(uint16_t bank) {
    SMC_BSR = bank;
}

static inline void smc_mmu_wait(void) {
    smc_set_bank(2);
    while (SMC_MMU_CMD & 0x01) {
        __asm__ volatile ("nop");
    }
}

#if SMC_IP_DEBUG
static void smc_print_ip(const char *dir, const uint8_t *d, size_t len) {
    if (len < 14) return;
    uint16_t type = ((uint16_t)d[12] << 8) | d[13];
    if (type == 0x0806) {
        kprintf("[%s-ARP] %x:%x:%x:%x:%x:%x -> %x:%x:%x:%x:%x:%x len=%d\n",
                dir,
                d[6], d[7], d[8], d[9], d[10], d[11],
                d[0], d[1], d[2], d[3], d[4], d[5],
                (int)len);
        kprintf("ARP-DUMP:");
        for (int i = 0; i < len; i++) {
            kprintf(" %x", d[i]);
            if ((i % 16) == 15) kprintf("\n         ");
        }
        kprintf("\n");
        return;
    }
    if (type != 0x0800) return;
    if (len < 34) return;
    uint8_t proto = d[23];
    if (proto != 6 && proto != 17) return; // Only TCP/UDP

    uint16_t flags = (proto == 6 && len >= 47) ? d[47] : 0;
    const char *pname = proto == 6 ? "TCP" : "UDP";
    uint16_t ip_csum = (d[24] << 8) | d[25];
    uint16_t tcp_csum = (proto == 6 && len >= 52) ? ((d[50] << 8) | d[51]) : 0;
    uint32_t seq = (proto == 6 && len >= 42) ? ((uint32_t)d[38]<<24 | (uint32_t)d[39]<<16 | (uint32_t)d[40]<<8 | (uint32_t)d[41]) : 0;
    uint32_t ack = (proto == 6 && len >= 46) ? ((uint32_t)d[42]<<24 | (uint32_t)d[43]<<16 | (uint32_t)d[44]<<8 | (uint32_t)d[45]) : 0;

    kprintf("[%s-%s] %x:%x:%x:%x:%x:%x -> %x:%x:%x:%x:%x:%x | %d.%d.%d.%d -> %d.%d.%d.%d len=%d%s%s%s (seq=%x ack=%x ipcsum=%x tcpcsum=%x)\n",
            dir, pname,
            d[6], d[7], d[8], d[9], d[10], d[11],
            d[0], d[1], d[2], d[3], d[4], d[5],
            d[26], d[27], d[28], d[29],
            d[30], d[31], d[32], d[33],
            (int)len,
            (flags & 0x02) ? " SYN" : "",
            (flags & 0x10) ? " ACK" : "",
            (flags & 0x04) ? " RST" : "",
            seq, ack, ip_csum, tcp_csum);

    if (proto == 6 && (flags & 0x02)) {
        kprintf("DUMP:");
        for (int i = 0; i < len; i++) {
            kprintf(" %x", d[i]);
            if ((i % 16) == 15) kprintf("\n     ");
        }
        kprintf("\n");
    }
}
#endif

void smc_free_tx_packets(void) {
    smc_set_bank(2);
    /* Drain the TX-done FIFO. SMC_TXFIFO (offset 0x05) holds completed TX
     * packet numbers. MMU_CMD_FREEPKT (0xA0) removes the top entry from the
     * TX done FIFO AND frees its memory page — exactly what Linux smc91x.c
     * does (MC_FREEPKT). Do NOT use MMU_CMD_REMOVE (0x60): that removes from
     * the RX FIFO, not the TX done FIFO. */
    for (int i = 0; i < 8; i++) {
        uint8_t tx_pkt = SMC_TXFIFO;  /* TX done FIFO (offset 0x05) */
        if (tx_pkt & 0x80)
            break;                     /* TX done FIFO is empty */
        (void)tx_pkt;
        SMC_MMU_CMD = MMU_CMD_FREEPKT; /* pop TX done FIFO top + free memory */
    }
}



void smc91c111_init(void) {
    smc_set_bank(2);
    SMC_MMU_CMD = MMU_CMD_RESET;
    smc_mmu_wait();

    smc_set_bank(0);
    SMC_TCR = TCR_ENABLE | TCR_PAD_EN;
    /* Do NOT enable promiscuous mode (RCR_PRMS): under QEMU user-net it
     * floods the RX FIFO with our own TX echoes and drops real replies. */
    SMC_RCR = RCR_RXEN | RCR_STRIP_CRC;

    smc_set_bank(1);
    SMC_CR = 0x1000 | 0x0100 | 0x0080; /* enable TX / RX / DMA burst bits */
}

int smc91c111_tx(const uint8_t *data, size_t len) {
    if (len > 1536 || len == 0)
        return -1;

    smc_free_tx_packets();

#if SMC_IP_DEBUG
    smc_print_ip("TX", data, len);
#endif

    int pad_len = 0;
    if (len < 60) {
        pad_len = 60 - len;
    }
    
    int total_len = len + pad_len;
    int even_len = (total_len + 1) & ~1;
    int packet_length = even_len + 6;
    int pages = (packet_length + 255) / 256;

    smc_set_bank(2);
    SMC_MMU_CMD = (uint8_t)(MMU_CMD_ALLOC_TX | pages);
    smc_mmu_wait();

    uint8_t pkt_num = SMC_ARR;
    if (pkt_num & SMC_ARR_FAILED) {
        kprintf("[TX] MMU alloc FAILED (pages=%d)\n", pages);
        return -1;
    }

    SMC_PKT_NUM = pkt_num;
    SMC_PTR = PTR_AUTO_INC; // for write

    SMC_DATA = 0; /* status word, ignored by MAC on TX */
    SMC_DATA = (uint16_t)(packet_length & 0x7FF);

    const uint16_t *data16 = (const uint16_t *)data;
    for (size_t i = 0; i < len / 2; i++)
        SMC_DATA = data16[i];

    if (len & 1) {
        if (pad_len > 0) {
            SMC_DATA = (uint16_t)(data[len - 1]);
            pad_len--;
            for (int i = 0; i < pad_len / 2; i++) {
                SMC_DATA = 0;
            }
            if (pad_len & 1) {
                SMC_DATA = (0x20 << 8); // Control byte: odd length
            } else {
                SMC_DATA = 0; // Control byte is 0 because total is even
            }
        } else {
            SMC_DATA = (uint16_t)(data[len - 1] | (0x20 << 8));
        }
    } else {
        for (int i = 0; i < pad_len / 2; i++) {
            SMC_DATA = 0;
        }
        if (pad_len & 1) {
             SMC_DATA = (0x20 << 8);
        } else {
             if (pad_len > 0 || even_len > total_len) {
                 SMC_DATA = 0;
             }
        }
    }

    SMC_PKT_NUM = pkt_num;
    SMC_MMU_CMD = MMU_CMD_ENQUEUE;
    smc_mmu_wait();

    return 0;
}

size_t smc91c111_rx(uint8_t *buf, size_t max_len) {
    smc_set_bank(2);

    uint8_t pkt_num = SMC_RXFIFO;
    if (pkt_num & SMC_RXFIFO_EMPTY)
        return 0;

    SMC_PKT_NUM = pkt_num;
    SMC_PTR = PTR_RCV | PTR_READ | PTR_AUTO_INC;

    uint16_t status = SMC_DATA;
    uint16_t len = SMC_DATA & 0x7FF;

    /*
     * QEMU/SMSC byte-count includes the 2-byte status, 2-byte count, and
     * 2-byte control trailer (6 bytes total). Payload is len - 6.
     */
    if (len < 6) {
        SMC_MMU_CMD = MMU_CMD_RELEASE;
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

    SMC_MMU_CMD = MMU_CMD_RELEASE;
    smc_mmu_wait();

#if SMC_IP_DEBUG
    smc_print_ip("RX", buf, payload_len);
#endif

    (void)status;
    return payload_len;
}
