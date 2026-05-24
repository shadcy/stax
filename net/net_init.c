#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/dhcp.h"
#include "lwip/dns.h"
#include "lwip/timeouts.h"
#include "lwip/ip4_addr.h"
#include "netif/ethernet.h"
#include "../include/gfx_console.h"
#include "../include/console.h"

extern err_t smc_netif_init(struct netif *netif);
extern void smc_netif_poll(struct netif *netif);

static struct netif smc_netif;

void net_init(void) {
    lwip_init();

    ip4_addr_t ipaddr, netmask, gw;
    IP4_ADDR(&ipaddr, 0, 0, 0, 0);
    IP4_ADDR(&netmask, 0, 0, 0, 0);
    IP4_ADDR(&gw, 0, 0, 0, 0);

    if (netif_add(&smc_netif, &ipaddr, &netmask, &gw, NULL, smc_netif_init, ethernet_input) == NULL) {
        kputs("net: failed to add smc0 interface\n");
        return;
    }
    netif_set_default(&smc_netif);
    netif_set_link_up(&smc_netif);
    netif_set_up(&smc_netif);

    if (dhcp_start(&smc_netif) != ERR_OK) {
        kputs("net: DHCP start failed\n");
    }
}

static void net_ensure_dns(void) {
    const ip_addr_t *dns = dns_getserver(0);
    if (dns != NULL && !ip_addr_isany(dns))
        return;
    ip_addr_t fallback;
    IP_ADDR4(&fallback, 10, 0, 2, 3);
    dns_setserver(0, &fallback);
}

void net_poll(void) {
    smc_netif_poll(&smc_netif);
    sys_check_timeouts();
    if (!ip4_addr_isany(netif_ip4_addr(&smc_netif)))
        net_ensure_dns();
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
