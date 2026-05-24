#include "lwip/raw.h"
#include "lwip/icmp.h"
#include "lwip/ip_addr.h"
#include "lwip/ip4_addr.h"
#include "lwip/ip.h"
#include "lwip/inet_chksum.h"
#include "lwip/dns.h"
#include "lwip/prot/icmp.h"
#include "../include/console.h"
#include "../include/string.h"

#define PING_ID    0xAFAF
#define PING_SIZE  (sizeof(struct icmp_echo_hdr) + 32)

static struct raw_pcb *ping_pcb;
static u16_t ping_seq_num = 0;

static u8_t ping_recv(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr) {
    struct icmp_echo_hdr *iecho;
    (void)arg;
    (void)pcb;
    (void)addr;

    if (p->tot_len < PBUF_IP_HLEN + sizeof(struct icmp_echo_hdr))
        return 0;
    if (pbuf_remove_header(p, PBUF_IP_HLEN) != 0)
        return 0;

    iecho = (struct icmp_echo_hdr *)p->payload;
    if (iecho->id == PING_ID && iecho->seqno == lwip_htons(ping_seq_num) &&
        ICMPH_TYPE(iecho) == ICMP_ER) {
        kputs("Ping reply received!\n");
        pbuf_free(p);
        return 1;
    }

    pbuf_add_header(p, PBUF_IP_HLEN);
    return 0;
}

static void do_ping(const ip_addr_t *target_ip) {
    if (!ping_pcb) {
        ping_pcb = raw_new(IP_PROTO_ICMP);
        raw_recv(ping_pcb, ping_recv, NULL);
        raw_bind(ping_pcb, IP_ADDR_ANY);
    }

    struct pbuf *p = pbuf_alloc(PBUF_IP, PING_SIZE, PBUF_RAM);
    if (!p || p->next != NULL) {
        kputs("Failed to allocate ping buffer.\n");
        if (p) pbuf_free(p);
        return;
    }

    struct icmp_echo_hdr *iecho = (struct icmp_echo_hdr *)p->payload;
    ICMPH_TYPE_SET(iecho, ICMP_ECHO);
    ICMPH_CODE_SET(iecho, 0);
    iecho->chksum = 0;
    iecho->id = PING_ID;
    iecho->seqno = lwip_htons(++ping_seq_num);

    for (int i = 0; i < 32; i++)
        ((u8_t *)iecho)[sizeof(struct icmp_echo_hdr) + i] = (u8_t)i;

    iecho->chksum = inet_chksum(iecho, PING_SIZE);

    kputs("Pinging target IP...\n");
    raw_sendto(ping_pcb, p, target_ip);
    pbuf_free(p);
}

static void ping_dns_found_cb(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
    (void)name;
    (void)callback_arg;
    if (ipaddr == NULL) {
        kputs("DNS resolution failed.\n");
        return;
    }
    kputs("Resolved hostname.\n");
    do_ping(ipaddr);
}

void cmd_ping(int argc, char *argv[]) {
    if (argc < 2) {
        kputs("Usage: ping <host_or_ip>\n");
        return;
    }

    char *host = argv[1];
    if (strncmp(host, "http://", 7) == 0)
        host += 7;
    else if (strncmp(host, "https://", 8) == 0)
        host += 8;

    ip_addr_t target_ip;
    ip4_addr_t ip4;
    if (ip4addr_aton(host, &ip4)) {
        ip_addr_copy_from_ip4(target_ip, ip4);
        do_ping(&target_ip);
        return;
    }

    err_t err = dns_gethostbyname(host, &target_ip, ping_dns_found_cb, NULL);
    if (err == ERR_OK)
        do_ping(&target_ip);
    else if (err == ERR_INPROGRESS)
        kputs("Resolving host...\n");
    else
        kputs("DNS request failed.\n");
}

void ping_init(void) {
    extern int command_register(const char *name, const char *desc, void (*func)(int, char**));
    command_register("ping", "Ping an IP address", cmd_ping);
}
