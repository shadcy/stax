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

extern volatile unsigned int tick_count;
extern int net_poll(void);

static struct raw_pcb *ping_pcb;
static u16_t ping_seq_num = 0;
static volatile int ping_reply_received = 0;

static u8_t ping_recv(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr) {
    struct icmp_echo_hdr *iecho;
    (void)arg;
    (void)pcb;
    (void)addr;

    kprintf("[ping_recv] Packet received! tot_len = %u\n", p->tot_len);

    if (p->tot_len < PBUF_IP_HLEN + sizeof(struct icmp_echo_hdr)) {
        kprintf("[ping_recv] Packet too short: %u < %d\n", p->tot_len, PBUF_IP_HLEN + sizeof(struct icmp_echo_hdr));
        return 0;
    }
    
    /* Let's examine the raw IP payload before removing header */
    uint8_t *raw_payload = (uint8_t *)p->payload;
    kprintf("[ping_recv] IP header info: version/IHL = 0x%02x, proto = %u\n", raw_payload[0], raw_payload[9]);

    if (pbuf_remove_header(p, PBUF_IP_HLEN) != 0) {
        kprintf("[ping_recv] Failed to remove IP header\n");
        return 0;
    }

    iecho = (struct icmp_echo_hdr *)p->payload;
    kprintf("[ping_recv] ICMP: type = %u, code = %u, id = 0x%04x, seq = %u (expected id = 0x%04x, seq = %u)\n",
            ICMPH_TYPE(iecho), ICMPH_CODE(iecho), iecho->id, lwip_ntohs(iecho->seqno), PING_ID, ping_seq_num);

    if (iecho->id == PING_ID && iecho->seqno == lwip_htons(ping_seq_num) &&
        ICMPH_TYPE(iecho) == ICMP_ER) {
        kputs("Ping reply received!\n");
        ping_reply_received = 1;
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

    kprintf("Pinging target IP... (seq=%u)\n", ping_seq_num);
    ping_reply_received = 0;
    raw_sendto(ping_pcb, p, target_ip);
    pbuf_free(p);

    /* Synchronous wait loop with timeout (3 seconds) */
    unsigned int start = tick_count;
    kprintf("Start tick: %u\n", start);
    int last_print = 0;
    while (!ping_reply_received && (tick_count - start < 300)) {
        net_poll();
        if (tick_count != last_print) {
            kprintf("Tick: %u\n", tick_count);
            last_print = tick_count;
        }
        for (volatile int i = 0; i < 15000; i++) __asm__ volatile ("nop");
    }

    if (!ping_reply_received) {
        kputs("Ping timed out.\n");
    }
}

static volatile int dns_resolved = 0;
static volatile int dns_failed = 0;
static ip_addr_t dns_resolved_ip;

static void ping_dns_found_cb(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
    (void)name;
    (void)callback_arg;
    if (ipaddr == NULL) {
        dns_failed = 1;
    } else {
        dns_resolved_ip = *ipaddr;
        dns_resolved = 1;
    }
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

    dns_resolved = 0;
    dns_failed = 0;
    err_t err = dns_gethostbyname(host, &target_ip, ping_dns_found_cb, NULL);
    if (err == ERR_OK) {
        do_ping(&target_ip);
    } else if (err == ERR_INPROGRESS) {
        kputs("Resolving host...\n");
        unsigned int start = tick_count;
        /* 5-second timeout for DNS */
        while (!dns_resolved && !dns_failed && (tick_count - start < 500)) {
            net_poll();
            for (volatile int i = 0; i < 15000; i++) __asm__ volatile ("nop");
        }

        if (dns_resolved) {
            do_ping(&dns_resolved_ip);
        } else {
            kputs("DNS resolution failed or timed out.\n");
        }
    } else {
        kputs("DNS request failed immediately.\n");
    }
}

void ping_init(void) {
    extern int command_register(const char *name, const char *desc, void (*func)(int, char**));
    command_register("ping", "Ping an IP address", cmd_ping);
}
