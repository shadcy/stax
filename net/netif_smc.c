#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/etharp.h"
#include "../drivers/net/smc91c111.h"
#include "../include/string.h"

static err_t smc_link_output(struct netif *netif, struct pbuf *p) {
    (void)netif;
    
    // Copy to contiguous buffer if chained
    uint8_t buffer[1536] __attribute__((aligned(4)));
    /* Copy pbuf to flat buffer */
    pbuf_copy_partial(p, buffer, p->tot_len, 0);
    size_t len = p->tot_len;

    /* Transmit the packet via the hardware driver */
    if (smc91c111_tx(buffer, len) == 0) {
        return ERR_OK;
    }
    return ERR_IF;
}

err_t smc_netif_init(struct netif *netif) {
    netif->name[0] = 's';
    netif->name[1] = 'm';
    netif->linkoutput = smc_link_output;
    netif->output     = etharp_output;
    netif->mtu        = 1500;
    netif->flags      = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

    smc91c111_init();
    netif->hwaddr_len = 6;
    
    /* Hardcode MAC: 52:54:00:12:34:57 */
    netif->hwaddr[0] = 0x52;
    netif->hwaddr[1] = 0x54;
    netif->hwaddr[2] = 0x00;
    netif->hwaddr[3] = 0x12;
    netif->hwaddr[4] = 0x34;
    netif->hwaddr[5] = 0x57;

    /* Program into SMC chip (Bank 1) */
    SMC_BSR = 1;
    SMC_IA0 = (netif->hwaddr[1] << 8) | netif->hwaddr[0];
    SMC_IA1 = (netif->hwaddr[3] << 8) | netif->hwaddr[2];
    SMC_IA2 = (netif->hwaddr[5] << 8) | netif->hwaddr[4];

    return ERR_OK;
}

void smc_netif_poll(struct netif *netif) {
    uint8_t rx_buf[1536] __attribute__((aligned(4)));
    size_t len;
    /* Process at most 4 packets per poll to avoid starving the main loop */
    for (int pkt = 0; pkt < 4; pkt++) {
        len = smc91c111_rx(rx_buf, sizeof(rx_buf));
        if (len == 0) break;

        /* Hub echo filter: QEMU -net nic -net user creates a virtual hub that
         * echoes every TX frame back to the sender's own RX FIFO.  Drop any
         * frame whose Ethernet source MAC (bytes 6-11) matches our own NIC
         * address.  Slirp's real responses use src_mac=52:54:00:12:34:56
         * (the slirp gateway) which is different from our 52:54:00:12:34:57. */
        if (len >= 12 &&
            rx_buf[6]  == netif->hwaddr[0] &&
            rx_buf[7]  == netif->hwaddr[1] &&
            rx_buf[8]  == netif->hwaddr[2] &&
            rx_buf[9]  == netif->hwaddr[3] &&
            rx_buf[10] == netif->hwaddr[4] &&
            rx_buf[11] == netif->hwaddr[5]) {
            /* Our own frame echoed by the hub — discard silently */
            continue;
        }

        /* Allocate len + 2 to allow shifting the payload for 4-byte IP header alignment */
        struct pbuf *p = pbuf_alloc(PBUF_RAW, len + 2, PBUF_POOL);
        if (p) {
            /* Shift payload forward by 2 bytes. Since PBUF_RAW payload is 4-byte aligned,
               payload + 2 + 14 (eth header) = 16 (4-byte aligned IP header) */
            pbuf_remove_header(p, 2);
            pbuf_take(p, rx_buf, len);

            if (1) {
                kprintf("DUMP: ");
                for (int i = 0; i < (p->tot_len < 40 ? p->tot_len : 40); i++) {
                    kprintf("%x ", ((unsigned char*)p->payload)[i]);
                    if (i % 16 == 15) kprintf("\n      ");
                }
                kprintf("\n");
            }

            if (netif->input(p, netif) != ERR_OK) {
                pbuf_free(p);
            }
        }
    }
}
