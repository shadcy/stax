#include "lwip/tcp.h"
#include "lwip/ip_addr.h"
#include "lwip/ip4_addr.h"
#include "lwip/dns.h"
#include "../include/console.h"
#include "../include/string.h"

extern volatile unsigned int tick_count;
extern int net_poll(void);

static volatile int fetch_done = 0;

static err_t fetch_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    (void)arg;
    (void)err;
    if (p == NULL) {
        kputs("\n[Connection closed by server]\n");
        tcp_close(tpcb);
        fetch_done = 1;
        return ERR_OK;
    }
    
    // Print payload
    for (struct pbuf *q = p; q != NULL; q = q->next) {
        for (int i = 0; i < q->len; i++) {
            kputc(((char *)q->payload)[i]);
        }
    }
    
    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

static err_t fetch_connected(void *arg, struct tcp_pcb *tpcb, err_t err) {
    (void)arg;
    if (err != ERR_OK) {
        kputs("Connection failed.\n");
        fetch_done = 1;
        return err;
    }
    
    kputs("Connected! Sending GET request...\n");
    
    const char *request = "GET / HTTP/1.0\r\nHost: example.com\r\nConnection: close\r\n\r\n";
    tcp_write(tpcb, request, strlen(request), TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);
    
    return ERR_OK;
}

static void fetch_err(void *arg, err_t err) {
    (void)arg;
    (void)err;
    kprintf("\nTCP error: %d\n", err);
    fetch_done = 1;
}

static void do_fetch(const ip_addr_t *ipaddr) {
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb) {
        kputs("Failed to create TCP PCB.\n");
        return;
    }
    
    tcp_recv(pcb, fetch_recv);
    tcp_err(pcb, fetch_err);
    
    fetch_done = 0;
    kputs("Starting TCP connection...\n");
    err_t err = tcp_connect(pcb, ipaddr, 80, fetch_connected);
    if (err != ERR_OK) {
        kprintf("tcp_connect failed: %d\n", err);
        tcp_close(pcb);
        return;
    }

    /* Synchronous wait loop with timeout (10 seconds) */
    unsigned int start = tick_count;
    kprintf("Fetch wait loop start. Start tick: %u\n", start);
    int last_print = 0;
    while (!fetch_done && (tick_count - start < 1000)) {
        net_poll();
        if (tick_count != last_print) {
            kprintf("[fetch] Tick: %u\n", tick_count);
            last_print = tick_count;
        }
        for (volatile int i = 0; i < 15000; i++) __asm__ volatile ("nop");
    }

    if (!fetch_done) {
        kputs("Fetch timed out.\n");
        tcp_abort(pcb);
    }
}

static volatile int dns_resolved = 0;
static volatile int dns_failed = 0;
static ip_addr_t dns_resolved_ip;

static void dns_found_cb(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
    (void)name;
    (void)callback_arg;
    if (ipaddr == NULL) {
        dns_failed = 1;
    } else {
        dns_resolved_ip = *ipaddr;
        dns_resolved = 1;
    }
}

void cmd_fetch(int argc, char *argv[]) {
    if (argc < 2) {
        kputs("Usage: fetch <host>\n");
        return;
    }

    ip_addr_t server_ip;
    ip4_addr_t ip4;
    if (ip4addr_aton(argv[1], &ip4)) {
        ip_addr_copy_from_ip4(server_ip, ip4);
        do_fetch(&server_ip);
        return;
    }

    dns_resolved = 0;
    dns_failed = 0;
    kprintf("Resolving host: %s\n", argv[1]);
    err_t err = dns_gethostbyname(argv[1], &server_ip, dns_found_cb, NULL);
    
    if (err == ERR_OK) {
        kputs("Resolved immediately.\n");
        do_fetch(&server_ip);
    } else if (err == ERR_INPROGRESS) {
        kputs("Resolving host (in progress)...\n");
        unsigned int start = tick_count;
        int last_print = 0;
        /* 5-second timeout for DNS */
        while (!dns_resolved && !dns_failed && (tick_count - start < 500)) {
            net_poll();
            if (tick_count != last_print) {
                kprintf("[dns] Tick: %u\n", tick_count);
                last_print = tick_count;
            }
            for (volatile int i = 0; i < 15000; i++) __asm__ volatile ("nop");
        }

        if (dns_resolved) {
            do_fetch(&dns_resolved_ip);
        } else {
            kputs("DNS resolution failed or timed out.\n");
        }
    } else {
        kprintf("DNS request failed immediately: %d\n", err);
    }
}

void fetch_init(void) {
    extern int command_register(const char *name, const char *desc, void (*func)(int, char**));
    command_register("fetch", "Fetch from HTTP server", cmd_fetch);
}
