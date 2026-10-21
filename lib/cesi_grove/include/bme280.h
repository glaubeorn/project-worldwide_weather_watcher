#ifndef BME280_H
#define BME280_H

#include "stm32l4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

// =============================================================================
// bme280.h — Driver BME280 pour STM32 HAL (I2C)
// =============================================================================
// Capteur : Bosch BME280 (température, humidité, pression atmosphérique)
// Module  : Grove BME280 (compatible BME280, PAS BME680 — pas de gaz)
// Bus     : I2C — adresse 0x76 (SDO → GND) ou 0x77 (SDO → VCC)
//
// Le BME280 ne fournit PAS directement des valeurs en °C / % / hPa.
// Il fournit des valeurs brutes (ADC) qu'il faut compenser avec des
// coefficients de calibration stockés en mémoire interne du capteur.
// Ces coefficients sont uniques à chaque capteur (calibrés en usine).
//
// Processus de lecture :
//   1. Lire les coefficients de calibration (registres 0x88-0xA1 et 0xE1-0xE7)
//   2. Configurer l'oversampling et déclencher une mesure (mode "forced")
//   3. Attendre que la mesure soit prête (registre status)
//   4. Lire les données brutes (registres 0xF7-0xFE : 8 octets)
//   5. Appliquer les formules de compensation Bosch (datasheet section 4.2.3)
// =============================================================================

// Adresse I2C du BME280 (décalée à gauche de 1 bit pour HAL)
// Grove BME280 : SDO → GND → adresse 0x76 → HAL = 0xEC
// Si SDO → VCC : adresse 0x77 → HAL = 0xEE
#define BME280_I2C_ADDR     (0x76 << 1)  // = 0xEC

// Registres principaux du BME280
#define BME280_REG_CHIP_ID      0xD0    // Doit retourner 0x60
#define BME280_REG_RESET        0xE0    // Écrire 0xB6 pour un soft reset
#define BME280_REG_CTRL_HUM     0xF2    // Oversampling humidité
#define BME280_REG_STATUS       0xF3    // Bit 3 = mesure en cours
#define BME280_REG_CTRL_MEAS    0xF4    // Oversampling temp/pression + mode
#define BME280_REG_CONFIG       0xF5    // Standby, filtre IIR
#define BME280_REG_DATA         0xF7    // Début des 8 octets de données brutes

// Registres de calibration
#define BME280_REG_CALIB_00     0x88    // Calibration T et P (26 octets)
#define BME280_REG_CALIB_26     0xE1    // Calibration H (7 octets)

// Chip ID attendu
#define BME280_CHIP_ID          0x60

// Structure pour les coefficients de calibration (uniques par capteur)
// Ces valeurs sont lues une seule fois au démarrage
typedef struct {
    // Température
    uint16_t dig_T1;
    int16_t  dig_T2;
    int16_t  dig_T3;

    // Pression
    uint16_t dig_P1;
    int16_t  dig_P2;
    int16_t  dig_P3;
    int16_t  dig_P4;
    int16_t  dig_P5;
    int16_t  dig_P6;
    int16_t  dig_P7;
    int16_t  dig_P8;
    int16_t  dig_P9;

    // Humidité
    uint8_t  dig_H1;
    int16_t  dig_H2;
    uint8_t  dig_H3;
    int16_t  dig_H4;
    int16_t  dig_H5;
    int8_t   dig_H6;
} BME280_CalibData;

// Structure pour les résultats de mesure
typedef struct {
    float temperature_c;        // Température en °C
    float humidity_percent;     // Humidité relative en %
    float pressure_hpa;         // Pression atmosphérique en hPa
} BME280_Data;

// Handle du capteur (contient le bus I2C + la calibration)
typedef struct {
    I2C_HandleTypeDef *hi2c;    // Pointeur vers le handle I2C HAL
    BME280_CalibData calib;     // Coefficients de calibration
    int32_t t_fine;             // Variable intermédiaire (partagée entre T et P/H)
} BME280_Handle;

// --- Fonctions publiques ---

// Initialise le capteur : vérifie le chip ID, lit la calibration, configure
// Retourne true si le capteur répond correctement
bool BME280_Init(BME280_Handle *dev, I2C_HandleTypeDef *hi2c);

// Lit température, humidité et pression (déclenche une mesure en mode forced)
// Retourne true si la lecture a réussi
bool BME280_Read(BME280_Handle *dev, BME280_Data *data);

#endif // BME280_H
