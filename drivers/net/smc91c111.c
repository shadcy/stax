#include "smc91c111.h"

static inline void smc_set_bank(uint16_t bank) {
    SMC_BSR = bank;
}

void smc91c111_init(void) {
    smc_set_bank(2);
    SMC_MMU_CMD = MMU_CMD_RESET;

    for (volatile int i = 0; i < 10000; i++);

    smc_set_bank(0);
    SMC_TCR = TCR_ENABLE | TCR_PAD_EN;
    SMC_RCR = RCR_RXEN | RCR_STRIP_CRC | RCR_PRMS;

    smc_set_bank(1);
    SMC_CR = 0x1000 | 0x0100 | 0x0080;
}

int smc91c111_tx(const uint8_t *data, size_t len) {
    if (len > 1536 || len == 0)
        return -1;

    int packet_length = (int)len + 6;
    int pages = (packet_length + 255) / 256;

    smc_set_bank(2);
    SMC_MMU_CMD = (uint8_t)(MMU_CMD_ALLOC_TX | pages);

    for (volatile int i = 0; i < 100; i++);

    uint8_t pkt_num = SMC_ARR;
    if (pkt_num & SMC_ARR_FAILED)
        return -1;

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

    if (len < 6) {
        SMC_MMU_CMD = MMU_CMD_RELEASE;
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

    (void)status;
    return payload_len;
}
