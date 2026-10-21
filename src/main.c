#include <stdio.h>
#include <stdlib.h>
#include "bme280.h"
 
BME280_Handle bme;
BME280_Data   env;
 
extern I2C_HandleTypeDef hi2c1;


int main(void)
{
    HAL_Init();
    // ... config horloge, GPIO, I2C ...
 
    // Initialiser le capteur (lit la calibration, configure l'oversampling)
    if (!BME280_Init(&bme, &hi2c1)) {
        // Le capteur ne répond pas — vérifier le câblage I2C
        while (1);
    }
 
    while (1) {
        if (BME280_Read(&bme, &env)) {
            // env.temperature_c   → ex: 23.45
            // env.humidity_percent → ex: 48.2
            // env.pressure_hpa    → ex: 1013.25
            printf("Temp: %.2f C  Hum: %.2f %%  Pression: %.2f hPa\n",
                   env.temperature_c, env.humidity_percent, env.pressure_hpa);
        }
        HAL_Delay(2000);
    }
}