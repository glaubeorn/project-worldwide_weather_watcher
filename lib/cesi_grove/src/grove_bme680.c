#include "grove_bme680.h"
#include "board_pins.h"

extern I2C_HandleTypeDef GROVE_I2C_HANDLE;

/*
 * Driver capteur environnement Bosch : température, pression, humidité.
 * Le modèle est détecté automatiquement grâce au chip ID :
 * - 0x60 : BME280 (capteur monté sur le prototype)
 * - 0x61 : BME680 (capteur de gaz non utilisé, chauffage coupé)
 * Mesure en mode "forced", puis compensation avec les coefficients de
 * calibration usine (formules de la datasheet Bosch).
 */

#define REG_CHIP_ID   0xD0
#define REG_RESET     0xE0
#define SOFT_RESET    0xB6

// Suréchantillonnage : humidité x1, température x2, pression x4, mode forced
#define CTRL_HUM_VALUE  0x01
#define CTRL_MEAS_VALUE ((0x02 << 5) | (0x03 << 2) | 0x01)

#define MEAS_TIMEOUT_MS 200

// --- Registres BME280 ---
#define BME280_REG_CALIB1    0x88  // 26 octets (T1..P9, H1)
#define BME280_REG_CALIB2    0xE1  // 7 octets (H2..H6)
#define BME280_REG_CTRL_HUM  0xF2
#define BME280_REG_STATUS    0xF3  // bit3 = measuring
#define BME280_REG_CTRL_MEAS 0xF4
#define BME280_REG_CONFIG    0xF5
#define BME280_REG_DATA      0xF7  // 0xF7..0xF9 pression, 0xFA..0xFC temp, 0xFD..0xFE hum

// --- Registres BME680 ---
#define BME680_REG_STATUS    0x1D  // bit7 = new_data
#define BME680_REG_DATA      0x1F  // 0x1F..0x21 pression, 0x22..0x24 temp, 0x25..0x26 hum
#define BME680_REG_GAS_0     0x70
#define BME680_REG_GAS_1     0x71
#define BME680_REG_CTRL_HUM  0x72
#define BME680_REG_CTRL_MEAS 0x74
#define BME680_REG_CONFIG    0x75
#define BME680_REG_CALIB1    0x89  // 25 octets
#define BME680_REG_CALIB2    0xE1  // 16 octets

typedef struct
{
    uint16_t t1; int16_t t2; int16_t t3;
    uint16_t p1; int16_t p2; int16_t p3; int16_t p4; int16_t p5;
    int16_t p6; int16_t p7; int16_t p8; int16_t p9; uint8_t p10;
    uint16_t h1; int16_t h2; int16_t h3; int16_t h4; int16_t h5; int16_t h6; int16_t h7;
} Bosch_Calib;

static uint16_t sensor_addr = 0;
static uint8_t chip_id = 0;
static uint8_t found_id = 0; // Dernier chip ID lu (diagnostic)
static Bosch_Calib calib;
static BME680_Status last_status = BME680_ERR_NOT_FOUND;

static bool read_reg(uint8_t reg, uint8_t *data, uint16_t len)
{
    return HAL_I2C_Mem_Read(&GROVE_I2C_HANDLE, sensor_addr, reg, I2C_MEMADD_SIZE_8BIT,
                            data, len, 100) == HAL_OK;
}

static bool write_reg(uint8_t reg, uint8_t value)
{
    return HAL_I2C_Mem_Write(&GROVE_I2C_HANDLE, sensor_addr, reg, I2C_MEMADD_SIZE_8BIT,
                             &value, 1, 100) == HAL_OK;
}

// Enregistre l'erreur et force une nouvelle détection à la prochaine lecture
static bool fail(BME680_Status status)
{
    last_status = status;
    sensor_addr = 0;
    return false;
}

/* ===================== BME280 ===================== */

static bool bme280_setup(void)
{
    uint8_t c[26];
    uint8_t h[7];

    if (!read_reg(BME280_REG_CALIB1, c, sizeof(c))) return false;
    if (!read_reg(BME280_REG_CALIB2, h, sizeof(h))) return false;

    calib.t1 = (uint16_t)((c[1] << 8) | c[0]);
    calib.t2 = (int16_t)((c[3] << 8) | c[2]);
    calib.t3 = (int16_t)((c[5] << 8) | c[4]);
    calib.p1 = (uint16_t)((c[7] << 8) | c[6]);
    calib.p2 = (int16_t)((c[9] << 8) | c[8]);
    calib.p3 = (int16_t)((c[11] << 8) | c[10]);
    calib.p4 = (int16_t)((c[13] << 8) | c[12]);
    calib.p5 = (int16_t)((c[15] << 8) | c[14]);
    calib.p6 = (int16_t)((c[17] << 8) | c[16]);
    calib.p7 = (int16_t)((c[19] << 8) | c[18]);
    calib.p8 = (int16_t)((c[21] << 8) | c[20]);
    calib.p9 = (int16_t)((c[23] << 8) | c[22]);
    calib.h1 = c[25];
    calib.h2 = (int16_t)((h[1] << 8) | h[0]);
    calib.h3 = h[2];
    calib.h4 = (int16_t)(((int8_t)h[3] << 4) | (h[4] & 0x0F));
    calib.h5 = (int16_t)(((int8_t)h[5] << 4) | (h[4] >> 4));
    calib.h6 = (int8_t)h[6];

    if (calib.t1 == 0 || calib.p1 == 0) return false;

    return write_reg(BME280_REG_CONFIG, 0x00); // Pas de filtre IIR
}

static bool bme280_measure(Env_Data *data)
{
    uint8_t raw[8];

    if (!write_reg(BME280_REG_CTRL_HUM, CTRL_HUM_VALUE) ||
        !write_reg(BME280_REG_CTRL_MEAS, CTRL_MEAS_VALUE))
        return fail(BME680_ERR_I2C);

    // Attente de la fin de conversion (bit measuring retombé)
    uint32_t start = HAL_GetTick();
    do
    {
        HAL_Delay(10);
        if (!read_reg(BME280_REG_STATUS, raw, 1))
            return fail(BME680_ERR_I2C);
        if (HAL_GetTick() - start > MEAS_TIMEOUT_MS)
        {
            last_status = BME680_ERR_TIMEOUT;
            return false;
        }
    } while (raw[0] & 0x08);

    if (!read_reg(BME280_REG_DATA, raw, 8))
        return fail(BME680_ERR_I2C);

    double adc_p = (double)(((uint32_t)raw[0] << 12) | ((uint32_t)raw[1] << 4) | (raw[2] >> 4));
    double adc_t = (double)(((uint32_t)raw[3] << 12) | ((uint32_t)raw[4] << 4) | (raw[5] >> 4));
    double adc_h = (double)(((uint16_t)raw[6] << 8) | raw[7]);

    // Température (°C)
    double var1 = (adc_t / 16384.0 - calib.t1 / 1024.0) * calib.t2;
    double var2 = adc_t / 131072.0 - calib.t1 / 8192.0;
    var2 = var2 * var2 * calib.t3;
    double t_fine = var1 + var2;
    double temp = t_fine / 5120.0;

    // Pression (Pa)
    double pres = 0.0;
    var1 = t_fine / 2.0 - 64000.0;
    var2 = var1 * var1 * calib.p6 / 32768.0;
    var2 = var2 + var1 * calib.p5 * 2.0;
    var2 = var2 / 4.0 + calib.p4 * 65536.0;
    var1 = (calib.p3 * var1 * var1 / 524288.0 + calib.p2 * var1) / 524288.0;
    var1 = (1.0 + var1 / 32768.0) * calib.p1;
    if (var1 != 0.0)
    {
        pres = 1048576.0 - adc_p;
        pres = (pres - var2 / 4096.0) * 6250.0 / var1;
        var1 = calib.p9 * pres * pres / 2147483648.0;
        var2 = pres * calib.p8 / 32768.0;
        pres = pres + (var1 + var2 + calib.p7) / 16.0;
    }

    // Humidité (%)
    double hum = t_fine - 76800.0;
    hum = (adc_h - (calib.h4 * 64.0 + calib.h5 / 16384.0 * hum)) *
          (calib.h2 / 65536.0 * (1.0 + calib.h6 / 67108864.0 * hum *
                                 (1.0 + calib.h3 / 67108864.0 * hum)));
    hum = hum * (1.0 - calib.h1 * hum / 524288.0);
    if (hum > 100.0) hum = 100.0;
    if (hum < 0.0)   hum = 0.0;

    data->temperature_c = (float)temp;
    data->pressure_hpa = (float)(pres / 100.0);
    data->humidity_percent = (float)hum;
    data->gas_resistance_ohm = 0.0f;
    return true;
}

/* ===================== BME680 ===================== */

static bool bme680_setup(void)
{
    uint8_t c[41];

    if (!read_reg(BME680_REG_CALIB1, c, 25)) return false;
    if (!read_reg(BME680_REG_CALIB2, c + 25, 16)) return false;

    // Indices issus de l'API Bosch BME680 (bloc 0x89 puis bloc 0xE1)
    calib.t2  = (int16_t)((c[2] << 8) | c[1]);
    calib.t3  = (int8_t)c[3];
    calib.p1  = (uint16_t)((c[6] << 8) | c[5]);
    calib.p2  = (int16_t)((c[8] << 8) | c[7]);
    calib.p3  = (int8_t)c[9];
    calib.p4  = (int16_t)((c[12] << 8) | c[11]);
    calib.p5  = (int16_t)((c[14] << 8) | c[13]);
    calib.p7  = (int8_t)c[15];
    calib.p6  = (int8_t)c[16];
    calib.p8  = (int16_t)((c[20] << 8) | c[19]);
    calib.p9  = (int16_t)((c[22] << 8) | c[21]);
    calib.p10 = c[23];
    calib.h2  = (int16_t)((c[25] << 4) | (c[26] >> 4));
    calib.h1  = (uint16_t)((c[27] << 4) | (c[26] & 0x0F));
    calib.h3  = (int8_t)c[28];
    calib.h4  = (int8_t)c[29];
    calib.h5  = (int8_t)c[30];
    calib.h6  = c[31];
    calib.h7  = (int8_t)c[32];
    calib.t1  = (uint16_t)((c[34] << 8) | c[33]);

    if (calib.t1 == 0 || calib.p1 == 0) return false;

    // Pas de filtre IIR, chauffage du capteur de gaz coupé
    return write_reg(BME680_REG_CONFIG, 0x00) &&
           write_reg(BME680_REG_GAS_0, 0x08) &&
           write_reg(BME680_REG_GAS_1, 0x00);
}

static bool bme680_measure(Env_Data *data)
{
    uint8_t raw[8];

    if (!write_reg(BME680_REG_CTRL_HUM, CTRL_HUM_VALUE) ||
        !write_reg(BME680_REG_CTRL_MEAS, CTRL_MEAS_VALUE))
        return fail(BME680_ERR_I2C);

    // Attente de la fin de conversion (bit new_data)
    uint32_t start = HAL_GetTick();
    do
    {
        HAL_Delay(5);
        if (!read_reg(BME680_REG_STATUS, raw, 1))
            return fail(BME680_ERR_I2C);
        if (HAL_GetTick() - start > MEAS_TIMEOUT_MS)
        {
            last_status = BME680_ERR_TIMEOUT;
            return false;
        }
    } while ((raw[0] & 0x80) == 0);

    if (!read_reg(BME680_REG_DATA, raw, 8))
        return fail(BME680_ERR_I2C);

    double adc_p = (double)(((uint32_t)raw[0] << 12) | ((uint32_t)raw[1] << 4) | (raw[2] >> 4));
    double adc_t = (double)(((uint32_t)raw[3] << 12) | ((uint32_t)raw[4] << 4) | (raw[5] >> 4));
    double adc_h = (double)(((uint16_t)raw[6] << 8) | raw[7]);

    // Température (°C)
    double var1 = (adc_t / 16384.0 - calib.t1 / 1024.0) * calib.t2;
    double var2 = adc_t / 131072.0 - calib.t1 / 8192.0;
    var2 = var2 * var2 * (calib.t3 * 16.0);
    double t_fine = var1 + var2;
    double temp = t_fine / 5120.0;

    // Pression (Pa)
    var1 = t_fine / 2.0 - 64000.0;
    var2 = var1 * var1 * (calib.p6 / 131072.0);
    var2 = var2 + var1 * calib.p5 * 2.0;
    var2 = var2 / 4.0 + calib.p4 * 65536.0;
    var1 = (calib.p3 * var1 * var1 / 16384.0 + calib.p2 * var1) / 524288.0;
    var1 = (1.0 + var1 / 32768.0) * calib.p1;
    double pres = 0.0;
    if (var1 != 0.0)
    {
        pres = 1048576.0 - adc_p;
        pres = (pres - var2 / 4096.0) * 6250.0 / var1;
        var1 = calib.p9 * pres * pres / 2147483648.0;
        var2 = pres * (calib.p8 / 32768.0);
        double var3 = (pres / 256.0) * (pres / 256.0) * (pres / 256.0) * (calib.p10 / 131072.0);
        pres = pres + (var1 + var2 + var3 + calib.p7 * 128.0) / 16.0;
    }

    // Humidité (%)
    var1 = adc_h - (calib.h1 * 16.0 + calib.h3 / 2.0 * temp);
    var2 = var1 * (calib.h2 / 262144.0 *
                   (1.0 + calib.h4 / 16384.0 * temp + calib.h5 / 1048576.0 * temp * temp));
    double var3 = calib.h6 / 16384.0;
    double var4 = calib.h7 / 2097152.0;
    double hum = var2 + (var3 + var4 * temp) * var2 * var2;
    if (hum > 100.0) hum = 100.0;
    if (hum < 0.0)   hum = 0.0;

    data->temperature_c = (float)temp;
    data->pressure_hpa = (float)(pres / 100.0);
    data->humidity_percent = (float)hum;
    data->gas_resistance_ohm = 0.0f; // Capteur de gaz non utilisé
    return true;
}

/* ===================== API publique ===================== */

bool GroveBME680_ReadChipId(uint8_t *id)
{
    if (id == 0 || sensor_addr == 0) return false;
    return read_reg(REG_CHIP_ID, id, 1);
}

bool GroveBME680_Init(void)
{
    uint8_t id = 0;

    // Adresse Seeed : 0x76 par défaut, parfois 0x77
    if (HAL_I2C_IsDeviceReady(&GROVE_I2C_HANDLE, 0x76 << 1, 3, 100) == HAL_OK)
        sensor_addr = 0x76 << 1;
    else if (HAL_I2C_IsDeviceReady(&GROVE_I2C_HANDLE, 0x77 << 1, 3, 100) == HAL_OK)
        sensor_addr = 0x77 << 1;
    else
        return fail(BME680_ERR_NOT_FOUND);

    if (!GroveBME680_ReadChipId(&id))
        return fail(BME680_ERR_I2C);
    found_id = id;
    if (id != BME280_CHIP_ID && id != BME680_CHIP_ID)
        return fail(BME680_ERR_BAD_CHIP_ID);
    chip_id = id;

    write_reg(REG_RESET, SOFT_RESET);
    HAL_Delay(10);

    bool ok = (chip_id == BME280_CHIP_ID) ? bme280_setup() : bme680_setup();
    if (!ok)
        return fail(BME680_ERR_CALIB);

    last_status = BME680_OK;
    return true;
}

bool GroveBME680_ReadEnvironment(Env_Data *data)
{
    if (data == 0) return false;

    // Capteur absent au démarrage (ou débranché) : on retente la détection
    if (sensor_addr == 0 && !GroveBME680_Init())
        return false;

    bool ok = (chip_id == BME280_CHIP_ID) ? bme280_measure(data) : bme680_measure(data);
    if (ok)
        last_status = BME680_OK;
    return ok;
}

BME680_Status GroveBME680_GetLastStatus(void)
{
    return last_status;
}

const char *GroveBME680_StatusToString(BME680_Status status)
{
    switch (status)
    {
        case BME680_OK:              return "OK";
        case BME680_ERR_NOT_FOUND:   return "aucune reponse en 0x76/0x77 (cablage, port I2C, alimentation)";
        case BME680_ERR_BAD_CHIP_ID: return "chip ID inconnu (ni BME280 0x60, ni BME680 0x61)";
        case BME680_ERR_CALIB:       return "lecture calibration impossible";
        case BME680_ERR_I2C:         return "erreur de communication I2C";
        case BME680_ERR_TIMEOUT:     return "mesure non terminee (timeout)";
    }
    return "inconnu";
}

const char *GroveBME680_GetChipName(void)
{
    if (chip_id == BME280_CHIP_ID) return "BME280";
    if (chip_id == BME680_CHIP_ID) return "BME680";
    return "?";
}

uint8_t GroveBME680_GetFoundChipId(void)
{
    return found_id;
}

uint8_t GroveBME680_GetAddress(void)
{
    return (uint8_t)(sensor_addr >> 1);
}
