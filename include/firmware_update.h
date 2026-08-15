#ifndef FIRMWARE_UPDATE_H
#define FIRMWARE_UPDATE_H

#include <stdint.h>

// Initialize firmware update subsystem
void stax_firmware_init(void);

// Write a signed firmware image (stax_firmware.stax) into the inactive slot
// file_path: path in the FAT16 filesystem (e.g. "/FIRMWARE.STAX")
// returns 0 on success, < 0 on error
int stax_firmware_update(const char *file_path);

// Confirm that the current boot was successful, preventing watchdog rollback
// returns 0 on success, < 0 on error
int stax_firmware_confirm(void);

#endif
