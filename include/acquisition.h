#ifndef ACQUISITION_H
#define ACQUISITION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "cesi_types.h"

typedef enum {
    LIGHT_LEVEL_NA = 0,
    LIGHT_LEVEL_LOW,     // < LUMIN_LOW
    LIGHT_LEVEL_MEDIUM,  // entre LUMIN_LOW et LUMIN_HIGH
    LIGHT_LEVEL_HIGH     // > LUMIN_HIGH
} LightLevel;

// Une ligne de mesures horodatée (champ *_valid = false -> "NA")
typedef struct {
    bool time_valid;
    RTC_DateTime time;

    bool temp_valid;
    float temperature_c;

    bool hum_valid;
    float humidity_percent;

    bool pres_valid;
    float pressure_hpa;

    bool light_valid;
    uint16_t light;          // 0-1023
    LightLevel light_level;

    bool gps_skipped;        // GPS non lu (mode économique : une mesure sur deux)
    bool gps_responded;      // Trame GPS reçue pendant l'acquisition
    bool gps_valid;          // Position disponible (fix GPS)
    double latitude;         // Degrés décimaux (négatif = Sud)
    double longitude;        // Degrés décimaux (négatif = Ouest)
    uint8_t satellites;
} Measurement;

// Bilan de santé mis à jour à la fin de chaque acquisition
typedef struct {
    bool rtc_ok;     // Horloge RTC lue
    bool gps_ok;     // GPS en erreur après 2 mesures consécutives sans trame
    bool sensor_ok;  // Aucun capteur en erreur (2 mesures consécutives en timeout)
    bool data_ok;    // Données dans les seuils (MIN/MAX_TEMP_AIR, PRESSURE_MIN/MAX)
} AcquisitionHealth;

// Lance une acquisition (non bloquant) ; with_gps = false : GPS non lu
void Acquisition_Start(bool with_gps);

// À appeler dans la boucle principale. Retourne true quand l'acquisition est
// terminée : *m contient alors la mesure. Un capteur qui ne répond pas est
// réinterrogé jusqu'à TIMEOUT, puis sa donnée vaut "NA".
bool Acquisition_Poll(Measurement *m);

bool Acquisition_IsRunning(void);
AcquisitionHealth Acquisition_GetHealth(void);

// Affichage lisible sur le moniteur série
void Acquisition_Print(const Measurement *m);

// Ligne pour le fichier de log SD : AAAA-MM-JJ HH:MM:SS;TEMP;HUM;PRES;LUMIN;NIVEAU;LAT;LON
const char *Acquisition_HeaderLine(void);
void Acquisition_FormatLine(const Measurement *m, char *buf, size_t len);

#endif // ACQUISITION_H
