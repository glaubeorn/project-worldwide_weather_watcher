#ifndef GROVE_BME680_H
#define GROVE_BME680_H

#include <stdbool.h>
#include <stdint.h>
#include "cesi_types.h"

/*
 * Capteur température / pression / humidité Bosch.
 * Gère le BME280 (monté sur le prototype) et le BME680, détectés par chip ID.
 */
#define BME280_CHIP_ID 0x60
#define BME680_CHIP_ID 0x61

// Cause de la dernière erreur (affichable sur le port série)
typedef enum
{
    BME680_OK = 0,
    BME680_ERR_NOT_FOUND,   // Aucune réponse en 0x76 ni 0x77 (câblage / alimentation)
    BME680_ERR_BAD_CHIP_ID, // Un composant répond mais ce n'est pas un BME680
    BME680_ERR_CALIB,       // Lecture des coefficients de calibration impossible
    BME680_ERR_I2C,         // Erreur de communication pendant une mesure
    BME680_ERR_TIMEOUT      // La mesure ne s'est jamais terminée
} BME680_Status;

bool GroveBME680_Init(void);
bool GroveBME680_ReadChipId(uint8_t *chip_id);
bool GroveBME680_ReadEnvironment(Env_Data *data);

BME680_Status GroveBME680_GetLastStatus(void);
const char *GroveBME680_StatusToString(BME680_Status status);
uint8_t GroveBME680_GetAddress(void); // Adresse 7 bits détectée (0 si absent)
const char *GroveBME680_GetChipName(void); // "BME280", "BME680" ou "?"
uint8_t GroveBME680_GetFoundChipId(void); // Dernier chip ID lu (0x61 = BME680, 0x60 = BME280, 0x58 = BMP280)

#endif
