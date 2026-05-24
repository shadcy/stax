#include "fatfs/ff.h"
#include "fatfs/diskio.h"
#include <stdint.h>

/* From our PL181 driver (fs/disk.c) */
extern int mci_init(void);
extern int pl181_disk_read(uint32_t lba, uint8_t *buf);
extern int pl181_disk_write(uint32_t lba, const uint8_t *buf);

DSTATUS disk_status(BYTE pdrv)
{
    if (pdrv != 0) return STA_NOINIT;
    return 0; /* OK */
}

DSTATUS disk_initialize(BYTE pdrv)
{
    if (pdrv != 0) return STA_NOINIT;
    if (mci_init() != 0) return STA_NOINIT;
    return 0;
}

DRESULT disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count)
{
    if (pdrv != 0) return RES_PARERR;
    for (UINT i = 0; i < count; i++) {
        if (pl181_disk_read((uint32_t)(sector + i), (uint8_t*)buff) != 0) return RES_ERROR;
        buff += 512;
    }
    return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count)
{
    if (pdrv != 0) return RES_PARERR;
    for (UINT i = 0; i < count; i++) {
        if (pl181_disk_write((uint32_t)(sector + i), (const uint8_t*)buff) != 0) return RES_ERROR;
        buff += 512;
    }
    return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff)
{
    (void)buff;
    if (pdrv != 0) return RES_PARERR;
    if (cmd == CTRL_SYNC) return RES_OK;
    return RES_PARERR;
}

DWORD get_fattime(void)
{
    /* Returning a fixed time if NORTC is not used, but NORTC is enabled so this might not be called. */
    return ((DWORD)(2025 - 1980) << 25) | ((DWORD)1 << 21) | ((DWORD)1 << 16);
}
