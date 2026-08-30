/* net/tls_bear.h — BearSSL TLS 1.2 client adapter for STAX OS
 * Provides a thin non-blocking interface between the lwIP TCP callbacks
 * in app_browser.c and the BearSSL SSL engine.
 *
 * Security note: initial version uses zero trusted anchors (trust-all mode).
 * Traffic is encrypted but server identity is not verified.
 * A [TLS⚠] badge is shown in the browser status bar.
 */
#ifndef TLS_BEAR_H
#define TLS_BEAR_H

#include <stdint.h>
#include <stddef.h>

/* Initialise the BearSSL client context for a new connection.
 * hostname: server hostname for SNI extension.
 * Returns 0 on success, -1 on error. */
int tls_init(const char *hostname);

/* Tear down and zero all TLS state. Must be called when the TCP
 * connection closes, whether cleanly or on error. */
void tls_reset(void);

/* Push raw ciphertext bytes received from the TCP stack into the
 * BearSSL engine's receive buffer.
 * Returns 0 on success, -1 if the engine has signalled a fatal error. */
int tls_feed_recv(const uint8_t *data, size_t len);

/* Drive the BearSSL state machine: flushes any pending encrypted output
 * to the TCP layer via the registered send callback, and processes any
 * newly available decrypted application data.
 * Must be called after tls_feed_recv() and also periodically on each
 * browser_poll() tick during handshake.
 * Returns 0 on success, -1 on fatal TLS error. */
int tls_pump(void);

/* Encrypt and queue plaintext application data to be sent to the server.
 * May only be called after tls_handshake_done() returns 1.
 * Returns number of bytes accepted, or -1 on error. */
int tls_write(const uint8_t *plain, size_t len);

/* Read available decrypted application data into buf.
 * Returns number of bytes copied, 0 if nothing available, -1 on error. */
int tls_read(uint8_t *buf, size_t max_len);

/* Returns 1 when the TLS handshake has completed successfully. */
int tls_handshake_done(void);

/* Returns 1 if BearSSL has reported a fatal alert (connection unusable). */
int tls_has_error(void);

/* Register the send callback used by tls_pump() to write ciphertext to
 * the network. Must be called once after tls_init().
 * send_fn(data, len, arg) should call tcp_write / tcp_output. */
typedef int (*tls_send_fn_t)(const uint8_t *data, size_t len, void *arg);
void tls_set_send_cb(tls_send_fn_t fn, void *arg);

#endif /* TLS_BEAR_H */
