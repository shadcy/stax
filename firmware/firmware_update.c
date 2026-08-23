#include "firmware_update.h"
#include "../boot/metadata.h"
#include "console.h"
#include "fat.h"
#include "string.h"
#include "heap.h"
#include "command.h"

extern int pl181_disk_read(uint32_t sector, uint8_t *buf);
extern int pl181_disk_write(uint32_t sector, const uint8_t *buf);
extern uint32_t crc32(const uint8_t *data, size_t length);

static boot_metadata_t current_meta;
static int meta_valid = 0;

void stax_firmware_init(void) {
    uint8_t buf_a[512];
    uint8_t buf_b[512];
    pl181_disk_read(1, buf_a);
    pl181_disk_read(2, buf_b);

    boot_metadata_t *meta_a = (boot_metadata_t *)buf_a;
    boot_metadata_t *meta_b = (boot_metadata_t *)buf_b;

    int a_valid = (meta_a->magic == METADATA_MAGIC && crc32(buf_a, sizeof(boot_metadata_t) - 4) == meta_a->crc32);
    int b_valid = (meta_b->magic == METADATA_MAGIC && crc32(buf_b, sizeof(boot_metadata_t) - 4) == meta_b->crc32);

    if (a_valid && b_valid) {
        current_meta = (meta_a->generation >= meta_b->generation) ? *meta_a : *meta_b;
        meta_valid = 1;
    } else if (a_valid) {
        current_meta = *meta_a;
        meta_valid = 1;
    } else if (b_valid) {
        current_meta = *meta_b;
        meta_valid = 1;
    } else {
        meta_valid = 0;
    }
}

static int write_metadata(boot_metadata_t *meta) {
    meta->generation++;
    meta->crc32 = crc32((const uint8_t *)meta, sizeof(boot_metadata_t) - 4);
    
    uint8_t buf[512];
    for (int i = 0; i < 512; i++) buf[i] = 0;
    for (uint32_t i = 0; i < sizeof(boot_metadata_t); i++) {
        buf[i] = ((uint8_t *)meta)[i];
    }
    
    if (pl181_disk_write(1, buf) != 0) return -1;
    if (pl181_disk_write(2, buf) != 0) return -1;
    return 0;
}

int stax_firmware_confirm(void) {
    if (!meta_valid) return -1;
    
    uint32_t state = (current_meta.active_slot == 0) ? current_meta.slot_a_state : current_meta.slot_b_state;
    if (state == SLOT_STATE_BOOTING) {
        if (current_meta.active_slot == 0) current_meta.slot_a_state = SLOT_STATE_CONFIRMED;
        else current_meta.slot_b_state = SLOT_STATE_CONFIRMED;
        
        if (write_metadata(&current_meta) == 0) {
            kputs("Firmware update confirmed successfully!\n");
            return 0;
        }
    }
    return 0;
}

int stax_firmware_update(const char *file_path) {
    if (!meta_valid) {
        kputs("Error: Boot metadata invalid.\n");
        return -1;
    }

    fat_file_t *f = fat_open(file_path);
    if (!f) {
        kputs("Error: Could not open firmware file.\n");
        return -1;
    }

    uint32_t file_size = fat_file_size(f);
    if (file_size > 2048 * 512) {
        kputs("Error: Firmware image too large.\n");
        fat_close(f);
        return -1;
    }
    
    // Read version from header
    uint32_t fw_version = 0;
    uint8_t hdr_buf[256]; // Enough for firmware_header_t
    if (fat_read(f, hdr_buf, sizeof(hdr_buf)) > 0) {
        // firmware_header_t offset to version is 8 bytes
        // struct: magic(4), format_ver(4), version(4)
        fw_version = *(uint32_t*)(hdr_buf + 8);
    }
    fat_seek(f, 0); // Rewind

    uint32_t inactive_slot = (current_meta.active_slot == 0) ? 1 : 0;
    uint32_t target_lba = (inactive_slot == 0) ? 3 : 2051;
    
    kputs("Writing firmware to inactive slot...\n");

    uint8_t *sec_buf = kmalloc(512);
    if (!sec_buf) {
        fat_close(f);
        return -1;
    }

    uint32_t bytes_read = 0;
    uint32_t current_lba = target_lba;
    
    while (bytes_read < file_size) {
        uint32_t chunk = (file_size - bytes_read) > 512 ? 512 : (file_size - bytes_read);
        for(int i=0; i<512; i++) sec_buf[i] = 0;
        fat_read(f, sec_buf, chunk);
        if (pl181_disk_write(current_lba, sec_buf) != 0) {
            kputs("Error writing to raw flash.\n");
            kfree(sec_buf);
            fat_close(f);
            return -1;
        }
        current_lba++;
        bytes_read += chunk;
        if ((current_lba - target_lba) % 64 == 0) kputs(".");
    }
    kputs("\nFirmware written successfully.\n");
    
    kfree(sec_buf);
    fat_close(f);

    // Update metadata
    if (inactive_slot == 0) {
        current_meta.slot_a_state = SLOT_STATE_PENDING;
        current_meta.slot_a_version = fw_version;
    } else {
        current_meta.slot_b_state = SLOT_STATE_PENDING;
        current_meta.slot_b_version = fw_version;
    }
    
    current_meta.active_slot = inactive_slot;

    if (write_metadata(&current_meta) != 0) {
        kputs("Error updating metadata.\n");
        return -1;
    }

    kputs("Update staged. Reboot to apply.\n");
    return 0;
}
