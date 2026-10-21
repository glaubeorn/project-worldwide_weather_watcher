/*
 * Lien entre FatFs et la carte SD en SPI (driver sd_logger).
 * Un seul volume : la carte SD (numéro 0).
 */
#include "diskio.h"
#include "ff.h"
#include "sd_logger.h"
#include "grove_rtc_ds1307.h"

static DSTATUS status = STA_NOINIT;

DSTATUS disk_initialize(BYTE pdrv)
{
    if (pdrv != 0)
        return STA_NOINIT;

    status = SDLogger_Init() ? 0 : STA_NOINIT;
    return status;
}

DSTATUS disk_status(BYTE pdrv)
{
    if (pdrv != 0)
        return STA_NOINIT;

    return status;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
    if (pdrv != 0 || count == 0)
        return RES_PARERR;
    if (status & STA_NOINIT)
        return RES_NOTRDY;

    return SDLogger_ReadBlocks(buff, sector, count) ? RES_OK : RES_ERROR;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
    if (pdrv != 0 || count == 0)
        return RES_PARERR;
    if (status & STA_NOINIT)
        return RES_NOTRDY;

    return SDLogger_WriteBlocks(buff, sector, count) ? RES_OK : RES_ERROR;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    if (pdrv != 0)
        return RES_PARERR;
    if (status & STA_NOINIT)
        return RES_NOTRDY;

    switch (cmd) {
        case CTRL_SYNC:
            return SDLogger_Sync() ? RES_OK : RES_ERROR;
        case GET_SECTOR_COUNT:
            return SDLogger_GetSectorCount((uint32_t *)buff) ? RES_OK : RES_ERROR;
        case GET_SECTOR_SIZE:
            *(WORD *)buff = 512;
            return RES_OK;
        case GET_BLOCK_SIZE:
            *(DWORD *)buff = 1; // Taille de bloc d'effacement inconnue
            return RES_OK;
        default:
            return RES_PARERR;
    }
}

// Date des fichiers : heure de la RTC, ou 01/01/2026 00:00 si elle ne répond pas
DWORD get_fattime(void)
{
    RTC_DateTime dt = { .seconds = 0, .minutes = 0, .hours = 0, .day = 1, .month = 1, .year = 2026 };

    if (!GroveRTC_GetDateTime(&dt)) {
        dt.seconds = 0; dt.minutes = 0; dt.hours = 0;
        dt.day = 1; dt.month = 1; dt.year = 2026;
    }

    return ((DWORD)(dt.year - 1980) << 25) | ((DWORD)dt.month << 21) | ((DWORD)dt.day << 16) |
           ((DWORD)dt.hours << 11) | ((DWORD)dt.minutes << 5) | ((DWORD)dt.seconds >> 1);
}
