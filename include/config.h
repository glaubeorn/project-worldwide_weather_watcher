#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Paramètres de la station (cahier des charges, mode configuration).
 * Sauvegardés dans la dernière page de la flash (EEPROM émulée : le
 * STM32L476 n'a pas d'EEPROM) pour être conservés après une coupure.
 */
typedef struct {
    uint16_t log_interval_min;  // LOG_INTERVAL : minutes entre 2 mesures
    uint16_t file_max_size;     // FILE_MAX_SIZE : taille max d'un fichier de log (octets)
    uint16_t timeout_s;         // TIMEOUT : abandon de l'acquisition d'un capteur (s)

    uint8_t  lumin;             // LUMIN : capteur de luminosité activé
    uint16_t lumin_low;         // LUMIN_LOW : en dessous -> luminosité "faible" (0-1023)
    uint16_t lumin_high;        // LUMIN_HIGH : au-dessus -> luminosité "forte" (0-1023)

    uint8_t  temp_air;          // TEMP_AIR : température de l'air activée
    int8_t   min_temp_air;      // MIN_TEMP_AIR : seuil bas (°C), en dessous = erreur
    int8_t   max_temp_air;      // MAX_TEMP_AIR : seuil haut (°C), au-dessus = erreur

    uint8_t  hygr;              // HYGR : hygrométrie activée
    int8_t   hygr_mint;         // HYGR_MINT : en dessous (°C), hygrométrie ignorée
    int8_t   hygr_maxt;         // HYGR_MAXT : au-dessus (°C), hygrométrie ignorée

    uint8_t  pressure;          // PRESSURE : pression atmosphérique activée
    uint16_t pressure_min;      // PRESSURE_MIN : seuil bas (hPa), en dessous = erreur
    uint16_t pressure_max;      // PRESSURE_MAX : seuil haut (hPa), au-dessus = erreur
} Config;

typedef enum {
    CONFIG_SET_OK = 0,
    CONFIG_SET_UNKNOWN,       // Paramètre inconnu
    CONFIG_SET_BAD_VALUE,     // Valeur non numérique
    CONFIG_SET_OUT_OF_RANGE,  // Hors du domaine de définition
    CONFIG_SET_INCOHERENT     // Seuil bas >= seuil haut (ex : LUMIN_LOW >= LUMIN_HIGH)
} ConfigSetResult;

extern Config g_config;

void Config_LoadDefaults(void);

// Charge les paramètres depuis la flash (false : valeurs par défaut utilisées)
bool Config_Load(void);

// Enregistre les paramètres en flash
bool Config_Save(void);

// Modifie un paramètre "NOM=valeur" en vérifiant son domaine de définition
ConfigSetResult Config_Set(const char *name, const char *value);

// Domaine de définition d'un paramètre (false si inconnu)
bool Config_GetRange(const char *name, long *min, long *max);

// Affiche tous les paramètres sur le port série (commande CONFIG)
void Config_Print(void);

#endif // CONFIG_H
