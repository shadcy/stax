#include "wm.h"
#include "font8x16.h"
#include "console.h"
#include "string.h"
#include "keyboard.h"
#include "framebuffer.h"
#include "browser_html.h"
#include "../ui/wm_internal.h"
#include <stdint.h>

#include "lwip/tcp.h"
#include "lwip/dns.h"
#include "lwip/ip_addr.h"
#include "lwip/err.h"

extern volatile unsigned int tick_count;
extern int net_poll(void);

#define BROWSER_REQ_SIZE 1024
#define BROWSER_DNS_TIMEOUT_MS 10000
#define BROWSER_CONN_TIMEOUT_MS 20000
#define BROWSER_IDLE_TIMEOUT_MS 15000
#define BROWSER_MAX_REDIRECTS 6
#define URL_INPUT_MAX 256
#define HDR_LINE_MAX 384
#define MAX_LINKS_HIT 64
#define BROWSER_DEBUG_RAW_CAP (2 * 1024)

#define NAV_ATTEMPT_DIRECT 0
#define NAV_ATTEMPT_STATIC 1
#define NAV_ATTEMPT_PROXY  2

#ifndef BROWSER_HTTP_PROXY
#define BROWSER_HTTP_PROXY "10.201.23.4"
#endif
#ifndef BROWSER_HTTP_PROXY_PORT
#define BROWSER_HTTP_PROXY_PORT 80
#endif

enum {
    HTTP_STATUS, HTTP_HDR, HTTP_BODY, HTTP_CHUNK_SIZE, HTTP_CHUNK_EXT,
    HTTP_CHUNK_DATA, HTTP_CHUNK_CRLF, HTTP_CHUNK_TRAILER, HTTP_DONE
};

typedef struct {
    int x, y, w, h;
    uint16_t url_id;
} link_rect_t;

static link_rect_t active_links[MAX_LINKS_HIT];
static int active_link_count;

static int scroll_y;
static char url_input[URL_INPUT_MAX] = "about:start";
static int url_pos = 11;
static int url_focused = 1;
static int max_scroll;
static int win_w = 640;
static int win_h = 480;

static struct tcp_pcb *browser_pcb;

static char nav_host[128];
static char nav_path[192];
static u16_t nav_port = 80;
static int nav_use_proxy;
static int nav_attempt;
static unsigned int nav_gen;

static unsigned int state_deadline;
static unsigned int last_rx_tick;

static enum {
    STATE_IDLE, STATE_RESOLVING, STATE_CONNECTING, STATE_REQUESTING,
    STATE_DOWNLOADING, STATE_FINISHED, STATE_ERROR
} browser_state = STATE_IDLE;

static char status_msg[128] = "Idle. Press Enter to go.";

static int http_phase;
static char hdr_line[HDR_LINE_MAX];
static int hdr_len;
static int http_status;
static int is_chunked;
static int gzipped;
static int body_len_limit = -1;
static int body_got;
static int chunk_left;
static int chunk_size_acc;
static char location_hdr[URL_INPUT_MAX];
static int redirect_hops;
static uint32_t bytes_rx;
static int trailer_nonempty;

/* Keep a printable copy of the wire response for the in-browser diagnostic
 * view. This is deliberately independent of the HTML tokenizer. */
static char debug_raw[BROWSER_DEBUG_RAW_CAP];
static uint32_t debug_raw_len;
static int debug_raw_truncated;
static int browser_debug_mode = 0;

static int clip_x0, clip_y0, clip_x1, clip_y1;

static void navigate(void);
static void browser_start_connect(const ip_addr_t *ipaddr);
static void browser_advance_fallback(void);

static int str_istart(const char *s, const char *pfx) {
    while (*pfx) {
        char ca = *s, cb = *pfx;
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
        if (!ca || ca != cb) return 0;
        s++; pfx++;
    }
    return 1;
}

static int host_equals_i(const char *a, const char *b) {
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
        if (ca != cb) return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

static void copy_str(char *dst, const char *src, int max) {
    int i = 0;
    if (max <= 0) return;
    while (src && src[i] && i < max - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int browser_lookup_static(const char *host, ip_addr_t *out) {
    ip4_addr_t ip4;
    if (host_equals_i(host, "httpforever.com")) {
        IP4_ADDR(&ip4, 104, 21, 4, 210);
    } else if (host_equals_i(host, "example.com")) {
        IP4_ADDR(&ip4, 93, 184, 216, 34);
    } else if (host_equals_i(host, "neverssl.com")) {
        IP4_ADDR(&ip4, 34, 223, 155, 91);
    } else if (host_equals_i(host, "wiby.me") || host_equals_i(host, "www.wiby.me")) {
        IP4_ADDR(&ip4, 172, 93, 49, 252);
    } else if (host_equals_i(host, "68k.news") || host_equals_i(host, "www.68k.news")) {
        IP4_ADDR(&ip4, 134, 209, 213, 152);
    } else if (host_equals_i(host, "www.bing.com") || host_equals_i(host, "bing.com")) {
        IP4_ADDR(&ip4, 23, 212, 254, 96);
    } else {
        return 0;
    }
    ip_addr_copy_from_ip4(*out, ip4);
    return 1;
}

static void set_status(const char *msg) {
    copy_str(status_msg, msg, (int)sizeof(status_msg));
}

static void page_reset(void) {
    html_doc_reset();
    html_tokenizer_reset();
    html_set_base(nav_host, nav_path, nav_port);
    http_phase = HTTP_STATUS;
    hdr_len = 0;
    http_status = 0;
    is_chunked = 0;
    gzipped = 0;
    body_len_limit = -1;
    body_got = 0;
    chunk_left = 0;
    chunk_size_acc = 0;
    location_hdr[0] = '\0';
    bytes_rx = 0;
    trailer_nonempty = 0;
    debug_raw_len = 0;
    debug_raw_truncated = 0;
    scroll_y = 0;
}

static void browser_log(const char *msg) {
    kprintf("[BROWSER] %s\n", msg);
    set_status(msg);
}

static const char *lwip_err_str(err_t err) {
    switch (err) {
        case ERR_OK: return "ok";
        case ERR_MEM: return "out of memory";
        case ERR_BUF: return "buffer error";
        case ERR_TIMEOUT: return "timeout";
        case ERR_RTE: return "no route";
        case ERR_INPROGRESS: return "in progress";
        case ERR_VAL: return "bad value";
        case ERR_WOULDBLOCK: return "would block";
        case ERR_USE: return "address in use";
        case ERR_ALREADY: return "already connecting";
        case ERR_ISCONN: return "already connected";
        case ERR_CONN: return "not connected";
        case ERR_IF: return "netif error";
        case ERR_ABRT: return "aborted";
        case ERR_RST: return "connection reset";
        case ERR_CLSD: return "connection closed";
        case ERR_ARG: return "bad argument";
        default: return "unknown";
    }
}

static void browser_close_pcb(int abort) {
    if (!browser_pcb) return;
    kprintf("[BROWSER] Closing PCB: abort=%d state=%d rx=%u phase=%d body=%d/%d\n",
            abort, browser_state, bytes_rx, http_phase, body_got, body_len_limit);
    tcp_arg(browser_pcb, NULL);
    tcp_recv(browser_pcb, NULL);
    tcp_err(browser_pcb, NULL);
    tcp_sent(browser_pcb, NULL);
    tcp_poll(browser_pcb, NULL, 0);
    if (abort) {
        tcp_abort(browser_pcb);
    } else {
        err_t e = tcp_close(browser_pcb);
        if (e != ERR_OK) {
            kprintf("[BROWSER] tcp_close failed: %d, aborting.\n", e);
            tcp_abort(browser_pcb);
        }
    }
    browser_pcb = NULL;
}

#include "font.h"

static void draw_char_8x16(int x, int y, char c, uint16_t color, uint16_t bg, uint16_t *vram, int stride) {
    (void)bg; (void)vram; (void)stride;
    font_draw_char_clipped(x, y, c, color, FONT_STYLE_REGULAR, clip_x0, clip_y0, clip_x1, clip_y1);
}

static void draw_string(int x, int y, const char *str, uint16_t color, uint16_t bg, uint16_t *vram, int stride) {
    (void)bg; (void)vram; (void)stride;
    font_draw_text_clipped(x, y, str, color, FONT_STYLE_REGULAR, clip_x0, clip_y0, clip_x1, clip_y1);
}

static int parse_url(void) {
    const char *p = url_input;
    int has_space = 0;
    int has_dot = 0;
    nav_port = 80;
    nav_host[0] = '\0';
    nav_path[0] = '/';
    nav_path[1] = '\0';

    if (str_istart(p, "https://")) {
        browser_log("HTTPS not supported. Use http:// or bare hostname.");
        return -1;
    }
    
    for (int i = 0; p[i] && p[i] != '/' && p[i] != ':' && p[i] != '?'; i++) {
        if (p[i] == ' ') has_space = 1;
        if (p[i] == '.') has_dot = 1;
    }

    if (!str_istart(p, "http://") && (has_space || !has_dot)) {
        if (!host_equals_i(p, "localhost")) {
            char search_url[URL_INPUT_MAX] = "http://wiby.me/?q=";
            int j = (int)strlen(search_url);
            for (int i = 0; p[i] && j < URL_INPUT_MAX - 1; i++) {
                if (p[i] == ' ') search_url[j++] = '+';
                else search_url[j++] = p[i];
            }
            search_url[j] = '\0';
            copy_str(url_input, search_url, URL_INPUT_MAX);
            url_pos = (int)strlen(url_input);
            p = url_input;
        }
    }

    if (str_istart(p, "http://")) p += 7;

    int hi = 0;
    while (*p && *p != '/' && *p != ':' && *p != '?' && hi < (int)sizeof(nav_host) - 1)
        nav_host[hi++] = *p++;
    nav_host[hi] = '\0';
    
    if (str_istart(nav_host, "google.com") || str_istart(nav_host, "www.google.com")) {
        copy_str(nav_host, "wiby.me", sizeof(nav_host));
        copy_str(url_input, "http://wiby.me/", URL_INPUT_MAX);
        url_pos = (int)strlen(url_input);
    }

    if (hi == 0) {
        browser_log("Empty hostname.");
        return -1;
    }

    if (*p == ':') {
        unsigned int port = 0;
        p++;
        while (*p >= '0' && *p <= '9') {
            port = port * 10u + (unsigned int)(*p - '0');
            if (port > 65535u) break;
            p++;
        }
        if (port == 0 || port > 65535u) {
            browser_log("Invalid port.");
            return -1;
        }
        nav_port = (u16_t)port;
    }

    if (*p == '/' || *p == '?' || *p == '\0') {
        int pi = 0;
        if (*p != '/' && *p != '\0') nav_path[pi++] = '/';
        while (*p && pi < (int)sizeof(nav_path) - 1) nav_path[pi++] = *p++;
        nav_path[pi] = '\0';
        if (nav_path[0] == '\0') {
            nav_path[0] = '/';
            nav_path[1] = '\0';
        }
    }
    return 0;
}

static int build_http_request(char *req, int req_max) {
    int len = 0;
    const char *get = "GET ";
    while (*get && len < req_max - 1) req[len++] = *get++;

    if (nav_use_proxy) {
        const char *scheme = "http://";
        while (*scheme && len < req_max - 1) req[len++] = *scheme++;
        for (int i = 0; nav_host[i] && len < req_max - 1; i++) req[len++] = nav_host[i];
        if (nav_port != 80) {
            unsigned int port = nav_port;
            char tmp[6];
            int ti = 0;
            if (len < req_max - 1) req[len++] = ':';
            do { tmp[ti++] = (char)('0' + (port % 10)); port /= 10; } while (port && ti < 6);
            while (ti > 0 && len < req_max - 1) req[len++] = tmp[--ti];
        }
        for (int i = 0; nav_path[i] && len < req_max - 1; i++) req[len++] = nav_path[i];
    } else {
        for (int i = 0; nav_path[i] && len < req_max - 1; i++) req[len++] = nav_path[i];
    }

    {
        const char *mid = " HTTP/1.1\r\nHost: ";
        while (*mid && len < req_max - 1) req[len++] = *mid++;
    }
    for (int i = 0; nav_host[i] && len < req_max - 1; i++) req[len++] = nav_host[i];
    if (nav_port != 80 && len < req_max - 8) {
        unsigned int port = nav_port;
        char tmp[6];
        int ti = 0;
        req[len++] = ':';
        do { tmp[ti++] = (char)('0' + (port % 10)); port /= 10; } while (port && ti < 6);
        while (ti > 0) req[len++] = tmp[--ti];
    }

    {
        const char *tail =
            "\r\nUser-Agent: curl/8.5.0\r\n"
            "Accept: */*\r\n"
            "Accept-Encoding: identity\r\n"
            "Connection: close\r\n\r\n";
        while (*tail && len < req_max - 1) req[len++] = *tail++;
    }
    req[len] = '\0';
    return len;
}

static int parse_int_dec(const char *s) {
    int n = 0;
    while (*s >= '0' && *s <= '9') {
        n = n * 10 + (*s - '0');
        s++;
    }
    return n;
}

static int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void handle_header_line(char *line) {
    char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (!http_status && str_istart(p, "http/")) {
        while (*p && *p != ' ') p++;
        while (*p == ' ') p++;
        http_status = parse_int_dec(p);
        return;
    }
    if (str_istart(p, "content-length:")) {
        p += 15;
        while (*p == ' ') p++;
        body_len_limit = parse_int_dec(p);
    } else if (str_istart(p, "transfer-encoding:")) {
        p += 18;
        while (*p == ' ') p++;
        if (str_istart(p, "chunked")) is_chunked = 1;
    } else if (str_istart(p, "content-encoding:")) {
        p += 17;
        while (*p == ' ') p++;
        if (str_istart(p, "gzip") || str_istart(p, "deflate") || str_istart(p, "br"))
            gzipped = 1;
    } else if (str_istart(p, "location:")) {
        p += 9;
        while (*p == ' ') p++;
        copy_str(location_hdr, p, URL_INPUT_MAX);
    }
}

static void headers_done(void) {
    if (gzipped) {
        html_emit_text("This page was sent compressed. The browser requests identity encoding only.\n");
        http_phase = HTTP_DONE;
        return;
    }
    if (http_status >= 300 && http_status < 400 && location_hdr[0]) {
        http_phase = HTTP_DONE;
        return;
    }
    if (is_chunked) {
        http_phase = HTTP_CHUNK_SIZE;
        chunk_size_acc = 0;
    } else {
        http_phase = HTTP_BODY;
    }
}

static void finish_status_line(void) {
    char buf[96];
    int n = 0;
    const char *pre = "Loaded ";
    unsigned int v;
    char tmp[12];
    int ti;
    while (*pre && n < 95) buf[n++] = *pre++;
    v = bytes_rx; ti = 0;
    if (!v) tmp[ti++] = '0';
    while (v && ti < 12) { tmp[ti++] = (char)('0' + (v % 10)); v /= 10; }
    while (ti > 0 && n < 95) buf[n++] = tmp[--ti];
    {
        const char *m = " B / ";
        while (*m && n < 95) buf[n++] = *m++;
    }
    v = html_text_len(); ti = 0;
    if (!v) tmp[ti++] = '0';
    while (v && ti < 12) { tmp[ti++] = (char)('0' + (v % 10)); v /= 10; }
    while (ti > 0 && n < 95) buf[n++] = tmp[--ti];
    {
        const char *m = " chars";
        while (*m && n < 95) buf[n++] = *m++;
    }
    if (html_truncated()) {
        const char *t = " (truncated)";
        while (*t && n < 95) buf[n++] = *t++;
    }
    buf[n] = '\0';
    set_status(buf);
}

static int try_redirect(void) {
    char resolved[URL_INPUT_MAX];
    if (http_status < 300 || http_status >= 400 || !location_hdr[0]) return 0;
    if (redirect_hops >= BROWSER_MAX_REDIRECTS) {
        html_emit_text("Too many redirects.\n");
        return 0;
    }
    if (str_istart(location_hdr, "https://")) {
        html_emit_text("Redirected to HTTPS, which is not supported.\n");
        return 0;
    }
    redirect_hops++;
    html_set_base(nav_host, nav_path, nav_port);
    html_resolve(location_hdr, resolved, URL_INPUT_MAX);
    if (!resolved[0]) copy_str(resolved, location_hdr, URL_INPUT_MAX);
    kprintf("[BROWSER] Redirect %d -> %s\n", redirect_hops, resolved);
    copy_str(url_input, resolved, URL_INPUT_MAX);
    url_pos = (int)strlen(url_input);
    navigate();
    return 1;
}

static void body_byte(unsigned char b) {
    body_got++;
    if (!gzipped) html_feed(b);
    if (body_len_limit >= 0 && body_got >= body_len_limit && !is_chunked)
        http_phase = HTTP_DONE;
}

static void debug_capture_byte(unsigned char b) {
    char c;
    if (debug_raw_len >= BROWSER_DEBUG_RAW_CAP - 1) {
        debug_raw_truncated = 1;
        return;
    }
    /* Preserve line structure, but never put an unrenderable control byte
     * into the framebuffer path. */
    if (b == '\r') return;
    c = (b == '\n' || b == '\t' || (b >= 32 && b <= 126)) ? (char)b : '.';
    debug_raw[debug_raw_len++] = c;
    debug_raw[debug_raw_len] = '\0';
}

static void http_byte(unsigned char b) {
    char c = (char)b;
    switch (http_phase) {
    case HTTP_DONE:
        return;
    case HTTP_STATUS:
    case HTTP_HDR:
        if (c == '\r') return;
        if (c == '\n') {
            if (hdr_len == 0) headers_done();
            else {
                hdr_line[hdr_len] = '\0';
                handle_header_line(hdr_line);
                hdr_len = 0;
            }
            return;
        }
        if (hdr_len < HDR_LINE_MAX - 1) hdr_line[hdr_len++] = c;
        return;
    case HTTP_BODY:
        body_byte(b);
        return;
    case HTTP_CHUNK_SIZE:
        if (c == '\n') {
            chunk_left = chunk_size_acc;
            chunk_size_acc = 0;
            http_phase = (chunk_left == 0) ? HTTP_CHUNK_TRAILER : HTTP_CHUNK_DATA;
            trailer_nonempty = 0;
            return;
        }
        if (c == '\r') return;
        {
            int v = hex_digit(c);
            if (v >= 0) chunk_size_acc = (chunk_size_acc << 4) | v;
            else http_phase = HTTP_CHUNK_EXT;
        }
        return;
    case HTTP_CHUNK_EXT:
        if (c == '\n') {
            chunk_left = chunk_size_acc;
            chunk_size_acc = 0;
            http_phase = (chunk_left == 0) ? HTTP_CHUNK_TRAILER : HTTP_CHUNK_DATA;
            trailer_nonempty = 0;
        }
        return;
    case HTTP_CHUNK_DATA:
        body_byte(b);
        chunk_left--;
        if (chunk_left <= 0) http_phase = HTTP_CHUNK_CRLF;
        return;
    case HTTP_CHUNK_CRLF:
        if (c == '\n') {
            http_phase = HTTP_CHUNK_SIZE;
            chunk_size_acc = 0;
        }
        return;
    case HTTP_CHUNK_TRAILER:
        if (c == '\n') {
            if (!trailer_nonempty) http_phase = HTTP_DONE;
            trailer_nonempty = 0;
            return;
        }
        if (c != '\r') trailer_nonempty = 1;
        return;
    }
}

static err_t browser_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    u16_t received;
    (void)arg;
    if (err != ERR_OK) {
        if (p) pbuf_free(p);
        return err;
    }
    if (p == NULL) {
        kprintf("[BROWSER] recv p==NULL\n");
        if (try_redirect()) return ERR_ABRT;
        browser_state = STATE_FINISHED;
        if (html_text_len() == 0)
            html_emit_text(bytes_rx ? "Response contains no visible text." : "Empty response.");
        finish_status_line();
        browser_close_pcb(0);
        return ERR_OK;
    }

    last_rx_tick = tick_count;
    state_deadline = tick_count + BROWSER_IDLE_TIMEOUT_MS;

    /* Keep the byte count separate from the HTML output count: headers,
     * markup and a response with no visible text are all valid received data.
     * Capture tot_len before handing the pbuf back to lwIP. */
    received = p->tot_len;
    bytes_rx += received;

    {
        struct pbuf *q;
        for (q = p; q != NULL; q = q->next) {
            unsigned char *data = (unsigned char *)q->payload;
            int i;
            if (bytes_rx == received && q->len >= 4) {
                kprintf("[BROWSER] First chunk rx: %d bytes. Start: '%c%c%c%c'\n", q->len, data[0], data[1], data[2], data[3]);
            }
            kprintf("[BROWSER] pbuf len: %d\n", q->len);
            for (i = 0; i < q->len; i++) {
                debug_capture_byte(data[i]);
                http_byte(data[i]);
            }
        }
    }

    tcp_recved(tpcb, received);
    pbuf_free(p);

    if (http_phase == HTTP_DONE) {
        /*
         * Do not tcp_close() from the data callback. tcp_close marks the
         * receive side closed; if the peer has already queued another TCP
         * segment, lwIP responds to that segment with RST.  That was the
         * source of the immediate reset after the first response packet.
         *
         * We request "Connection: close", so the peer will send FIN shortly.
         * browser_recv() handles that p == NULL notification and closes the
         * PCB cleanly then.  Continue acknowledging any trailing bytes in the
         * meantime (HTTP_DONE makes http_byte() ignore them).
         */
        browser_state = STATE_FINISHED;
        finish_status_line();
    }
    return ERR_OK;
}

static err_t browser_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    (void)arg;
    (void)len;
    return tcp_output(tpcb);
}

static err_t browser_connected(void *arg, struct tcp_pcb *tpcb, err_t err) {
    char req[BROWSER_REQ_SIZE];
    int len;
    (void)arg;
    if (err != ERR_OK) {
        char msg[96];
        const char *es = lwip_err_str(err);
        int i = 0;
        const char *pre = "Connect failed: ";
        while (*pre && i < 95) msg[i++] = *pre++;
        while (*es && i < 95) msg[i++] = *es++;
        msg[i] = '\0';
        browser_state = STATE_ERROR;
        browser_log(msg);
        browser_pcb = NULL;
        browser_advance_fallback();
        return err;
    }

    browser_state = STATE_REQUESTING;
    kprintf("[BROWSER] TCP connected to %s%s\n", nav_use_proxy ? "proxy for " : "", nav_host);
    set_status("Connected. Sending request...");

    len = build_http_request(req, BROWSER_REQ_SIZE);
    if (len <= 0 || len >= BROWSER_REQ_SIZE) {
        browser_state = STATE_ERROR;
        browser_log("Failed to build HTTP request.");
        browser_close_pcb(1);
        return ERR_VAL;
    }

    tcp_recv(tpcb, browser_recv);
    tcp_sent(tpcb, browser_sent);

    {
        err_t werr = tcp_write(tpcb, req, (u16_t)len, TCP_WRITE_FLAG_COPY);
        if (werr != ERR_OK) {
            char msg[96];
            int i = 0;
            const char *pre = "tcp_write failed: ";
            const char *es = lwip_err_str(werr);
            while (*pre && i < 95) msg[i++] = *pre++;
            while (*es && i < 95) msg[i++] = *es++;
            msg[i] = '\0';
            browser_state = STATE_ERROR;
            browser_log(msg);
            browser_close_pcb(1);
            return werr;
        }
    }
    tcp_output(tpcb);
    browser_state = STATE_DOWNLOADING;
    set_status("Downloading...");
    last_rx_tick = tick_count;
    state_deadline = tick_count + BROWSER_IDLE_TIMEOUT_MS;
    return ERR_OK;
}

static void browser_err(void *arg, err_t err) {
    char msg[96];
    int i = 0;
    const char *pre = "TCP error: ";
    const char *es = lwip_err_str(err);
    (void)arg;
    browser_pcb = NULL;
    while (*pre && i < 95) msg[i++] = *pre++;
    while (*es && i < 95) msg[i++] = *es++;
    msg[i] = '\0';

    /* A page can legitimately contain only headers, markup, or content the
     * compact renderer does not expose as text.  bytes_rx is therefore the
     * authoritative indication that an HTTP response reached this browser;
     * using html_text_len() here discarded that response, reset its counters
     * during fallback, and made the later timeout report rx=0. */
    if (bytes_rx > 0 && http_phase != HTTP_STATUS && http_phase != HTTP_HDR) {
        if (try_redirect()) return;
        browser_state = STATE_FINISHED;
        finish_status_line();
        kprintf("[BROWSER] %s (ignored, page received)\n", msg);
        return;
    }
    browser_state = STATE_ERROR;
    browser_log(msg);
    browser_advance_fallback();
}

static void browser_dns_found(const char *name, const ip_addr_t *ipaddr, void *arg) {
    unsigned int gen = (unsigned int)(uintptr_t)arg;
    (void)name;
    if (gen != nav_gen || browser_state != STATE_RESOLVING) return;
    if (ipaddr == NULL) {
        browser_state = STATE_ERROR;
        browser_log("DNS failed. Host not found.");
        browser_advance_fallback();
        return;
    }
    kprintf("[BROWSER] DNS OK -> %s\n", ipaddr_ntoa(ipaddr));
    browser_start_connect(ipaddr);
}

static void browser_start_connect(const ip_addr_t *ipaddr) {
    u16_t port;
    err_t err;
    browser_close_pcb(1);
    browser_pcb = tcp_new();
    if (!browser_pcb) {
        browser_state = STATE_ERROR;
        browser_log("Out of memory (TCP PCB).");
        return;
    }
    browser_state = STATE_CONNECTING;
    set_status(nav_use_proxy ? "Connecting via proxy..." : "Connecting...");
    state_deadline = tick_count + BROWSER_CONN_TIMEOUT_MS;
    tcp_arg(browser_pcb, NULL);
    tcp_err(browser_pcb, browser_err);
    tcp_recv(browser_pcb, browser_recv);
    port = nav_use_proxy ? (u16_t)BROWSER_HTTP_PROXY_PORT : nav_port;
    err = tcp_connect(browser_pcb, ipaddr, port, browser_connected);
    if (err != ERR_OK) {
        char msg[96];
        int i = 0;
        const char *pre = "tcp_connect failed: ";
        const char *es = lwip_err_str(err);
        while (*pre && i < 95) msg[i++] = *pre++;
        while (*es && i < 95) msg[i++] = *es++;
        msg[i] = '\0';
        browser_state = STATE_ERROR;
        browser_log(msg);
        browser_close_pcb(1);
        browser_advance_fallback();
    }
}

static void browser_resolve_and_connect(const char *hostname) {
    void *gen_arg = (void *)(uintptr_t)nav_gen;
    ip_addr_t addr;
    ip4_addr_t ip4;
    err_t err;
    browser_state = STATE_RESOLVING;
    set_status("Resolving DNS...");
    state_deadline = tick_count + BROWSER_DNS_TIMEOUT_MS;

    if (browser_lookup_static(hostname, &addr)) {
        kprintf("[BROWSER] Static DNS match -> %s\n", ipaddr_ntoa(&addr));
        browser_dns_found(hostname, &addr, gen_arg);
        return;
    }

    if (ip4addr_aton(hostname, &ip4)) {
        ip_addr_copy_from_ip4(addr, ip4);
        browser_dns_found(hostname, &addr, gen_arg);
        return;
    }
    err = dns_gethostbyname(hostname, &addr, browser_dns_found, gen_arg);
    if (err == ERR_OK) browser_dns_found(hostname, &addr, gen_arg);
    else if (err != ERR_INPROGRESS) {
        char msg[96];
        int i = 0;
        const char *pre = "DNS request failed: ";
        const char *es = lwip_err_str(err);
        while (*pre && i < 95) msg[i++] = *pre++;
        while (*es && i < 95) msg[i++] = *es++;
        msg[i] = '\0';
        browser_state = STATE_ERROR;
        browser_log(msg);
        browser_advance_fallback();
    }
}

static void browser_advance_fallback(void) {
    if (nav_host[0] == '\0') return;
    if (nav_attempt == NAV_ATTEMPT_DIRECT) {
        ip_addr_t static_ip;
        if (browser_lookup_static(nav_host, &static_ip)) {
            browser_close_pcb(1);
            nav_gen++;
            nav_attempt = NAV_ATTEMPT_STATIC;
            nav_use_proxy = 0;
            page_reset();
            kprintf("[BROWSER] DNS miss — using static IP for %s\n", nav_host);
            set_status("Using static IP...");
            browser_start_connect(&static_ip);
            return;
        }
        nav_attempt = NAV_ATTEMPT_STATIC;
    }
    if (nav_attempt == NAV_ATTEMPT_STATIC) {
        ip_addr_t proxy_addr;
        ip4_addr_t proxy_ip4;
        if (!ip4addr_aton(BROWSER_HTTP_PROXY, &proxy_ip4)) return;
        ip_addr_copy_from_ip4(proxy_addr, proxy_ip4);
        browser_close_pcb(1);
        nav_gen++;
        nav_attempt = NAV_ATTEMPT_PROXY;
        nav_use_proxy = 1;
        page_reset();
        kprintf("[BROWSER] Retrying via HTTP proxy %s...\n", BROWSER_HTTP_PROXY);
        set_status("Trying HTTP proxy...");
        browser_start_connect(&proxy_addr);
    }
}

#define BROWSER_HISTORY_MAX 8
static char history_urls[BROWSER_HISTORY_MAX][URL_INPUT_MAX];
static int history_count = 0;
static int history_pos = -1;
static int is_navigating_history = 0;

static void history_push(const char *url) {
    if (!url || !url[0]) return;
    if (history_pos >= 0 && history_pos < history_count && strcmp(history_urls[history_pos], url) == 0)
        return;
    history_count = history_pos + 1;
    if (history_count >= BROWSER_HISTORY_MAX) {
        for (int i = 0; i < BROWSER_HISTORY_MAX - 1; i++) {
            copy_str(history_urls[i], history_urls[i + 1], URL_INPUT_MAX);
        }
        history_count = BROWSER_HISTORY_MAX - 1;
    }
    copy_str(history_urls[history_count], url, URL_INPUT_MAX);
    history_pos = history_count;
    history_count++;
}

static void history_back(void) {
    if (history_pos > 0) {
        history_pos--;
        copy_str(url_input, history_urls[history_pos], URL_INPUT_MAX);
        url_pos = (int)strlen(url_input);
        is_navigating_history = 1;
        redirect_hops = 0;
        navigate();
        is_navigating_history = 0;
    }
}

static void history_forward(void) {
    if (history_pos >= 0 && history_pos < history_count - 1) {
        history_pos++;
        copy_str(url_input, history_urls[history_pos], URL_INPUT_MAX);
        url_pos = (int)strlen(url_input);
        is_navigating_history = 1;
        redirect_hops = 0;
        navigate();
        is_navigating_history = 0;
    }
}

static void history_reload(void) {
    redirect_hops = 0;
    navigate();
}

static void history_home(void) {
    copy_str(url_input, "about:start", URL_INPUT_MAX);
    url_pos = (int)strlen(url_input);
    redirect_hops = 0;
    navigate();
}

static const char *start_page_html =
    "<html><head><title>STAX Web Browser</title></head><body>\n"
    "<p><img src=\"BMP/LOGO.BMP\" alt=\"STAX OS\"></p>\n"
    "<h1>STAX Web Browser</h1>\n"
    "<p>Fast, lightweight graphical web browser with inline image rendering for STAX OS.</p>\n"
    "<hr>\n"
    "<h2>Search the Web with Wiby</h2>\n"
    "<p>Wiby is a classic web search engine built for vintage, lightweight, and text browsers.</p>\n"
    "<p>\n"
    " * <a href=\"http://wiby.me/\">Wiby Search Homepage</a>\n"
    " * <a href=\"http://wiby.me/surprise/\">Surprise Me (Random Classic Website)</a>\n"
    " * <a href=\"http://wiby.me/about/\">About Wiby Project</a>\n"
    "</p>\n"
    "<p><i>Quick Search: Type any search query in the URL bar above and press Enter.</i></p>\n"
    "<hr>\n"
    "<h2>Featured Retro Websites</h2>\n"
    "<p>\n"
    " * <a href=\"http://wiby.me/\">Wiby - The Classic Web Search Engine</a>\n"
    " * <a href=\"http://httpforever.com/\">HTTP Forever - Plain HTTP Test Site</a>\n"
    " * <a href=\"http://68k.news/\">68k.news - Minimalist Headline Reader</a>\n"
    " * <a href=\"http://example.com/\">Example Domain - IANA Standard Test</a>\n"
    " * <a href=\"http://neverssl.com/\">NeverSSL - Captive Portal Test</a>\n"
    "</p>\n"
    "<hr>\n"
    "<h2>Credits & Free Software</h2>\n"
    "<p>The <b>Wiby</b> search engine is now free software under <b>GPLv2</b>.</p>\n"
    "<p><a href=\"http://wiby.me/about/guide.html\">Click here to read the install guide and download the source code.</a></p>\n"
    "<p>STAX OS Network Stack powered by <b>lwIP TCP/IP Stack</b> &amp; <b>SMC91C111</b> driver.</p>\n"
    "<hr>\n"
    "<p><i>Toolbar: [&lt;] Back | [&gt;] Next / Fwd | [R] Reload | [H] Home | [L] URL Bar</i></p>\n"
    "<p><i>Keys: Up/Down or W/S: Scroll | Space: Page Down | B: Back | N: Forward / Next</i></p>\n"
    "</body></html>";

static int is_about_url(const char *u) {
    return host_equals_i(u, "about:start") || host_equals_i(u, "about:home") ||
           host_equals_i(u, "about:credits") || host_equals_i(u, "about:blank") ||
           host_equals_i(u, "home") || host_equals_i(u, "start") ||
           host_equals_i(u, "credits") || u[0] == '\0';
}

static void navigate(void) {
    browser_close_pcb(1);
    nav_gen++;
    nav_use_proxy = 0;
    nav_attempt = NAV_ATTEMPT_DIRECT;

    if (!is_navigating_history) {
        history_push(url_input);
    }

    if (is_about_url(url_input)) {
        page_reset();
        copy_str(nav_host, "localhost", sizeof(nav_host));
        copy_str(nav_path, "/about:start", sizeof(nav_path));
        html_set_base(nav_host, nav_path, 80);
        for (int i = 0; start_page_html[i]; i++) {
            debug_capture_byte((unsigned char)start_page_html[i]);
            html_feed((unsigned char)start_page_html[i]);
        }
        browser_state = STATE_FINISHED;
        finish_status_line();
        set_status("STAX Start Page Loaded.");
        return;
    }

    if (parse_url() != 0) {
        browser_state = STATE_ERROR;
        return;
    }
    page_reset();
    kprintf("[BROWSER] Navigate host=%s path=%s port=%u\n", nav_host, nav_path, nav_port);
    browser_resolve_and_connect(nav_host);
}

static uint16_t style_color(uint8_t st) {
    switch (st) {
        case HTML_ST_H1: return rgb565(180, 20, 20);
        case HTML_ST_H2: return rgb565(20, 110, 40);
        case HTML_ST_H3: return rgb565(20, 60, 140);
        case HTML_ST_LINK: return rgb565(20, 40, 200);
        case HTML_ST_PRE: return rgb565(40, 40, 40);
        case HTML_ST_QUOTE: return rgb565(80, 80, 80);
        case HTML_ST_IMG: return rgb565(70, 70, 90);
        default: return COLOR_BLACK;
    }
}

#define BROWSER_SB_WIDTH 14

static void browser_scroll_by(int dy) {
    scroll_y += dy;
    if (scroll_y < 0) scroll_y = 0;
    if (scroll_y > max_scroll) scroll_y = max_scroll;
}

static void browser_draw_scrollbar(int cx, int cy, int cw, int ch, uint16_t *vram, int stride) {
    int content_top = 24;
    int content_bot = ch - 16;
    int sb_x = cx + cw - BROWSER_SB_WIDTH;
    int sb_y = cy + content_top;
    int sb_w = BROWSER_SB_WIDTH;
    int sb_h = content_bot - content_top;
    if (sb_h <= 32 || sb_w <= 0) return;

    /* Background track */
    fb_fillrect(sb_x, sb_y, sb_w, sb_h, rgb565(230, 230, 235));
    fb_drawline(sb_x, sb_y, sb_x, sb_y + sb_h - 1, rgb565(190, 190, 200));

    /* Up button */
    fb_fillrect(sb_x + 1, sb_y + 1, sb_w - 1, 13, rgb565(215, 215, 225));
    fb_drawline(sb_x, sb_y + 14, sb_x + sb_w - 1, sb_y + 14, rgb565(190, 190, 200));
    draw_char_8x16(sb_x + 3, sb_y - 1, '^', COLOR_BLACK, rgb565(215, 215, 225), vram, stride);

    /* Down button */
    int db_y = sb_y + sb_h - 14;
    fb_fillrect(sb_x + 1, db_y + 1, sb_w - 1, 13, rgb565(215, 215, 225));
    fb_drawline(sb_x, db_y, sb_x + sb_w - 1, db_y, rgb565(190, 190, 200));
    draw_char_8x16(sb_x + 3, db_y - 2, 'v', COLOR_BLACK, rgb565(215, 215, 225), vram, stride);

    /* Thumb */
    int track_top = sb_y + 15;
    int track_h = sb_h - 30;
    if (track_h <= 10) return;

    int total_h = html_layout_height();
    int view_h = sb_h - 8;
    int thumb_h = track_h;
    if (total_h > view_h && total_h > 0) {
        thumb_h = (view_h * track_h) / total_h;
        if (thumb_h < 14) thumb_h = 14;
        if (thumb_h > track_h) thumb_h = track_h;
    }

    int thumb_y = track_top;
    if (max_scroll > 0) {
        thumb_y = track_top + (scroll_y * (track_h - thumb_h)) / max_scroll;
        if (thumb_y < track_top) thumb_y = track_top;
        if (thumb_y + thumb_h > track_top + track_h) thumb_y = track_top + track_h - thumb_h;
    }

    fb_fillrect(sb_x + 1, thumb_y, sb_w - 2, thumb_h, rgb565(165, 170, 185));
    fb_drawline(sb_x + 1, thumb_y, sb_x + sb_w - 2, thumb_y, rgb565(200, 205, 220));
    fb_drawline(sb_x + 1, thumb_y + thumb_h - 1, sb_x + sb_w - 2, thumb_y + thumb_h - 1, rgb565(130, 135, 145));

    /* Grip lines */
    if (thumb_h >= 18) {
        int mid_y = thumb_y + thumb_h / 2;
        fb_drawline(sb_x + 3, mid_y - 2, sb_x + sb_w - 4, mid_y - 2, rgb565(130, 135, 145));
        fb_drawline(sb_x + 3, mid_y,     sb_x + sb_w - 4, mid_y,     rgb565(130, 135, 145));
        fb_drawline(sb_x + 3, mid_y + 2, sb_x + sb_w - 4, mid_y + 2, rgb565(130, 135, 145));
    }
}

static void browser_draw(struct window *win, int cx, int cy, int cw, int ch) {
    extern uint32_t fb_width;
    extern uint32_t fb_height;
    uint16_t *vram = fb_get_buffer();
    int content_top = 24;
    int content_bot;
    int i, y0, y1;
    const char *url_show;
    const char *page_title;
    int page_cw;

    if (!vram) return;
    win_w = cw;
    win_h = ch;
    content_bot = ch - 16;
    page_cw = cw - BROWSER_SB_WIDTH;
    if (page_cw < 32) page_cw = 32;

    clip_x0 = cx; clip_y0 = cy;
    clip_x1 = cx + cw; clip_y1 = cy + ch;
    if (clip_x1 > (int)fb_width) clip_x1 = (int)fb_width;
    if (clip_y1 > (int)fb_height) clip_y1 = (int)fb_height;

    /* Top Toolbar Background */
    fb_fillrect(cx, cy, cw, 24, COLOR_GRAY_5);

    /* Back Button (<) */
    int can_back = (history_pos > 0);
    uint16_t back_bg = can_back ? rgb565(240, 240, 245) : rgb565(215, 215, 220);
    uint16_t back_fg = can_back ? COLOR_BLACK : rgb565(140, 140, 150);
    fb_fillrect(cx + 4, cy + 2, 22, 20, back_bg);
    fb_drawline(cx + 4, cy + 2, cx + 25, cy + 2, rgb565(180, 180, 190));
    fb_drawline(cx + 4, cy + 21, cx + 25, cy + 21, rgb565(180, 180, 190));
    fb_drawline(cx + 4, cy + 2, cx + 4, cy + 21, rgb565(180, 180, 190));
    fb_drawline(cx + 25, cy + 2, cx + 25, cy + 21, rgb565(180, 180, 190));
    draw_char_8x16(cx + 11, cy + 4, '<', back_fg, back_bg, vram, (int)fb_width);

    /* Forward / Next Button (>) */
    int can_fwd = (history_pos >= 0 && history_pos < history_count - 1);
    uint16_t fwd_bg = can_fwd ? rgb565(240, 240, 245) : rgb565(215, 215, 220);
    uint16_t fwd_fg = can_fwd ? COLOR_BLACK : rgb565(140, 140, 150);
    fb_fillrect(cx + 28, cy + 2, 22, 20, fwd_bg);
    fb_drawline(cx + 28, cy + 2, cx + 49, cy + 2, rgb565(180, 180, 190));
    fb_drawline(cx + 28, cy + 21, cx + 49, cy + 21, rgb565(180, 180, 190));
    fb_drawline(cx + 28, cy + 2, cx + 28, cy + 21, rgb565(180, 180, 190));
    fb_drawline(cx + 49, cy + 2, cx + 49, cy + 21, rgb565(180, 180, 190));
    draw_char_8x16(cx + 35, cy + 4, '>', fwd_fg, fwd_bg, vram, (int)fb_width);

    /* Reload Button (R) */
    fb_fillrect(cx + 52, cy + 2, 22, 20, rgb565(240, 240, 245));
    fb_drawline(cx + 52, cy + 2, cx + 73, cy + 2, rgb565(180, 180, 190));
    fb_drawline(cx + 52, cy + 21, cx + 73, cy + 21, rgb565(180, 180, 190));
    fb_drawline(cx + 52, cy + 2, cx + 52, cy + 21, rgb565(180, 180, 190));
    fb_drawline(cx + 73, cy + 2, cx + 73, cy + 21, rgb565(180, 180, 190));
    draw_char_8x16(cx + 59, cy + 4, 'R', COLOR_BLACK, rgb565(240, 240, 245), vram, (int)fb_width);

    /* Home Button (H) */
    fb_fillrect(cx + 76, cy + 2, 22, 20, rgb565(240, 240, 245));
    fb_drawline(cx + 76, cy + 2, cx + 97, cy + 2, rgb565(180, 180, 190));
    fb_drawline(cx + 76, cy + 21, cx + 97, cy + 21, rgb565(180, 180, 190));
    fb_drawline(cx + 76, cy + 2, cx + 76, cy + 21, rgb565(180, 180, 190));
    fb_drawline(cx + 97, cy + 2, cx + 97, cy + 21, rgb565(180, 180, 190));
    draw_char_8x16(cx + 83, cy + 4, 'H', COLOR_BLACK, rgb565(240, 240, 245), vram, (int)fb_width);

    /* URL label */
    draw_string(cx + 104, cy + 4, "URL:", COLOR_BLACK, COLOR_GRAY_5, vram, (int)fb_width);

    {
        uint16_t input_bg = url_focused ? COLOR_WHITE : COLOR_GRAY_5;
        int field_x = cx + 140;
        int field_w = cw - 148;
        if (field_w < 16) field_w = 16;
        fb_fillrect(field_x, cy + 2, field_w, 20, input_bg);
        url_show = url_input;
        {
            int vis = field_w / 8 - 1;
            int ulen = (int)strlen(url_input);
            if (vis < 4) vis = 4;
            if (ulen > vis) url_show = url_input + (ulen - vis);
        }
        clip_x0 = field_x;
        clip_x1 = field_x + field_w;
        draw_string(field_x + 4, cy + 4, url_show, COLOR_BLACK, input_bg, vram, (int)fb_width);
        if (url_focused && ((tick_count / 500) % 2 == 0)) {
            int caret = (int)strlen(url_show);
            draw_char_8x16(field_x + 4 + caret * 8, cy + 4, '_', COLOR_BLACK, input_bg, vram, (int)fb_width);
        }
    }

    fb_fillrect(cx, cy + content_top, cw, content_bot - content_top, COLOR_WHITE);
    fb_fillrect(cx, cy + ch - 16, cw, 16, COLOR_GRAY);

    if (browser_debug_mode) {
        const char *text = debug_raw;
        int len = (int)debug_raw_len;
        int tx = cx + 8, ty = cy + content_top;
        clip_x0 = cx; clip_x1 = cx + page_cw;
        clip_y0 = cy + content_top; clip_y1 = cy + content_bot;
        draw_string(cx + 8, cy + content_top, "RAW HTTP RESPONSE (D: rendered view)",
                    COLOR_BLACK, COLOR_WHITE, vram, (int)fb_width);
        ty += 16;
        for (i = 0; i < len; i++) {
            if (ty + 16 >= cy + content_bot) break;
            if (text[i] == '\n') { tx = cx + 8; ty += 16; }
            else if (text[i] >= 32) {
                draw_char_8x16(tx, ty, text[i], COLOR_BLACK, COLOR_WHITE, vram, (int)fb_width);
                tx += 8;
                if (tx + 8 > cx + page_cw) { tx = cx + 8; ty += 16; }
            }
        }
        if (debug_raw_truncated && ty + 16 < cy + content_bot)
            draw_string(cx + 8, ty, "[raw response truncated]", COLOR_BLACK, COLOR_WHITE,
                        vram, (int)fb_width);
        clip_x0 = cx; clip_x1 = cx + cw;
        clip_y0 = cy + ch - 16; clip_y1 = cy + ch;
        draw_string(cx + 4, cy + ch - 16, status_msg, COLOR_WHITE, COLOR_GRAY,
                    vram, (int)fb_width);
    } else {
        clip_x0 = cx; clip_x1 = cx + cw;
        if (clip_x1 > (int)fb_width) clip_x1 = (int)fb_width;
        clip_y0 = cy + ch - 16; clip_y1 = cy + ch;
        draw_string(cx + 4, cy + ch - 16, status_msg, COLOR_WHITE, COLOR_GRAY, vram, (int)fb_width);

        page_title = html_title();
        if (win && page_title && page_title[0])
            copy_str(win->title, page_title, (int)sizeof(win->title));

        html_layout(page_cw);
        max_scroll = html_layout_height() - (content_bot - content_top - 8);
        if (max_scroll < 0) max_scroll = 0;
        if (scroll_y > max_scroll) scroll_y = max_scroll;

        clip_x0 = cx;
        clip_y0 = cy + content_top;
        clip_x1 = cx + page_cw;
        clip_y1 = cy + content_bot;
        if (clip_x1 > (int)fb_width) clip_x1 = (int)fb_width;
        if (clip_y1 > (int)fb_height) clip_y1 = (int)fb_height;

        y0 = scroll_y - 16;
        y1 = scroll_y + (content_bot - content_top);
        active_link_count = 0;

        for (i = 0; i < html_span_count(); i++) {
            const html_span_t *s = html_span_at(i);
            const char *text;
            uint16_t color;
            int sx, sy, j;
            if (!s) break;
            if (s->y + 16 < y0) continue;
            if (s->y > y1) break;
            color = style_color(s->style);
            text = html_text_ptr();
            sx = cx + s->x;
            sy = cy + content_top + s->y - scroll_y;
            for (j = 0; j < s->n; j++)
                draw_char_8x16(sx + j * 8, sy, text[s->off + j], color, COLOR_WHITE, vram, (int)fb_width);
            if (s->style == HTML_ST_LINK && s->url_id) {
                int uw = s->n * 8;
                int uy = sy + 15;
                if (uy >= clip_y0 && uy < clip_y1) {
                    int ux;
                    for (ux = 0; ux < uw; ux++) {
                        int px = sx + ux;
                        if (px >= clip_x0 && px < clip_x1)
                            vram[uy * (int)fb_width + px] = color;
                    }
                }
                if (active_link_count < MAX_LINKS_HIT) {
                    link_rect_t *r = &active_links[active_link_count++];
                    r->x = s->x;
                    r->y = content_top + s->y - scroll_y;
                    r->w = uw;
                    r->h = 16;
                    r->url_id = s->url_id;
                }
            }
        }

        /* Render HTML Images */
        for (i = 0; i < html_image_count(); i++) {
            const html_image_t *img = html_image_at(i);
            if (!img) continue;
            int ix = cx + img->x;
            int iy = cy + content_top + img->y - scroll_y;
            int iw = img->w;
            int ih = img->h;

            if (iy + ih < clip_y0 || iy >= clip_y1) continue;

            if (img->is_loaded && img->pixels) {
                for (int py = 0; py < ih; py++) {
                    int sy = iy + py;
                    if (sy < clip_y0 || sy >= clip_y1) continue;
                    for (int px = 0; px < iw; px++) {
                        int sx = ix + px;
                        if (sx < clip_x0 || sx >= clip_x1) continue;
                        uint16_t col = img->pixels[py * iw + px];
                        if (col == 0xF81F) continue; /* Transparent color key */
                        vram[sy * (int)fb_width + sx] = col;
                    }
                }
                /* Draw subtle image frame border */
                fb_drawline(ix - 1, iy - 1, ix + iw, iy - 1, rgb565(200, 200, 210));
                fb_drawline(ix - 1, iy + ih, ix + iw, iy + ih, rgb565(200, 200, 210));
                fb_drawline(ix - 1, iy - 1, ix - 1, iy + ih, rgb565(200, 200, 210));
                fb_drawline(ix + iw, iy - 1, ix + iw, iy + ih, rgb565(200, 200, 210));
            } else {
                int box_w = (iw < 140) ? 140 : (iw > 280 ? 280 : iw);
                int box_h = 22;
                if (ix + box_w <= clip_x1 && iy + box_h <= clip_y1 && iy >= clip_y0) {
                    fb_fillrect(ix, iy, box_w, box_h, rgb565(245, 245, 250));
                    fb_drawline(ix, iy, ix + box_w, iy, rgb565(190, 190, 200));
                    fb_drawline(ix, iy + box_h - 1, ix + box_w, iy + box_h - 1, rgb565(190, 190, 200));
                    fb_drawline(ix, iy, ix, iy + box_h - 1, rgb565(190, 190, 200));
                    fb_drawline(ix + box_w, iy, ix + box_w, iy + box_h - 1, rgb565(190, 190, 200));
                    draw_char_8x16(ix + 4, iy + 3, '#', rgb565(50, 90, 160), rgb565(245, 245, 250), vram, (int)fb_width);
                    draw_string(ix + 16, iy + 3, img->alt[0] ? img->alt : "Image", rgb565(50, 50, 70), rgb565(245, 245, 250), vram, (int)fb_width);
                }
            }
        }
    }

    /* Draw right-hand scrollbar */
    clip_x0 = cx; clip_y0 = cy;
    clip_x1 = cx + cw; clip_y1 = cy + ch;
    if (clip_x1 > (int)fb_width) clip_x1 = (int)fb_width;
    if (clip_y1 > (int)fb_height) clip_y1 = (int)fb_height;
    browser_draw_scrollbar(cx, cy, cw, ch, vram, (int)fb_width);
}

static void browser_key_event(struct window *win, char c) {
    (void)win;
    int view_h = (win_h - 16 - 24) - 32;
    if (view_h < 32) view_h = 32;

    /* Global scroll keys (always active even if URL bar is focused) */
    if (c == KB_UP) {
        browser_scroll_by(-48);
        return;
    } else if (c == KB_DOWN) {
        browser_scroll_by(48);
        return;
    }

    if (url_focused) {
        if (c == '\b' || c == 127) {
            if (url_pos > 0) {
                url_pos--;
                url_input[url_pos] = '\0';
            }
        } else if (c == '\r' || c == '\n') {
            url_focused = 0;
            redirect_hops = 0;
            navigate();
        } else if (c == 27) { /* Escape un-focuses URL bar */
            url_focused = 0;
        } else if (c >= 32 && c <= 126 && url_pos < URL_INPUT_MAX - 1) {
            url_input[url_pos++] = c;
            url_input[url_pos] = '\0';
        }
    } else {
        if (c == 'd' || c == 'D') {
            browser_debug_mode = !browser_debug_mode;
        } else if (c == 'b' || c == 'B' || c == '[' || c == '\b' || c == 127) {
            history_back();
        } else if (c == 'n' || c == 'N' || c == ']') {
            history_forward();
        } else if (c == 'h' || c == 'H') {
            history_home();
        } else if (c == 'r' || c == 'R') {
            history_reload();
        } else if (c == 'w' || c == 'W' || c == 'k' || c == 'K') {
            browser_scroll_by(-48);
        } else if (c == 's' || c == 'S' || c == 'j' || c == 'J') {
            browser_scroll_by(48);
        } else if (c == ' ' || c == 'f' || c == 'F') {
            browser_scroll_by(view_h);
        } else if (c == 'g') {
            scroll_y = 0;
        } else if (c == 'G') {
            scroll_y = max_scroll;
        } else if (c == 'l' || c == 'L' || c == '/') {
            url_focused = 1;
        }
    }
}

static void browser_mouse_click(struct window *win, int x, int y, int button) {
    int i;
    (void)win;
    if (button != 1) return;

    /* Toolbar click */
    if (y < 24) {
        if (x >= 4 && x < 26) {
            history_back();
            return;
        }
        if (x >= 28 && x < 50) {
            history_forward();
            return;
        }
        if (x >= 52 && x < 74) {
            history_reload();
            return;
        }
        if (x >= 76 && x < 98) {
            history_home();
            return;
        }
        if (x >= 140) {
            url_focused = 1;
            return;
        }
        return;
    }

    url_focused = 0;

    int content_top = 24;
    int content_bot = win_h - 16;
    int sb_x = win_w - BROWSER_SB_WIDTH;

    /* Check scrollbar click */
    if (x >= sb_x) {
        if (y >= content_top && y < content_top + 14) {
            browser_scroll_by(-48);
            return;
        }
        if (y >= content_bot - 14 && y < content_bot) {
            browser_scroll_by(48);
            return;
        }
        /* Track click */
        int track_top = content_top + 15;
        int track_h = (content_bot - content_top) - 30;
        int total_h = html_layout_height();
        int view_h = (content_bot - content_top) - 8;
        int thumb_h = track_h;
        if (total_h > view_h && total_h > 0) {
            thumb_h = (view_h * track_h) / total_h;
            if (thumb_h < 14) thumb_h = 14;
            if (thumb_h > track_h) thumb_h = track_h;
        }
        int thumb_y = track_top;
        if (max_scroll > 0) {
            thumb_y = track_top + (scroll_y * (track_h - thumb_h)) / max_scroll;
        }

        if (y < thumb_y) {
            browser_scroll_by(-(view_h - 32));
        } else if (y > thumb_y + thumb_h) {
            browser_scroll_by(view_h - 32);
        }
        return;
    }

    /* Check link clicks in web page */
    for (i = 0; i < active_link_count; i++) {
        link_rect_t *r = &active_links[i];
        if (x >= r->x && x <= r->x + r->w && y >= r->y && y <= r->y + r->h) {
            const char *u = html_url(r->url_id);
            if (!u[0]) break;
            if (str_istart(u, "https://")) {
                set_status("HTTPS links are not supported.");
                break;
            }
            copy_str(url_input, u, URL_INPUT_MAX);
            url_pos = (int)strlen(url_input);
            redirect_hops = 0;
            navigate();
            break;
        }
    }
}

static void browser_mouse_drag(struct window *win, int x, int y) {
    (void)win;
    int content_top = 24;
    int content_bot = win_h - 16;
    int sb_x = win_w - BROWSER_SB_WIDTH;

    if (x >= sb_x - 16 && max_scroll > 0) {
        int track_top = content_top + 15;
        int track_h = (content_bot - content_top) - 30;
        int total_h = html_layout_height();
        int view_h = (content_bot - content_top) - 8;
        int thumb_h = track_h;
        if (total_h > view_h && total_h > 0) {
            thumb_h = (view_h * track_h) / total_h;
            if (thumb_h < 14) thumb_h = 14;
            if (thumb_h > track_h) thumb_h = track_h;
        }
        int track_range = track_h - thumb_h;
        if (track_range > 0) {
            int drag_y = (y - track_top) - (thumb_h / 2);
            if (drag_y < 0) drag_y = 0;
            if (drag_y > track_range) drag_y = track_range;
            scroll_y = (drag_y * max_scroll) / track_range;
            if (scroll_y < 0) scroll_y = 0;
            if (scroll_y > max_scroll) scroll_y = max_scroll;
        }
    }
}

static void browser_check_timeout(void) {
    if (browser_state != STATE_RESOLVING && browser_state != STATE_CONNECTING &&
        browser_state != STATE_REQUESTING && browser_state != STATE_DOWNLOADING)
        return;
    if ((int)(tick_count - state_deadline) < 0) return;
    if (browser_state == STATE_DOWNLOADING && (html_text_len() > 0 || http_status > 0)) {
        if (try_redirect()) return;
        browser_state = STATE_FINISHED;
        finish_status_line();
        browser_close_pcb(1);
        return;
    }
    kprintf("[BROWSER] Timeout. state=%d, http_status=%d, rx=%d, hlen=%d\n", browser_state, http_status, bytes_rx, html_text_len());
    browser_state = STATE_ERROR;
    browser_log("Timed out waiting for network. Screen not cleared to see partial data.");
    browser_close_pcb(1);
    browser_advance_fallback();
}

static void browser_update(struct window *win, int dt_ms) {
    (void)win;
    (void)dt_ms;
    net_poll();
    browser_check_timeout();
}

void cmd_browser(int argc, char *argv[]) {
    extern struct window *window_list;
    struct window *curr;
    if (argc > 1) {
        copy_str(url_input, argv[1], URL_INPUT_MAX);
        url_pos = (int)strlen(url_input);
    }
    curr = window_list;
    while (curr) {
        if (curr->draw_client == (void *)browser_draw) {
            curr->state = WM_STATE_ACTIVE;
            curr->mouse_drag = browser_mouse_drag;
            if (argc > 1) { redirect_hops = 0; navigate(); }
            return;
        }
        curr = curr->next;
    }
    {
        window_t *win = wm_add_window(0, TASKBAR_HEIGHT, fb_width,
                                      fb_height - TASKBAR_HEIGHT, "Web Browser", browser_draw);
        if (win) {
            win->update_client = browser_update;
            win->key_event = browser_key_event;
            win->mouse_click = browser_mouse_click;
            win->mouse_drag = browser_mouse_drag;
            /* Load the URL bar target immediately so the page isn't a blank
             * white pane waiting for Enter. */
            redirect_hops = 0;
            navigate();
        }
    }
}
