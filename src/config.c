#include "config.h"
#include "stm32l4xx_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Config g_config;

/* ---------- Domaines de définition (cahier des charges) ---------- */

typedef enum { PARAM_U8, PARAM_I8, PARAM_U16 } ParamType;

typedef struct {
    const char *name;
    void *ptr;
    ParamType type;
    long min;
    long max;
} ParamDef;

static const ParamDef params[] = {
    { "LOG_INTERVAL",  &g_config.log_interval_min, PARAM_U16, 1,   1440  },
    { "FILE_MAX_SIZE", &g_config.file_max_size,    PARAM_U16, 512, 32768 },
    { "TIMEOUT",       &g_config.timeout_s,        PARAM_U16, 1,   300   },
    { "LUMIN",         &g_config.lumin,            PARAM_U8,  0,   1     },
    { "LUMIN_LOW",     &g_config.lumin_low,        PARAM_U16, 0,   1023  },
    { "LUMIN_HIGH",    &g_config.lumin_high,       PARAM_U16, 0,   1023  },
    { "TEMP_AIR",      &g_config.temp_air,         PARAM_U8,  0,   1     },
    { "MIN_TEMP_AIR",  &g_config.min_temp_air,     PARAM_I8,  -40, 85    },
    { "MAX_TEMP_AIR",  &g_config.max_temp_air,     PARAM_I8,  -40, 85    },
    { "HYGR",          &g_config.hygr,             PARAM_U8,  0,   1     },
    { "HYGR_MINT",     &g_config.hygr_mint,        PARAM_I8,  -40, 85    },
    { "HYGR_MAXT",     &g_config.hygr_maxt,        PARAM_I8,  -40, 85    },
    { "PRESSURE",      &g_config.pressure,         PARAM_U8,  0,   1     },
    { "PRESSURE_MIN",  &g_config.pressure_min,     PARAM_U16, 300, 1100  },
    { "PRESSURE_MAX",  &g_config.pressure_max,     PARAM_U16, 300, 1100  },
};

#define PARAM_COUNT (sizeof(params) / sizeof(params[0]))

static const ParamDef *find_param(const char *name)
{
    for (unsigned i = 0; i < PARAM_COUNT; i++) {
        if (strcmp(params[i].name, name) == 0)
            return &params[i];
    }
    return 0;
}

static long get_value(const ParamDef *p)
{
    switch (p->type) {
        case PARAM_U8:  return *(uint8_t *)p->ptr;
        case PARAM_I8:  return *(int8_t *)p->ptr;
        case PARAM_U16: return *(uint16_t *)p->ptr;
    }
    return 0;
}

static void set_value(const ParamDef *p, long value)
{
    switch (p->type) {
        case PARAM_U8:  *(uint8_t *)p->ptr = (uint8_t)value;   break;
        case PARAM_I8:  *(int8_t *)p->ptr = (int8_t)value;     break;
        case PARAM_U16: *(uint16_t *)p->ptr = (uint16_t)value; break;
    }
}

static bool thresholds_coherent(void)
{
    return g_config.lumin_low < g_config.lumin_high &&
           g_config.min_temp_air < g_config.max_temp_air &&
           g_config.hygr_mint < g_config.hygr_maxt &&
           g_config.pressure_min < g_config.pressure_max;
}

void Config_LoadDefaults(void)
{
    g_config.log_interval_min = 10;
    g_config.file_max_size    = 2048;
    g_config.timeout_s        = 30;

    g_config.lumin      = 1;
    g_config.lumin_low  = 255;
    g_config.lumin_high = 768;

    g_config.temp_air     = 1;
    g_config.min_temp_air = -10;
    g_config.max_temp_air = 60;

    g_config.hygr      = 1;
    g_config.hygr_mint = 0;
    g_config.hygr_maxt = 50;

    g_config.pressure     = 1;
    g_config.pressure_min = 850;
    g_config.pressure_max = 1080;
}

ConfigSetResult Config_Set(const char *name, const char *value)
{
    const ParamDef *p = find_param(name);
    if (p == 0)
        return CONFIG_SET_UNKNOWN;

    char *end;
    long v = strtol(value, &end, 10);
    if (end == value || *end != '\0')
        return CONFIG_SET_BAD_VALUE;

    if (v < p->min || v > p->max)
        return CONFIG_SET_OUT_OF_RANGE;

    long old = get_value(p);
    set_value(p, v);
    if (!thresholds_coherent()) {
        set_value(p, old);
        return CONFIG_SET_INCOHERENT;
    }
    return CONFIG_SET_OK;
}

bool Config_GetRange(const char *name, long *min, long *max)
{
    const ParamDef *p = find_param(name);
    if (p == 0)
        return false;

    *min = p->min;
    *max = p->max;
    return true;
}

void Config_Print(void)
{
    for (unsigned i = 0; i < PARAM_COUNT; i++) {
        printf("  %-14s = %ld\r\n", params[i].name, get_value(&params[i]));
    }
}

/* ---------- EEPROM émulée en flash ---------- */

#define CONFIG_FLASH_ADDR  0x080FF800UL // Dernière page (2 Ko) de la banque 2
#define CONFIG_FLASH_PAGE  255
#define CONFIG_MAGIC       0x57335731UL // "W3W1" : page déjà écrite par ce programme

// Image écrite en flash : programmation par mots de 64 bits
typedef union {
    struct {
        uint32_t magic;
        uint32_t checksum;
        Config config;
    } s;
    uint64_t dwords[(2 * sizeof(uint32_t) + sizeof(Config) + 7) / 8];
} ConfigFlashImage;

static uint32_t checksum(const Config *c)
{
    const uint8_t *p = (const uint8_t *)c;
    uint32_t sum = 5381;

    for (unsigned i = 0; i < sizeof(*c); i++)
        sum = sum * 33 + p[i];
    return sum;
}

bool Config_Load(void)
{
    const ConfigFlashImage *img = (const ConfigFlashImage *)CONFIG_FLASH_ADDR;

    if (img->s.magic == CONFIG_MAGIC && img->s.checksum == checksum(&img->s.config)) {
        g_config = img->s.config;
        if (thresholds_coherent())
            return true;
    }

    Config_LoadDefaults();
    return false;
}

bool Config_Save(void)
{
    ConfigFlashImage img;
    memset(&img, 0, sizeof(img));
    img.s.magic = CONFIG_MAGIC;
    img.s.config = g_config;
    img.s.checksum = checksum(&img.s.config);

    FLASH_EraseInitTypeDef erase = {
        .TypeErase = FLASH_TYPEERASE_PAGES,
        .Banks = FLASH_BANK_2,
        .Page = CONFIG_FLASH_PAGE,
        .NbPages = 1
    };
    uint32_t page_error = 0;

    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);

    bool ok = HAL_FLASHEx_Erase(&erase, &page_error) == HAL_OK;
    for (unsigned i = 0; ok && i < sizeof(img.dwords) / sizeof(img.dwords[0]); i++) {
        ok = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, CONFIG_FLASH_ADDR + i * 8, img.dwords[i]) == HAL_OK;
    }

    HAL_FLASH_Lock();
    return ok;
}
