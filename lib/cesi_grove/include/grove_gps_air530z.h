#ifndef GROVE_GPS_AIR530Z_H
#define GROVE_GPS_AIR530Z_H

#include <stdbool.h>
#include <stdint.h>
#include "cesi_types.h"

/*
 * GPS Grove Air530Z (trames NMEA à 9600 bauds).
 * Réception en arrière-plan par interruption : les caractères sont stockés
 * ligne par ligne, puis GroveGPS_Process() décode les trames GGA reçues.
 */

void GroveGPS_Init(void);

// À appeler dans la boucle principale : décode la dernière ligne reçue
void GroveGPS_Process(void);

// Nombre de trames GGA valides reçues depuis le démarrage
uint32_t GroveGPS_GetGGACount(void);

// Dernière trame GGA décodée (false si aucune reçue)
bool GroveGPS_GetLast(GPS_Data *gps);

/*
 * À appeler depuis HAL_UART_RxCpltCallback / HAL_UART_ErrorCallback de
 * l'application (ces callbacks HAL sont uniques et partagés entre les UART).
 */
#include "stm32l4xx_hal.h"
void GroveGPS_UART_RxCplt(UART_HandleTypeDef *huart);
void GroveGPS_UART_Error(UART_HandleTypeDef *huart);

#endif
