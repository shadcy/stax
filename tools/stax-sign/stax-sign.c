#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../firmware/image_format/firmware_format.h"
#include "../../crypto/sha256/sha256.h"
#include "../../crypto/crc32/crc32.h"
#include "../../include/monocypher.h"
// Monocypher's Ed25519 API requires random bytes for key generation.
// We'll use /dev/urandom on Linux.

void print_usage() {
    printf("STAX Firmware Signing Tool\n");
    printf("Usage:\n");
    printf("  stax-sign --gen-key <prefix>\n");
    printf("  stax-sign --sign <in.bin> --version <N> --key <priv.key> --output <out.stax>\n");
}

int generate_keys(const char *prefix) {
    uint8_t seed[32];
    FILE *urandom = fopen("/dev/urandom", "rb");
    if (!urandom) {
        perror("Failed to open /dev/urandom");
        return 1;
    }
    fread(seed, 1, 32, urandom);
    fclose(urandom);

    uint8_t secret_key[64];
    uint8_t public_key[32];
    
    crypto_eddsa_key_pair(secret_key, public_key, seed);

    char priv_path[256];
    char pub_path[256];
    snprintf(priv_path, sizeof(priv_path), "%s.priv", prefix);
    snprintf(pub_path, sizeof(pub_path), "%s.pub", prefix);

    FILE *f_priv = fopen(priv_path, "wb");
    if (!f_priv) { perror("fopen priv"); return 1; }
    fwrite(secret_key, 1, 64, f_priv);
    fclose(f_priv);

    FILE *f_pub = fopen(pub_path, "wb");
    if (!f_pub) { perror("fopen pub"); return 1; }
    fwrite(public_key, 1, 32, f_pub);
    fclose(f_pub);

    char hdr_path[256];
    snprintf(hdr_path, sizeof(hdr_path), "%s.pub.h", prefix);
    FILE *f_hdr = fopen(hdr_path, "w");
    if (f_hdr) {
        fprintf(f_hdr, "#ifndef STAX_KEY_PUB_H\n#define STAX_KEY_PUB_H\n\n");
        fprintf(f_hdr, "const uint8_t STAX_PUBLIC_KEY[32] = {\n    ");
        for(int i=0; i<32; i++) {
            fprintf(f_hdr, "0x%02X", public_key[i]);
            if (i < 31) fprintf(f_hdr, ", ");
            if ((i + 1) % 8 == 0 && i < 31) fprintf(f_hdr, "\n    ");
        }
        fprintf(f_hdr, "\n};\n\n#endif\n");
        fclose(f_hdr);
    }

    printf("Generated %s, %s and %s\n", priv_path, pub_path, hdr_path);
    return 0;
}

int sign_firmware(const char *in_path, int version, const char *key_path, const char *out_path) {
    FILE *f_in = fopen(in_path, "rb");
    if (!f_in) { perror("fopen in"); return 1; }

    fseek(f_in, 0, SEEK_END);
    long size = ftell(f_in);
    fseek(f_in, 0, SEEK_SET);

    uint8_t *payload = malloc(size);
    fread(payload, 1, size, f_in);
    fclose(f_in);

    FILE *f_key = fopen(key_path, "rb");
    if (!f_key) { perror("fopen key"); free(payload); return 1; }
    uint8_t secret_key[64];
    fread(secret_key, 1, 64, f_key);
    fclose(f_key);

    firmware_header_t header;
    memset(&header, 0, sizeof(header));
    header.magic = FIRMWARE_MAGIC;
    header.format_ver = FIRMWARE_FORMAT_VERSION;
    header.image_ver = version;
    header.image_size = size;
    header.load_addr = 0x100000;
    header.entry_point = 0x100000;
    header.flags = 0;

    // Hash payload
    sha256(payload, size, header.payload_hash);
    
    printf("DEBUG stax-sign: Payload[0..3] = %02X %02X %02X %02X\n", payload[0], payload[1], payload[2], payload[3]);
    printf("DEBUG stax-sign: Hash[0..3] = %02X %02X %02X %02X\n", header.payload_hash[0], header.payload_hash[1], header.payload_hash[2], header.payload_hash[3]);

    // Sign the hash
    crypto_eddsa_sign(header.signature, secret_key, header.payload_hash, 32);

    // Calculate CRC of the header (first 28 bytes)
    header.crc32 = crc32((const uint8_t *)&header, 28);

    FILE *f_out = fopen(out_path, "wb");
    if (!f_out) { perror("fopen out"); free(payload); return 1; }
    fwrite(&header, 1, sizeof(header), f_out);
    fwrite(payload, 1, size, f_out);
    fclose(f_out);

    free(payload);
    printf("Successfully signed firmware: %s (v%d, %ld bytes payload)\n", out_path, version, size);
    
    // Also print out public key as C array so it can be pasted into bootloader
    // Extract public key from the second half of secret_key
    uint8_t *public_key = secret_key + 32;
    printf("Public Key (C array format for bootloader):\n");
    printf("const uint8_t STAX_PUBLIC_KEY[32] = {\n    ");
    for(int i=0; i<32; i++) {
        printf("0x%02X", public_key[i]);
        if (i < 31) printf(", ");
        if ((i + 1) % 8 == 0 && i < 31) printf("\n    ");
    }
    printf("\n};\n");
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    if (strcmp(argv[1], "--gen-key") == 0 && argc == 3) {
        return generate_keys(argv[2]);
    } else if (strcmp(argv[1], "--sign") == 0 && argc == 9) {
        const char *in_path = argv[2];
        int version = 0;
        const char *key_path = NULL;
        const char *out_path = NULL;

        for (int i = 3; i < argc; i += 2) {
            if (strcmp(argv[i], "--version") == 0) version = atoi(argv[i+1]);
            else if (strcmp(argv[i], "--key") == 0) key_path = argv[i+1];
            else if (strcmp(argv[i], "--output") == 0) out_path = argv[i+1];
        }

        if (in_path && key_path && out_path) {
            return sign_firmware(in_path, version, key_path, out_path);
        }
    }

    print_usage();
    return 1;
}
