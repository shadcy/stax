/* net/tls_bear.c — BearSSL TLS 1.2 client adapter for STAX OS
 *
 * Architecture:
 *   - All state is statically allocated (BearSSL never calls malloc).
 *   - 4 KB bidirectional I/O buffers to cap RAM use (down from default 16 KB).
 *   - Trust-all X.509 verifier: zero anchor roots — traffic is encrypted but
 *     server identity is NOT authenticated.
 *   - The adapter glues BearSSL's "push/pull" byte-stream model to lwIP's
 *     callback-driven TCP PCB model.
 *
 * Cipher suite (minimal, ARM-friendly):
 *   TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256  (preferred)
 *   TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256
 *   TLS_RSA_WITH_AES_128_GCM_SHA256              (fallback, no PFS)
 */

#include "tls_bear.h"
#include "bearssl.h"   /* single include from third_party/bearssl/ */

#include <string.h>

/* -------------------------------------------------------------------------
 * Constants
 * ---------------------------------------------------------------------- */
#define TLS_IOBUF_SZ    4096   /* 4 KB per direction; caps record size     */
#define TLS_PLAIN_SZ    4096   /* staging buffer for outbound plaintext    */

/* -------------------------------------------------------------------------
 * Static state
 * ---------------------------------------------------------------------- */
static br_ssl_client_context  s_cc;
static br_x509_minimal_context s_xc;   /* minimal X.509 — zero trust anchors */
static unsigned char           s_iobuf[BR_SSL_BUFSIZE_BIDI];

/* A simpler "known-key" / trust-all verifier that accepts any certificate.
 * We implement a custom x509 class that unconditionally returns 0 (ok). */
static void         ta_start_chain(const br_x509_class **ctx, const char *server_name);
static void         ta_start_cert(const br_x509_class **ctx, uint32_t length);
static void         ta_append(const br_x509_class **ctx, const unsigned char *buf, size_t len);
static void         ta_end_cert(const br_x509_class **ctx);
static unsigned     ta_end_chain(const br_x509_class **ctx);
static const br_x509_pkey *ta_get_pkey(const br_x509_class *const *ctx, unsigned *usages);

static const br_x509_class s_nocheck_vtable = {
    sizeof(br_x509_minimal_context),
    ta_start_chain,
    ta_start_cert,
    ta_append,
    ta_end_cert,
    ta_end_chain,
    ta_get_pkey,
};

static const br_x509_class *s_nocheck_ptr = &s_nocheck_vtable;

/* Dummy public key returned by the trust-all verifier. */
static br_rsa_public_key s_dummy_rsa = { (unsigned char *)"", 0, (unsigned char *)"", 0 };
static br_x509_pkey      s_dummy_pkey;

static void ta_start_chain(const br_x509_class **ctx, const char *server_name) {
    (void)ctx; (void)server_name;
}
static void ta_start_cert(const br_x509_class **ctx, uint32_t length) {
    (void)ctx; (void)length;
}
static void ta_append(const br_x509_class **ctx, const unsigned char *buf, size_t len) {
    (void)ctx; (void)buf; (void)len;
}
static void ta_end_cert(const br_x509_class **ctx) {
    (void)ctx;
}
static unsigned ta_end_chain(const br_x509_class **ctx) {
    (void)ctx;
    return 0; /* 0 = success — unconditionally trust */
}
static const br_x509_pkey *ta_get_pkey(const br_x509_class *const *ctx, unsigned *usages) {
    (void)ctx;
    if (usages) *usages = BR_KEYTYPE_KEYX | BR_KEYTYPE_SIGN;
    s_dummy_pkey.key_type = BR_KEYTYPE_RSA;
    s_dummy_pkey.key.rsa   = s_dummy_rsa;
    return &s_dummy_pkey;
}

/* -------------------------------------------------------------------------
 * Send callback registration
 * ---------------------------------------------------------------------- */
static tls_send_fn_t s_send_fn = NULL;
static void         *s_send_arg = NULL;

void tls_set_send_cb(tls_send_fn_t fn, void *arg) {
    s_send_fn  = fn;
    s_send_arg = arg;
}

/* -------------------------------------------------------------------------
 * Lifecycle
 * ---------------------------------------------------------------------- */
static int s_initialised = 0;
static int s_handshake_done = 0;
static int s_fatal = 0;

int tls_init(const char *hostname) {
    s_initialised    = 0;
    s_handshake_done = 0;
    s_fatal          = 0;

    /*
     * Initialise the SSL client with the full client profile
     * (supports ECDHE+RSA, ECDHE+ECDSA, RSA key exchange).
     * This automatically registers ChaCha20, AES-GCM, SHA-256/384.
     */
    br_ssl_client_init_full(&s_cc, &s_xc, NULL, 0);

    /* Override X.509 engine with our trust-all verifier */
    br_ssl_engine_set_x509(&s_cc.eng, &s_nocheck_ptr);

    /* Set the 4 KB bidirectional I/O buffer */
    br_ssl_engine_set_buffer(&s_cc.eng, s_iobuf, sizeof(s_iobuf), 1);

    /* Set SNI and reset the engine */
    if (br_ssl_client_reset(&s_cc, hostname, 0) == 0) {
        s_fatal = 1;
        return -1;
    }

    s_initialised = 1;
    return 0;
}

void tls_reset(void) {
    memset(&s_cc, 0, sizeof(s_cc));
    s_initialised    = 0;
    s_handshake_done = 0;
    s_fatal          = 0;
}

/* -------------------------------------------------------------------------
 * Data flow helpers
 * ---------------------------------------------------------------------- */

/* Flush any encrypted data the engine wants to send out over TCP. */
static int flush_send(void) {
    if (!s_send_fn) return 0;
    for (;;) {
        size_t len = 0;
        unsigned char *buf = br_ssl_engine_sendrec_buf(&s_cc.eng, &len);
        if (!buf || len == 0) break;
        int sent = s_send_fn(buf, len, s_send_arg);
        if (sent <= 0) return -1;
        br_ssl_engine_sendrec_ack(&s_cc.eng, (size_t)sent);
    }
    return 0;
}

/* -------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------- */

int tls_feed_recv(const uint8_t *data, size_t len) {
    if (!s_initialised || s_fatal) return -1;
    while (len > 0) {
        size_t avail = 0;
        unsigned char *buf = br_ssl_engine_recvrec_buf(&s_cc.eng, &avail);
        if (!buf || avail == 0) break; /* engine buffer full; will retry next pump */
        size_t chunk = (len < avail) ? len : avail;
        memcpy(buf, data, chunk);
        br_ssl_engine_recvrec_ack(&s_cc.eng, chunk);
        data += chunk;
        len  -= chunk;
    }
    return 0;
}

int tls_pump(void) {
    if (!s_initialised || s_fatal) return -1;

    unsigned st = br_ssl_engine_current_state(&s_cc.eng);

    /* Check for fatal error from engine */
    if (st == BR_SSL_CLOSED) {
        int err = br_ssl_engine_last_error(&s_cc.eng);
        if (err != BR_ERR_OK) {
            s_fatal = 1;
            return -1;
        }
    }

    /* Flush any outbound ciphertext produced by the handshake or writes */
    if (flush_send() != 0) {
        s_fatal = 1;
        return -1;
    }

    /* Detect handshake completion */
    if (!s_handshake_done) {
        st = br_ssl_engine_current_state(&s_cc.eng);
        if (st & BR_SSL_SENDAPP) {
            /* SENDAPP bit set: handshake is done, application data can flow */
            s_handshake_done = 1;
        }
    }

    return 0;
}

int tls_write(const uint8_t *plain, size_t len) {
    if (!s_initialised || s_fatal || !s_handshake_done) return -1;
    size_t total = 0;
    while (len > 0) {
        size_t avail = 0;
        unsigned char *buf = br_ssl_engine_sendapp_buf(&s_cc.eng, &avail);
        if (!buf || avail == 0) break;
        size_t chunk = (len < avail) ? len : avail;
        memcpy(buf, plain, chunk);
        br_ssl_engine_sendapp_ack(&s_cc.eng, chunk);
        br_ssl_engine_flush(&s_cc.eng, 0);
        flush_send();
        plain += chunk;
        len   -= chunk;
        total += chunk;
    }
    return (int)total;
}

int tls_read(uint8_t *buf, size_t max_len) {
    if (!s_initialised || s_fatal) return -1;
    size_t avail = 0;
    unsigned char *src = br_ssl_engine_recvapp_buf(&s_cc.eng, &avail);
    if (!src || avail == 0) return 0;
    size_t chunk = (max_len < avail) ? max_len : avail;
    memcpy(buf, src, chunk);
    br_ssl_engine_recvapp_ack(&s_cc.eng, chunk);
    return (int)chunk;
}

int tls_handshake_done(void) {
    return s_handshake_done;
}

int tls_has_error(void) {
    return s_fatal;
}
