// =============================================================================
// bme280.c — Driver BME280 pour STM32 HAL (I2C)
// =============================================================================
// Implémente la lecture réelle du capteur BME280 via I2C.
// Les formules de compensation sont issues de la datasheet Bosch BME280
// (document BST-BME280-DS002, section 4.2.3 "Compensation formulas").
// =============================================================================

#include "bme280.h"

// Timeout I2C en millisecondes
#define BME280_TIMEOUT  100

// =============================================================================
// Fonctions I2C bas niveau
// =============================================================================

// Lire un seul registre du capteur
static bool BME280_ReadReg(BME280_Handle *dev, uint8_t reg, uint8_t *value)
{
    // HAL_I2C_Mem_Read lit un registre d'un périphérique I2C :
    //   - dev->hi2c        : handle I2C (I2C1, I2C2, etc.)
    //   - BME280_I2C_ADDR  : adresse du capteur (déjà décalée pour HAL)
    //   - reg              : adresse du registre à lire
    //   - I2C_MEMADD_SIZE_8BIT : le registre est sur 8 bits
    //   - value            : buffer de réception
    //   - 1                : nombre d'octets à lire
    //   - BME280_TIMEOUT   : timeout en ms
    return HAL_I2C_Mem_Read(dev->hi2c, BME280_I2C_ADDR,
                            reg, I2C_MEMADD_SIZE_8BIT,
                            value, 1, BME280_TIMEOUT) == HAL_OK;
}

// Lire plusieurs registres consécutifs
static bool BME280_ReadRegs(BME280_Handle *dev, uint8_t reg, uint8_t *buf, uint16_t len)
{
    return HAL_I2C_Mem_Read(dev->hi2c, BME280_I2C_ADDR,
                            reg, I2C_MEMADD_SIZE_8BIT,
                            buf, len, BME280_TIMEOUT) == HAL_OK;
}

// Écrire un seul registre
static bool BME280_WriteReg(BME280_Handle *dev, uint8_t reg, uint8_t value)
{
    return HAL_I2C_Mem_Write(dev->hi2c, BME280_I2C_ADDR,
                             reg, I2C_MEMADD_SIZE_8BIT,
                             &value, 1, BME280_TIMEOUT) == HAL_OK;
}

// =============================================================================
// Lecture des coefficients de calibration
// =============================================================================
// Chaque capteur BME280 a des coefficients uniques, calibrés en usine et
// stockés dans des registres internes. Ils sont lus une seule fois à l'init.
//
// Registres 0x88 à 0xA1 (26 octets) : coefficients T1-T3, P1-P9
// Registres 0xE1 à 0xE7 (7 octets)  : coefficients H2-H6
// Registre  0xA1 (1 octet)          : coefficient H1

static bool BME280_ReadCalibration(BME280_Handle *dev)
{
    uint8_t buf[26];
    BME280_CalibData *c = &dev->calib;

    // --- Bloc 1 : température et pression (0x88 - 0xA1, 26 octets) ---
    if (!BME280_ReadRegs(dev, BME280_REG_CALIB_00, buf, 26))
        return false;

    // Les coefficients sont stockés en little-endian (LSB first)
    // Exemple : dig_T1 = buf[0] (LSB) | buf[1] (MSB) << 8
    c->dig_T1 = (uint16_t)(buf[0]  | (buf[1] << 8));
    c->dig_T2 = (int16_t) (buf[2]  | (buf[3] << 8));
    c->dig_T3 = (int16_t) (buf[4]  | (buf[5] << 8));

    c->dig_P1 = (uint16_t)(buf[6]  | (buf[7] << 8));
    c->dig_P2 = (int16_t) (buf[8]  | (buf[9] << 8));
    c->dig_P3 = (int16_t) (buf[10] | (buf[11] << 8));
    c->dig_P4 = (int16_t) (buf[12] | (buf[13] << 8));
    c->dig_P5 = (int16_t) (buf[14] | (buf[15] << 8));
    c->dig_P6 = (int16_t) (buf[16] | (buf[17] << 8));
    c->dig_P7 = (int16_t) (buf[18] | (buf[19] << 8));
    c->dig_P8 = (int16_t) (buf[20] | (buf[21] << 8));
    c->dig_P9 = (int16_t) (buf[22] | (buf[23] << 8));

    // dig_H1 est à l'adresse 0xA1 (= buf[25] du bloc 0x88)
    c->dig_H1 = buf[25];

    // --- Bloc 2 : humidité (0xE1 - 0xE7, 7 octets) ---
    uint8_t hbuf[7];
    if (!BME280_ReadRegs(dev, BME280_REG_CALIB_26, hbuf, 7))
        return false;

    c->dig_H2 = (int16_t)(hbuf[0] | (hbuf[1] << 8));
    c->dig_H3 = hbuf[2];
    // dig_H4 et dig_H5 partagent un octet (hbuf[3], hbuf[4], hbuf[5])
    // dig_H4 = hbuf[3] << 4 | (hbuf[4] & 0x0F)
    // dig_H5 = hbuf[5] << 4 | (hbuf[4] >> 4)
    c->dig_H4 = (int16_t)((hbuf[3] << 4) | (hbuf[4] & 0x0F));
    c->dig_H5 = (int16_t)((hbuf[5] << 4) | (hbuf[4] >> 4));
    c->dig_H6 = (int8_t)hbuf[6];

    return true;
}

// =============================================================================
// Formules de compensation Bosch (datasheet BST-BME280-DS002, section 4.2.3)
// =============================================================================
// Ces formules convertissent les valeurs brutes ADC en unités physiques.
// Elles utilisent les coefficients de calibration lus à l'init.
//
// La température est compensée en premier car elle produit "t_fine",
// une valeur intermédiaire nécessaire au calcul de la pression et de l'humidité.

// Compensation température → résultat en °C (× 100 en entier, ex: 2345 = 23.45 °C)
static int32_t BME280_CompensateTemp(BME280_Handle *dev, int32_t adc_T)
{
    BME280_CalibData *c = &dev->calib;

    int32_t var1 = ((((adc_T >> 3) - ((int32_t)c->dig_T1 << 1))) *
                    ((int32_t)c->dig_T2)) >> 11;

    int32_t var2 = (((((adc_T >> 4) - ((int32_t)c->dig_T1)) *
                      ((adc_T >> 4) - ((int32_t)c->dig_T1))) >> 12) *
                    ((int32_t)c->dig_T3)) >> 14;

    // t_fine est partagé avec les compensations pression et humidité
    dev->t_fine = var1 + var2;

    // Résultat en centièmes de degré (ex: 2345 = 23.45 °C)
    return (dev->t_fine * 5 + 128) >> 8;
}

// Compensation pression → résultat en Pa (ex: 101325 = 1013.25 hPa)
static uint32_t BME280_CompensatePress(BME280_Handle *dev, int32_t adc_P)
{
    BME280_CalibData *c = &dev->calib;

    int64_t var1 = ((int64_t)dev->t_fine) - 128000;
    int64_t var2 = var1 * var1 * (int64_t)c->dig_P6;
    var2 = var2 + ((var1 * (int64_t)c->dig_P5) << 17);
    var2 = var2 + (((int64_t)c->dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)c->dig_P3) >> 8) +
           ((var1 * (int64_t)c->dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)c->dig_P1) >> 33;

    if (var1 == 0) return 0;  // Éviter la division par zéro

    int64_t p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)c->dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)c->dig_P8) * p) >> 19;

    p = ((p + var1 + var2) >> 8) + (((int64_t)c->dig_P7) << 4);

    // Résultat en Pa × 256 → diviser par 256 pour avoir des Pa
    return (uint32_t)(p >> 8);
}

// Compensation humidité → résultat en % × 1024 (ex: 51200 = 50.0 %)
static uint32_t BME280_CompensateHum(BME280_Handle *dev, int32_t adc_H)
{
    BME280_CalibData *c = &dev->calib;

    int32_t v = dev->t_fine - ((int32_t)76800);

    v = (((((adc_H << 14) - (((int32_t)c->dig_H4) << 20) -
            (((int32_t)c->dig_H5) * v)) + ((int32_t)16384)) >> 15) *
         (((((((v * ((int32_t)c->dig_H6)) >> 10) *
              (((v * ((int32_t)c->dig_H3)) >> 11) +
               ((int32_t)32768))) >> 10) +
            ((int32_t)2097152)) * ((int32_t)c->dig_H2) + 8192) >> 14));

    v = (v - (((((v >> 15) * (v >> 15)) >> 7) * ((int32_t)c->dig_H1)) >> 4));
    v = (v < 0) ? 0 : v;
    v = (v > 419430400) ? 419430400 : v;

    // Résultat en % × 1024 (ex: 51200 = 50.0 %)
    return (uint32_t)(v >> 12);
}

// =============================================================================
// Fonctions publiques
// =============================================================================

bool BME280_Init(BME280_Handle *dev, I2C_HandleTypeDef *hi2c)
{
    dev->hi2c = hi2c;
    dev->t_fine = 0;

    // 1. Vérifier que le capteur répond et retourne le bon chip ID
    uint8_t chip_id = 0;
    if (!BME280_ReadReg(dev, BME280_REG_CHIP_ID, &chip_id))
        return false;

    if (chip_id != BME280_CHIP_ID)
        return false;  // Ce n'est pas un BME280

    // 2. Soft reset du capteur
    BME280_WriteReg(dev, BME280_REG_RESET, 0xB6);
    HAL_Delay(10);  // Attendre la fin du reset

    // 3. Attendre que le capteur ait fini de charger sa calibration (NVM)
    uint8_t status = 0;
    do {
        BME280_ReadReg(dev, BME280_REG_STATUS, &status);
        HAL_Delay(1);
    } while (status & 0x01);  // Bit 0 = im_update (NVM en cours de copie)

    // 4. Lire les coefficients de calibration
    if (!BME280_ReadCalibration(dev))
        return false;

    // 5. Configurer le filtre IIR (coefficient 4) et le standby
    //    Config register (0xF5) : t_sb[7:5]=000, filter[4:2]=010, spi3w_en=0
    //    → filtre IIR coefficient 4 (lisse les mesures de pression)
    BME280_WriteReg(dev, BME280_REG_CONFIG, 0x08);

    // 6. Configurer l'oversampling humidité (×1)
    //    Ctrl_hum (0xF2) : osrs_h[2:0] = 001
    //    IMPORTANT : doit être écrit AVANT ctrl_meas pour prendre effet
    BME280_WriteReg(dev, BME280_REG_CTRL_HUM, 0x01);

    // 7. Configurer oversampling température (×2) et pression (×16), mode sleep
    //    Ctrl_meas (0xF4) : osrs_t[7:5]=010, osrs_p[4:2]=101, mode[1:0]=00
    //    On reste en sleep — la mesure sera déclenchée dans BME280_Read()
    BME280_WriteReg(dev, BME280_REG_CTRL_MEAS, 0x54);

    return true;
}

bool BME280_Read(BME280_Handle *dev, BME280_Data *data)
{
    // 1. Déclencher une mesure en MODE FORCÉ
    //    On réécrit ctrl_meas avec mode=01 (forced) en gardant l'oversampling
    //    En mode forcé, le capteur fait UNE mesure puis retourne en sleep
    //    (économise de l'énergie par rapport au mode normal)
    BME280_WriteReg(dev, BME280_REG_CTRL_MEAS, 0x55);  // 0x54 | 0x01

    // 2. Attendre la fin de la mesure (bit 3 du registre status = measuring)
    uint8_t status;
    uint8_t retries = 50;
    do {
        HAL_Delay(2);
        if (!BME280_ReadReg(dev, BME280_REG_STATUS, &status))
            return false;
        retries--;
    } while ((status & 0x08) && retries > 0);  // Bit 3 = mesure en cours

    if (retries == 0)
        return false;  // Timeout — le capteur ne répond plus

    // 3. Lire les 8 octets de données brutes (0xF7 à 0xFE)
    //    Format : press[19:12] press[11:4] press[3:0]xxxx
    //             temp[19:12]  temp[11:4]  temp[3:0]xxxx
    //             hum[15:8]    hum[7:0]
    uint8_t raw[8];
    if (!BME280_ReadRegs(dev, BME280_REG_DATA, raw, 8))
        return false;

    // 4. Assembler les valeurs brutes ADC (20 bits pour T et P, 16 bits pour H)
    int32_t adc_P = ((int32_t)raw[0] << 12) | ((int32_t)raw[1] << 4) | (raw[2] >> 4);
    int32_t adc_T = ((int32_t)raw[3] << 12) | ((int32_t)raw[4] << 4) | (raw[5] >> 4);
    int32_t adc_H = ((int32_t)raw[6] << 8)  |  (int32_t)raw[7];

    // 5. Appliquer les formules de compensation Bosch
    //    IMPORTANT : la température doit être compensée EN PREMIER
    //    car elle calcule t_fine, utilisé par la pression et l'humidité
    int32_t temp_raw = BME280_CompensateTemp(dev, adc_T);
    data->temperature_c = (float)temp_raw / 100.0f;

    uint32_t press_raw = BME280_CompensatePress(dev, adc_P);
    data->pressure_hpa = (float)press_raw / 100.0f;

    uint32_t hum_raw = BME280_CompensateHum(dev, adc_H);
    data->humidity_percent = (float)hum_raw / 1024.0f;

    return true;
}
