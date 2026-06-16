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
    netif_set_up(&smc_netif);
    netif_set_link_up(&smc_netif);

    ip_addr_t dns;
    IP_ADDR4(&dns, 8, 8, 8, 8);
    dns_setserver(0, &dns);

    /* QEMU static ARP hacks */
    ip4_addr_t gw_ip;
    IP4_ADDR(&gw_ip, 10, 0, 2, 2);
    struct eth_addr gw_mac = {{0x52, 0x54, 0x00, 0x12, 0x35, 0x02}};
    etharp_add_static_entry(&gw_ip, &gw_mac);
}



int net_poll(void) {
    smc_netif_poll(&smc_netif);
    sys_check_timeouts();
    
    int ret = 0;
    if (!ip4_addr_isany(netif_ip4_addr(&smc_netif))) {
        static int ip_printed = 0;
        if (!ip_printed) {
            kprintf("\n[NET] Network Ready! IP: %s\n", ip4addr_ntoa(netif_ip4_addr(&smc_netif)));
            ip_printed = 1;
            ret = 1; /* Only return 1 when we first print it, to redraw the line ONCE */
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
