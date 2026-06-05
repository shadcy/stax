#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/dhcp.h"
#include "lwip/dns.h"
#include "lwip/timeouts.h"
#include "lwip/ip4_addr.h"
#include "netif/ethernet.h"
#include "lwip/etharp.h"
#include "lwip/prot/ethernet.h"
#include "../include/gfx_console.h"
#include "../include/console.h"

extern err_t smc_netif_init(struct netif *netif);
extern void smc_netif_poll(struct netif *netif);

static struct netif smc_netif;

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
    netif_set_link_up(&smc_netif);
    netif_set_up(&smc_netif);
}

static void net_ensure_dns(void) {
    const ip_addr_t *dns = dns_getserver(0);
    if (dns != NULL && !ip_addr_isany(dns))
        return;
    ip_addr_t fallback;
    IP_ADDR4(&fallback, 10, 0, 2, 3);
    dns_setserver(0, &fallback);
}

int net_poll(void) {
    smc_netif_poll(&smc_netif);
    sys_check_timeouts();
    
    static u32_t last_print = 0;
    u32_t now = sys_now();
    if (now - last_print >= 1000) {
        last_print = now;
        kprintf("[NET] sys_now = %u\n", now);
    }
    
    int ret = 0;
    if (!ip4_addr_isany(netif_ip4_addr(&smc_netif))) {
        static int ip_printed = 0;
        if (!ip_printed) {
            kprintf("\n[NET] Network Ready! IP: %s\n", ip4addr_ntoa(netif_ip4_addr(&smc_netif)));
            ip_printed = 1;
            ret = 1;
        }

        net_ensure_dns();
        
        static int static_arp_done = 0;
        if (!static_arp_done) {
            ip4_addr_t gw_ip, dns_ip;
            struct eth_addr qemu_gateway_mac = ETH_ADDR(0x52, 0x55, 0x0a, 0x00, 0x02, 0x02);

            IP4_ADDR(&gw_ip, 10, 0, 2, 2);
            /* Request ARP resolution for gateway so the correct MAC is learned */
            etharp_request(&smc_netif, &gw_ip);

            IP4_ADDR(&dns_ip, 10, 0, 2, 3);
            /* QEMU slirp answers DNS at 10.0.2.3, but replies come from the gateway MAC. */
            etharp_add_static_entry(&dns_ip, &qemu_gateway_mac);

            static_arp_done = 1;
        }
    }
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
    kputs("\n");
}

void ifconfig_init(void) {
    extern int command_register(const char *name, const char *desc, void (*func)(int, char**));
    command_register("ifconfig", "Show network interface configuration", cmd_ifconfig);
}
