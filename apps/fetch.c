#include "lwip/tcp.h"
#include "lwip/ip_addr.h"
#include "lwip/ip4_addr.h"
#include "lwip/dns.h"
#include "../include/console.h"
#include "../include/string.h"

extern volatile unsigned int tick_count;
extern int net_poll(void);

static volatile int fetch_done = 0;
static const char *fetch_host = NULL;
static const char *fetch_path = "";

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
    
    const char *req_a = "GET ";
    const char *req_path_sep = "/";
    const char *req_b = " HTTP/1.0\r\nHost: ";
    const char *req_c = "\r\nConnection: close\r\n\r\n";
    tcp_write(tpcb, req_a, strlen(req_a), TCP_WRITE_FLAG_COPY);
    tcp_write(tpcb, req_path_sep, strlen(req_path_sep), TCP_WRITE_FLAG_COPY);
    tcp_write(tpcb, fetch_path, strlen(fetch_path), TCP_WRITE_FLAG_COPY);
    tcp_write(tpcb, req_b, strlen(req_b), TCP_WRITE_FLAG_COPY);
    tcp_write(tpcb, fetch_host, strlen(fetch_host), TCP_WRITE_FLAG_COPY);
    tcp_write(tpcb, req_c, strlen(req_c), TCP_WRITE_FLAG_COPY);
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
    kputs("Fetching data...  ");
    unsigned int last_print = start;
    const char spinner[] = {'/', '-', '\\', '|'};
    int spin_idx = 0;
    while (!fetch_done && (tick_count - start < 1000)) {
        net_poll();
        if (tick_count - last_print >= 25) {
            kputc('\b');
            kputc(spinner[spin_idx]);
            spin_idx = (spin_idx + 1) % 4;
            last_print = tick_count;
        }
        if (kgetc() == '\x1b') {
            kputs("\n[fetch] Canceled by user.\n");
            break;
        }
        for (volatile int i = 0; i < 15000; i++) __asm__ volatile ("nop");
    }
    kputs("\n");

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
        kputs("Usage: fetch <host_or_url>\n");
        return;
    }

    char *host = argv[1];
    if (strncmp(host, "http://", 7) == 0)
        host += 7;
    else if (strncmp(host, "https://", 8) == 0) {
        kputs("HTTPS is not supported; use http:// or a plain host.\n");
        return;
    }

    char *path = strchr(host, '/');
    if (path != NULL) {
        *path++ = '\0';
        fetch_path = path;
    } else {
        fetch_path = "";
    }
    fetch_host = host;

    ip_addr_t server_ip;
    ip4_addr_t ip4;
    if (ip4addr_aton(host, &ip4)) {
        ip_addr_copy_from_ip4(server_ip, ip4);
        do_fetch(&server_ip);
        return;
    }

    dns_resolved = 0;
    dns_failed = 0;
    kprintf("Resolving host: %s\n", host);
    err_t err = dns_gethostbyname(host, &server_ip, dns_found_cb, NULL);
    
    if (err == ERR_OK) {
        kputs("Resolved immediately.\n");
        do_fetch(&server_ip);
    } else if (err == ERR_INPROGRESS) {
        kputs("Resolving host (in progress)...  ");
        unsigned int start = tick_count;
        unsigned int last_print = start;
        const char spinner[] = {'/', '-', '\\', '|'};
        int spin_idx = 0;
        /* 5-second timeout for DNS */
        while (!dns_resolved && !dns_failed && (tick_count - start < 500)) {
            net_poll();
            if (tick_count - last_print >= 25) {
                kputc('\b');
                kputc(spinner[spin_idx]);
                spin_idx = (spin_idx + 1) % 4;
                last_print = tick_count;
            }
            if (kgetc() == '\x1b') {
                kputs("\n[dns] Canceled by user.\n");
                dns_failed = 1;
                break;
            }
            for (volatile int i = 0; i < 15000; i++) __asm__ volatile ("nop");
        }
        kputs("\n");

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
