#include "firmware_bench.h"
#include <monocypher.h>
#include "../../crypto/crc32/crc32.h"
#include "../../crypto/sha256/sha256.h"

extern void kputs(const char *s);
extern void kput_uint(uint32_t n);

/* Mock firmware payload for benchmark (use a fixed small block scaled up by repeating) */
static uint8_t mock_payload[16 * 1024]; /* 16 KB block */

static void fill_mock_data(void) {
    for (int i = 0; i < 16 * 1024; i++) {
        mock_payload[i] = (uint8_t)(i ^ (i >> 8));
    }
}

void bench_secure_boot_run(void) {
    bench_section("SECURE BOOT CRYPTO PERFORMANCE");

    fill_mock_data();

    bench_result_t r_crc, r_sha, r_ed;

    /* 1. Benchmark CRC32 (16 KB) */
    BENCH_RUN(&r_crc, 10, {
        crc32(mock_payload, 16 * 1024);
    });
    r_crc.name = "crc32_16kb";
    r_crc.unit = "us";
    bench_report(&r_crc);

    /* 2. Benchmark SHA-256 (16 KB) */
    BENCH_RUN(&r_sha, 10, {
        sha256_ctx ctx;
        sha256_init(&ctx);
        sha256_update(&ctx, mock_payload, 16 * 1024);
        uint8_t hash[32];
        sha256_final(&ctx, hash);
    });
    r_sha.name = "sha256_16kb";
    r_sha.unit = "us";
    bench_report(&r_sha);

    /* 3. Benchmark Ed25519 verification */
    uint8_t mock_sig[64] = {0};
    uint8_t mock_pub[32] = {0};
    uint8_t mock_hash[32] = {0};
    BENCH_RUN(&r_ed, 10, {
        crypto_eddsa_check(mock_sig, mock_pub, mock_hash, 32);
    });
    r_ed.name = "ed25519_verify";
    r_ed.unit = "us";
    bench_report(&r_ed);
}
