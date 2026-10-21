#ifndef SD_LOGGER_H
#define SD_LOGGER_H

#include <stdbool.h>

typedef enum
{
	SDLOGGER_CARD_UNKNOWN = 0,
	SDLOGGER_CARD_SDSC,
	SDLOGGER_CARD_SDHC
} SDLoggerCardType;

typedef enum
{
	SDLOGGER_INIT_OK = 0,
	SDLOGGER_INIT_ERR_CMD0,
	SDLOGGER_INIT_ERR_CMD8_PATTERN,
	SDLOGGER_INIT_ERR_ACMD41_TIMEOUT,
	SDLOGGER_INIT_ERR_CMD58,
	SDLOGGER_INIT_ERR_CMD1_TIMEOUT
} SDLoggerInitStatus;

bool SDLogger_Init(void);
SDLoggerCardType SDLogger_GetCardType(void);
SDLoggerInitStatus SDLogger_GetLastInitStatus(void);
const char *SDLogger_GetActiveCsLabel(void);
bool SDLogger_BitBangCMD0Test(void);

// Accès par secteurs de 512 octets (utilisé par FatFs, voir lib/fatfs/diskio.c)
#include <stdint.h>
bool SDLogger_IsReady(void);
bool SDLogger_ReadBlocks(uint8_t *buf, uint32_t sector, uint32_t count);
bool SDLogger_WriteBlocks(const uint8_t *buf, uint32_t sector, uint32_t count);
bool SDLogger_GetSectorCount(uint32_t *count);
bool SDLogger_Sync(void);

#endif
