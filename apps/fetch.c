#include "lwip/tcp.h"
#include "lwip/ip_addr.h"
#include "lwip/ip4_addr.h"
#include "lwip/dns.h"
#include "../include/console.h"
#include "../include/string.h"

static err_t fetch_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    (void)arg;
    (void)err;
    if (p == NULL) {
        kputs("\n[Connection closed by server]\n");
        tcp_close(tpcb);
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
    kputs("\nTCP error.\n");
}

static void do_fetch(const ip_addr_t *ipaddr) {
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb) {
        kputs("Failed to create TCP PCB.\n");
        return;
    }
    
    tcp_recv(pcb, fetch_recv);
    tcp_err(pcb, fetch_err);
    
    err_t err = tcp_connect(pcb, ipaddr, 80, fetch_connected);
    if (err != ERR_OK) {
        kputs("tcp_connect failed.\n");
        tcp_close(pcb);
    }
}

static void dns_found_cb(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
    (void)name;
    (void)callback_arg;
    if (ipaddr == NULL) {
        kputs("DNS resolution failed.\n");
        return;
    }
    
    kputs("Resolved hostname.\n");
    do_fetch(ipaddr);
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

    err_t err = dns_gethostbyname(argv[1], &server_ip, dns_found_cb, NULL);
    
    if (err == ERR_OK) {
        // Cached or IP address passed directly
        kputs("Resolved immediately.\n");
        do_fetch(&server_ip);
    } else if (err == ERR_INPROGRESS) {
        kputs("Resolving host...\n");
    } else {
        kputs("DNS request failed.\n");
    }
}

void fetch_init(void) {
    extern int command_register(const char *name, const char *desc, void (*func)(int, char**));
    command_register("fetch", "Fetch from HTTP server", cmd_fetch);
}
