#ifndef DRIVERS_NET_SMC91C111_H
#define DRIVERS_NET_SMC91C111_H

#include <stdint.h>
#include <stddef.h>

#define SMC_BASE 0x10010000

#define SMC_REG16(offset) (*(volatile uint16_t *)(SMC_BASE + (offset)))
#define SMC_REG8(offset)  (*(volatile uint8_t *)(SMC_BASE + (offset)))

#define SMC_BSR      SMC_REG16(0x0E)

/* Bank 0 */
#define SMC_TCR      SMC_REG16(0x00)
#define SMC_RCR      SMC_REG16(0x04)

/* Bank 1 */
#define SMC_CR       SMC_REG16(0x00)
#define SMC_IA0      SMC_REG16(0x04)
#define SMC_IA1      SMC_REG16(0x06)
#define SMC_IA2      SMC_REG16(0x08)

/* Bank 2 (byte offsets per Linux smc91x.h / QEMU) */
#define SMC_MMU_CMD  SMC_REG8(0x00)
#define SMC_PKT_NUM  SMC_REG8(0x02)
#define SMC_ARR      SMC_REG8(0x03)
#define SMC_RXFIFO   SMC_REG8(0x05)
#define SMC_PTR      SMC_REG16(0x06)
#define SMC_DATA     SMC_REG16(0x08)

#define SMC_RXFIFO_EMPTY 0x80
#define SMC_ARR_FAILED   0x80

#define MMU_CMD_ALLOC_TX 0x20
#define MMU_CMD_RESET    0x40
#define MMU_CMD_RELEASE  0x80
#define MMU_CMD_ENQUEUE  0xC0

#define PTR_RCV       0x8000
#define PTR_AUTO_INC  0x4000
#define PTR_READ      0x2000

#define TCR_ENABLE    0x0001
#define TCR_PAD_EN    0x0080
#define RCR_RXEN      0x0100
#define RCR_STRIP_CRC 0x0200
#define RCR_PRMS      0x0002

void smc91c111_init(void);
int smc91c111_tx(const uint8_t *data, size_t len);
size_t smc91c111_rx(uint8_t *buf, size_t max_len);

#endif
