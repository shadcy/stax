#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/etharp.h"
#include "../drivers/net/smc91c111.h"
#include "../include/string.h"

static err_t smc_link_output(struct netif *netif, struct pbuf *p) {
    (void)netif;
    
    // In NO_SYS mode,    // Copy to contiguous buffer if chained
    uint8_t buffer[1536] __attribute__((aligned(4)));
    size_t len = pbuf_copy_partial(p, buffer, p->tot_len, 0);
    
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
    SMC_BSR = 1;
    uint16_t mac0 = SMC_IA0;
    uint16_t mac1 = SMC_IA1;
    uint16_t mac2 = SMC_IA2;
    netif->hwaddr[0] = mac0 & 0xFF;
    netif->hwaddr[1] = mac0 >> 8;
    netif->hwaddr[2] = mac1 & 0xFF;
    netif->hwaddr[3] = mac1 >> 8;
    netif->hwaddr[4] = mac2 & 0xFF;
    netif->hwaddr[5] = mac2 >> 8;

    return ERR_OK;
}

void smc_netif_poll(struct netif *netif) {
    uint8_t rx_buf[1536];
    size_t len;
    while ((len = smc91c111_rx(rx_buf, sizeof(rx_buf))) > 0) {
        struct pbuf *p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
        if (p) {
            pbuf_take(p, rx_buf, len);
            if (netif->input(p, netif) != ERR_OK) {
                pbuf_free(p);
            }
        }
    }
}
