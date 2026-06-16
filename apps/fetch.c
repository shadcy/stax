#include "lwip/tcp.h"
#include "lwip/ip_addr.h"
#include "lwip/ip4_addr.h"
#include "lwip/dns.h"
#include "../include/console.h"
#include "../include/string.h"
#include "../fs/fatfs/ff.h"
#include "bearssl.h"

extern volatile unsigned int tick_count;
extern int net_poll(void);

static volatile int fetch_done = 0;
static const char *fetch_host = NULL;
static const char *fetch_path = "";
static int fetch_is_https = 0;
static int header_state = 0;
static unsigned int bytes_downloaded = 0;
static unsigned int last_st = 0xFFFF;

static br_ssl_client_context sc;
static br_x509_minimal_context xc;
static br_hmac_drbg_context rng;
static unsigned char iobuf[BR_SSL_BUFSIZE_BIDI];

static FIL dl_file;
static int file_is_open = 0;
static int headers_parsed = 0;
static int req_sent = 0;

/* Wrapper to bypass X.509 validation */
typedef struct {
    const br_x509_class *vtable;
    br_x509_minimal_context *minimal;
} x509_no_verify_context;

static void nv_start_chain(const br_x509_class **ctx, const char *server_name) {
    x509_no_verify_context *c = (x509_no_verify_context *)ctx;
    c->minimal->vtable->start_chain((const br_x509_class **)&c->minimal, server_name);
}
static void nv_start_cert(const br_x509_class **ctx, uint32_t length) {
    x509_no_verify_context *c = (x509_no_verify_context *)ctx;
    c->minimal->vtable->start_cert((const br_x509_class **)&c->minimal, length);
}
static void nv_append(const br_x509_class **ctx, const unsigned char *buf, size_t len) {
    x509_no_verify_context *c = (x509_no_verify_context *)ctx;
    c->minimal->vtable->append((const br_x509_class **)&c->minimal, buf, len);
}
static void nv_end_cert(const br_x509_class **ctx) {
    x509_no_verify_context *c = (x509_no_verify_context *)ctx;
    c->minimal->vtable->end_cert((const br_x509_class **)&c->minimal);
}
static unsigned nv_end_chain(const br_x509_class **ctx) {
    x509_no_verify_context *c = (x509_no_verify_context *)ctx;
    unsigned err = c->minimal->vtable->end_chain((const br_x509_class **)&c->minimal);
    if (err) {
        // Ignoring error!
    }
    return 0; // Force success
}
static const br_x509_pkey *nv_get_pkey(const br_x509_class *const *ctx, unsigned *usages) {
    x509_no_verify_context *c = (x509_no_verify_context *)ctx;
    return c->minimal->vtable->get_pkey((const br_x509_class *const *)&c->minimal, usages);
}
static const br_x509_class nv_vtable = {
    sizeof(x509_no_verify_context),
    nv_start_chain,
    nv_start_cert,
    nv_append,
    nv_end_cert,
    nv_end_chain,
    nv_get_pkey
};
static x509_no_verify_context nv_ctx;

/* Converts arbitrary URL path into a strictly valid FAT 8.3 filename (uppercase, sanitized) */
static void get_83_filename(const char *path, char *out_name) {
    const char *filename = strrchr(path, '/');
    if (filename) {
        filename++;
    } else {
        filename = path;
    }
    if (!filename || *filename == '\0') {
        filename = "DOWNLOAD.BIN";
    }

    const char *dot = strrchr(filename, '.');
    char name[128] = "";
    char ext[32] = "";

    if (dot && dot != filename) {
        int name_len = dot - filename;
        if (name_len > 127) name_len = 127;
        memcpy(name, filename, name_len);
        name[name_len] = '\0';
        
        int ext_len = 0;
        const char *ext_src = dot + 1;
        while (ext_src[ext_len] && ext_len < 31) {
            ext[ext_len] = ext_src[ext_len];
            ext_len++;
        }
        ext[ext_len] = '\0';
    } else {
        int name_len = 0;
        while (filename[name_len] && name_len < 127) {
            name[name_len] = filename[name_len];
            name_len++;
        }
        name[name_len] = '\0';
        ext[0] = '\0';
    }

    char clean_name[9];
    int ni = 0;
    for (int i = 0; name[i] && ni < 8; i++) {
        char c = name[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
            if (c >= 'a' && c <= 'z') c -= 32;
            clean_name[ni++] = c;
        }
    }
    clean_name[ni] = '\0';
    if (ni == 0) {
        clean_name[0] = 'D'; clean_name[1] = 'O'; clean_name[2] = 'W'; clean_name[3] = 'N';
        clean_name[4] = 'L'; clean_name[5] = 'O'; clean_name[6] = 'A'; clean_name[7] = 'D';
        clean_name[8] = '\0';
    }

    char clean_ext[4];
    int ei = 0;
    for (int i = 0; ext[i] && ei < 3; i++) {
        char c = ext[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
            if (c >= 'a' && c <= 'z') c -= 32;
            clean_ext[ei++] = c;
        }
    }
    clean_ext[ei] = '\0';
    if (ei == 0) {
        clean_ext[0] = 'B'; clean_ext[1] = 'I'; clean_ext[2] = 'N'; clean_ext[3] = '\0';
    }

    int out_i = 0;
    for (int i = 0; clean_name[i]; i++) {
        out_name[out_i++] = clean_name[i];
    }
    out_name[out_i++] = '.';
    for (int i = 0; clean_ext[i]; i++) {
        out_name[out_i++] = clean_ext[i];
    }
    out_name[out_i] = '\0';
}

static void process_app_data(const unsigned char *buf, size_t len) {
    size_t i = 0;
    while (i < len) {
        if (!headers_parsed) {
            /* Mathematically perfect state machine to find \r\n\r\n */
            while (i < len && !headers_parsed) {
                char c = buf[i++];
                if (c == '\r') {
                    if (header_state == 0 || header_state == 1) {
                        header_state = 1;
                    } else if (header_state == 2) {
                        header_state = 3;
                    } else if (header_state == 3) {
                        header_state = 1;
                    }
                } else if (c == '\n') {
                    if (header_state == 1) {
                        header_state = 2;
                    } else if (header_state == 3) {
                        header_state = 4; /* Found \r\n\r\n */
                    } else {
                        header_state = 0;
                    }
                } else {
                    header_state = 0;
                }

                if (header_state == 4) {
                    headers_parsed = 1;
                    kputs("HTTP headers received.\n");
                    
                    char upper_name[13];
                    get_83_filename(fetch_path, upper_name);
                    
                    FRESULT fr = f_open(&dl_file, upper_name, FA_WRITE | FA_CREATE_ALWAYS);
                    if (fr == FR_OK) {
                        file_is_open = 1;
                        kprintf("Saving to %s...\n", upper_name);
                    } else {
                        kprintf("Failed to open file %s (%d)\n", upper_name, fr);
                    }
                }
            }
        } else {
            if (file_is_open) {
                UINT bw;
                FRESULT fr = f_write(&dl_file, buf + i, len - i, &bw);
                if (fr == FR_OK) {
                    bytes_downloaded += bw;
                }
            }
            i = len;
        }
    }
}

static void pump_ssl(struct tcp_pcb *tpcb) {
    if (!fetch_is_https) return;
    
    unsigned st = br_ssl_engine_current_state(&sc.eng);
    
    if (st & BR_SSL_CLOSED) {
        int err = br_ssl_engine_last_error(&sc.eng);
        if (err != BR_ERR_OK) {
            kprintf("\nTLS connection closed with error: %d\n", err);
        } else {
            kputs("\nTLS connection closed cleanly.\n");
        }
        if (file_is_open) {
            f_close(&dl_file);
            file_is_open = 0;
            kputs("Download saved.\n");
        }
        tcp_close(tpcb);
        fetch_done = 1;
        return;
    }
    
    size_t len;
    unsigned char *buf;
    while ((buf = br_ssl_engine_sendrec_buf(&sc.eng, &len)) != NULL && len > 0) {
        err_t err = tcp_write(tpcb, buf, len, TCP_WRITE_FLAG_COPY);
        if (err != ERR_OK) {
            kprintf("[tcp_write error: %d]\n", err);
            break;
        }
        br_ssl_engine_sendrec_ack(&sc.eng, len);
        tcp_output(tpcb);
    }
    while ((buf = br_ssl_engine_recvapp_buf(&sc.eng, &len)) != NULL && len > 0) {
        process_app_data(buf, len);
        br_ssl_engine_recvapp_ack(&sc.eng, len);
    }
    if (!req_sent && (br_ssl_engine_current_state(&sc.eng) & BR_SSL_SENDAPP)) {
        req_sent = 1;
        kputs("TLS Handshake done! Sending GET request...\n");
        buf = br_ssl_engine_sendapp_buf(&sc.eng, &len);
        if (buf && len > 0) {
            char req[512];
            req[0] = '\0';
            strcat(req, "GET /");
            strcat(req, fetch_path);
            strcat(req, " HTTP/1.0\r\nHost: ");
            strcat(req, fetch_host);
            strcat(req, "\r\nConnection: close\r\n\r\n");
            size_t rlen = strlen(req);
            if (rlen > len) rlen = len;
            memcpy(buf, req, rlen);
            br_ssl_engine_sendapp_ack(&sc.eng, rlen);
            br_ssl_engine_flush(&sc.eng, 0);
            pump_ssl(tpcb);
        }
    }
}

static err_t fetch_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    (void)arg;
    (void)err;
    if (p == NULL) {
        kputs("\n[Connection closed by server]\n");
        if (file_is_open) {
            f_close(&dl_file);
            file_is_open = 0;
            kputs("Download saved.\n");
        }
        tcp_close(tpcb);
        fetch_done = 1;
        return ERR_OK;
    }
    if (fetch_is_https) {
        kprintf("[fetch_recv: got packet, len=%u]\n", p->tot_len);
        size_t offset = 0;
        while (offset < p->tot_len) {
            /* Pump SSL *before* checking available receive buffer size to empty pending app data */
            pump_ssl(tpcb);
            if (fetch_done) break;
            
            size_t elen;
            unsigned char *ebuf = br_ssl_engine_recvrec_buf(&sc.eng, &elen);
            if (ebuf == NULL || elen == 0) break;
            
            size_t chunk = p->tot_len - offset;
            if (chunk > elen) chunk = elen;
            pbuf_copy_partial(p, ebuf, chunk, offset);
            br_ssl_engine_recvrec_ack(&sc.eng, chunk);
            offset += chunk;
        }
        if (!fetch_done) {
            pump_ssl(tpcb);
        }
    } else {
        for (struct pbuf *q = p; q != NULL; q = q->next) {
            process_app_data(q->payload, q->len);
        }
    }
    if (!fetch_done) {
        tcp_recved(tpcb, p->tot_len);
        tcp_output(tpcb);
    }
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
    kputs("TCP Connected!\n");
    if (fetch_is_https) {
        br_ssl_engine_set_buffer(&sc.eng, iobuf, sizeof iobuf, 1);
        br_ssl_client_reset(&sc, fetch_host, 0);
        pump_ssl(tpcb);
    } else {
        req_sent = 1;
        const char *req_a = "GET /";
        const char *req_b = " HTTP/1.0\r\nHost: ";
        const char *req_c = "\r\nConnection: close\r\n\r\n";
        tcp_write(tpcb, req_a, strlen(req_a), TCP_WRITE_FLAG_COPY);
        tcp_write(tpcb, fetch_path, strlen(fetch_path), TCP_WRITE_FLAG_COPY);
        tcp_write(tpcb, req_b, strlen(req_b), TCP_WRITE_FLAG_COPY);
        tcp_write(tpcb, fetch_host, strlen(fetch_host), TCP_WRITE_FLAG_COPY);
        tcp_write(tpcb, req_c, strlen(req_c), TCP_WRITE_FLAG_COPY);
        tcp_output(tpcb);
    }
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
    headers_parsed = 0;
    req_sent = 0;
    file_is_open = 0;
    header_state = 0;
    bytes_downloaded = 0;
    last_st = 0xFFFF;
    
    if (fetch_is_https) {
        // Init BearSSL
        br_ssl_client_zero(&sc);
        br_ssl_client_init_full(&sc, &xc, NULL, 0);
        
        // Setup wrapper
        nv_ctx.vtable = &nv_vtable;
        nv_ctx.minimal = &xc;
        br_ssl_engine_set_x509(&sc.eng, &nv_ctx.vtable);
        
        br_ssl_engine_inject_entropy(&sc.eng, (const void*)&tick_count, sizeof(tick_count));
    }
    
    kputs("Starting TCP connection...\n");
    err_t err = tcp_connect(pcb, ipaddr, fetch_is_https ? 443 : 80, fetch_connected);
    if (err != ERR_OK) {
        kprintf("tcp_connect failed: %d\n", err);
        tcp_close(pcb);
        return;
    }

    unsigned int start = tick_count;
    kputs("Fetching data...  ");
    unsigned int last_print = start;
    unsigned int last_bytes = 0;
    while (!fetch_done && (tick_count - start < 15000)) {
        net_poll();
        if (bytes_downloaded != last_bytes) {
            start = tick_count; /* Reset idle timeout whenever bytes are written */
            last_bytes = bytes_downloaded;
        }
        if (tick_count - last_print >= 250) {
            kprintf("\rDownloading: %u KB... ", bytes_downloaded / 1024);
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
    if (ipaddr == NULL) dns_failed = 1;
    else { dns_resolved_ip = *ipaddr; dns_resolved = 1; }
}

void cmd_fetch(int argc, char *argv[]) {
    if (argc < 2) {
        kputs("Usage: fetch <host_or_url>\n");
        return;
    }
    char *host = argv[1];
    fetch_is_https = 0;
    if (strncmp(host, "http://", 7) == 0) host += 7;
    else if (strncmp(host, "https://", 8) == 0) {
        host += 8;
        fetch_is_https = 1;
    }
    char *path = strchr(host, '/');
    if (path != NULL) {
        *path++ = '\0';
        fetch_path = path;
    } else fetch_path = "";
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
        while (!dns_resolved && !dns_failed && (tick_count - start < 5000)) {
            net_poll();
            if (tick_count - last_print >= 250) {
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
        if (dns_resolved) do_fetch(&dns_resolved_ip);
        else kputs("DNS resolution failed or timed out.\n");
    } else kprintf("DNS request failed immediately: %d\n", err);
}

void fetch_init(void) {
    extern int command_register(const char *name, const char *desc, void (*func)(int, char**));
    command_register("fetch", "Fetch from HTTP/HTTPS server", cmd_fetch);
}
