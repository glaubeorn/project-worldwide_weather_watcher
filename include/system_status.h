#ifndef SYSTEM_STATUS_H
#define SYSTEM_STATUS_H

#include "stm32l4xx_hal.h"
#include "cesi_types.h"
#include "modes.h"

// État de santé des sous-systèmes (1 = OK, 0 = Erreur), par ordre de priorité
typedef struct {
    uint8_t rtc_ok;      // 0 = Rouge / Bleu (erreur d'accès à l'horloge RTC)
    uint8_t gps_ok;      // 0 = Rouge / Jaune (erreur d'accès aux données du GPS)
    uint8_t sensor_ok;   // 0 = Rouge / Vert (erreur d'accès aux données d'un capteur)
    uint8_t data_ok;     // 0 = Rouge / Vert, vert 2x plus long (données incohérentes)
    uint8_t sd_space_ok; // 0 = Rouge / Blanc (carte SD pleine)
    uint8_t sd_ok;       // 0 = Rouge / Blanc, blanc 2x plus long (erreur d'accès ou d'écriture SD)
} SystemHealth;

// Structure de suivi pour le clignotement non-bloquant de la LED
typedef struct {
    uint32_t last_toggle_tick;
    uint8_t toggle_state;  // 1 = phase rouge affichée
    uint8_t led_valid;     // 0 = couleur de la LED inconnue (forcer l'envoi)
    uint8_t led_r, led_g, led_b; // Dernière couleur envoyée à la LED
} SystemStatus;

// Prototypes des fonctions publiques
void SystemStatus_Init(SystemStatus *status);
void SystemStatus_Update(SystemStatus *status, SystemMode mode, SystemHealth health);

#endif // SYSTEM_STATUS_H
