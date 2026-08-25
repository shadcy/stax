#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/dns.h"
#include "lwip/timeouts.h"
#include "lwip/ip4_addr.h"
#include "netif/ethernet.h"
#include "lwip/etharp.h"
#include "lwip/prot/ethernet.h"
#include "../include/gfx_console.h"
#include "../include/console.h"
#include "../include/irq.h"

extern err_t smc_netif_init(struct netif *netif);
extern void smc_netif_poll(struct netif *netif);

static struct netif smc_netif;

static void net_ensure_dns(void);
int net_poll(void);

static void net_install_static_arp(void);
void net_init(void) {
    lwip_init();

    ip4_addr_t ipaddr, netmask, gw;
    IP4_ADDR(&ipaddr, 10, 0, 2, 15);
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    IP4_ADDR(&gw, 10, 0, 2, 2);

    if (netif_add(&smc_netif, &ipaddr, &netmask, &gw, NULL, smc_netif_init, ethernet_input) == NULL) {
        kputs("net: failed to add smc0 interface\n");
        return;
    }
    netif_set_default(&smc_netif);
    netif_set_up(&smc_netif);

    net_install_static_arp();
    net_ensure_dns();
}

/*
 * lwIP is built with NO_SYS=1 and is NOT thread-safe. Do not call net_poll
 * (or any lwIP API) from a second task. The old net_thread raced the
 * kernel/browser poll path and corrupted DNS/TCP state.
 *
 * Kept as an idle task so existing task_create() call sites stay valid.
 */
void net_thread_entry(void) {
    while (1) {
        asm volatile ("mcr p15, 0, %0, c7, c0, 4" : : "r" (0));
    }
}

static void net_install_static_arp(void) {
    ip4_addr_t ip;
    /* QEMU slirp gateway MAC — this is the MAC address slirp uses for ALL
     * frames it delivers to the guest (confirmed by [RX] src= in debug output).
     * Using the wrong MAC causes slirp to silently drop outbound TCP/UDP frames. */
    struct eth_addr qemu_gateway_mac = ETH_ADDR(0x52, 0x54, 0x00, 0x12, 0x34, 0x56);

    /* Critical: without a gateway ARP entry, TCP/UDP to the internet never leaves. */
    IP4_ADDR(&ip, 10, 0, 2, 2);
    etharp_add_static_entry(&ip, &qemu_gateway_mac);

    /* QEMU built-in DNS is on-link at 10.0.2.3 but replies use the gateway MAC. */
    IP4_ADDR(&ip, 10, 0, 2, 3);
    etharp_add_static_entry(&ip, &qemu_gateway_mac);

    /* Public resolvers via the same gateway MAC (slirp NAT). */
    IP4_ADDR(&ip, 8, 8, 8, 8);
    etharp_add_static_entry(&ip, &qemu_gateway_mac);
    IP4_ADDR(&ip, 1, 1, 1, 1);
    etharp_add_static_entry(&ip, &qemu_gateway_mac);
}

static void net_ensure_dns(void) {
    ip_addr_t primary, secondary;
    /* Prefer QEMU built-in DNS; fall back to public resolvers. */
    IP_ADDR4(&primary, 10, 0, 2, 3);
    IP_ADDR4(&secondary, 8, 8, 8, 8);
    dns_setserver(0, &primary);
    dns_setserver(1, &secondary);
}

int net_poll(void) {
    /*
     * Serialize against the preemptive scheduler and allow nested calls
     * from the main loop (kernel polls, then browser_update may poll again).
     */
    static int depth = 0;
    if (depth == 0)
        irq_disable();
    depth++;

    smc_netif_poll(&smc_netif);
    sys_check_timeouts();

    int ret = 0;
    if (!ip4_addr_isany(netif_ip4_addr(&smc_netif))) {
        static int ip_printed = 0;
        if (!ip_printed) {
            kprintf("\n[NET] Network Ready! IP: %s\n", ip4addr_ntoa(netif_ip4_addr(&smc_netif)));
            ip_printed = 1;
            ret = 1;
        }

        static int dns_done = 0;
        if (!dns_done) {
            net_ensure_dns();
            dns_done = 1;
        }
    }

    depth--;
    if (depth == 0)
        irq_enable();
    return ret;
}

void cmd_ifconfig(int argc, char *argv[]) {
    (void)argc; (void)argv;
    kputs("Interface: smc0\n");

    kputs("  IP Address : ");
    kputs(ip4addr_ntoa(netif_ip4_addr(&smc_netif)));
    kputs("\n  Subnet Mask: ");
    kputs(ip4addr_ntoa(netif_ip4_netmask(&smc_netif)));
    kputs("\n  Gateway    : ");
    kputs(ip4addr_ntoa(netif_ip4_gw(&smc_netif)));
    kputs("\n  Link UP    : ");
    kputs(netif_is_link_up(&smc_netif) ? "Yes" : "No");

    kprintf("\n  MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n",
            smc_netif.hwaddr[0], smc_netif.hwaddr[1], smc_netif.hwaddr[2],
            smc_netif.hwaddr[3], smc_netif.hwaddr[4], smc_netif.hwaddr[5]);
    kputs("  DNS Server : ");
    kputs(ipaddr_ntoa(dns_getserver(0)));
    kputs("\n  DNS Backup : ");
    kputs(ipaddr_ntoa(dns_getserver(1)));
    kputs("\n");
}

void ifconfig_init(void) {
}
