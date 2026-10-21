#include "system_status.h"
#include "grove_rgb.h"
#include "modes.h"

#define BLINK_PERIOD_MS 1000 // Clignotement à 1 Hz

// Code d'erreur : couleur alternée avec le rouge, et durée relative à celle du rouge
typedef struct {
    uint8_t r, g, b;
    uint8_t ratio; // 1 = durées identiques, 2 = couleur 2x plus longue que le rouge
} ErrorCode;

void SystemStatus_Init(SystemStatus *status) {
    status->last_toggle_tick = 0;
    status->toggle_state = 0;
    status->led_valid = 0;
}

// Envoie la couleur à la LED uniquement si elle a changé
static void set_color(SystemStatus *status, uint8_t r, uint8_t g, uint8_t b) {
    if (status->led_valid && status->led_r == r && status->led_g == g && status->led_b == b) {
        return;
    }

    GroveRGB_SetColor(r, g, b);
    status->led_r = r;
    status->led_g = g;
    status->led_b = b;
    status->led_valid = 1;
}

// Erreur la plus prioritaire (ordre du tableau du cahier des charges), false si aucune
static bool get_error(SystemHealth health, ErrorCode *code) {
    if (!health.rtc_ok)      { *code = (ErrorCode){ 0,   0,   255, 1 }; return true; } // Bleu
    if (!health.gps_ok)      { *code = (ErrorCode){ 255, 255, 0,   1 }; return true; } // Jaune
    if (!health.sensor_ok)   { *code = (ErrorCode){ 0,   255, 0,   1 }; return true; } // Vert
    if (!health.data_ok)     { *code = (ErrorCode){ 0,   255, 0,   2 }; return true; } // Vert x2
    if (!health.sd_space_ok) { *code = (ErrorCode){ 255, 255, 255, 1 }; return true; } // Blanc
    if (!health.sd_ok)       { *code = (ErrorCode){ 255, 255, 255, 2 }; return true; } // Blanc x2
    return false;
}

void SystemStatus_Update(SystemStatus *status, SystemMode mode, SystemHealth health) {
    ErrorCode code;

    // 1. GESTION DES ERREURS : alternance rouge / couleur de la panne sur 1 s
    if (get_error(health, &code)) {
        uint32_t current_tick = HAL_GetTick();
        uint32_t red_ms = BLINK_PERIOD_MS / (1 + code.ratio);
        uint32_t phase_ms = status->toggle_state ? red_ms : (BLINK_PERIOD_MS - red_ms);

        if (current_tick - status->last_toggle_tick >= phase_ms) {
            status->last_toggle_tick = current_tick;
            status->toggle_state = !status->toggle_state;
        }

        if (status->toggle_state) {
            set_color(status, 255, 0, 0);            // Phase 1 : Toujours ROUGE
        } else {
            set_color(status, code.r, code.g, code.b); // Phase 2 : Couleur de la panne
        }
        return;
    }

    // 2. FONCTIONNEMENT NOMINAL : Couleur fixe selon le mode actif
    switch (mode) {
        case MODE_STANDARD:      set_color(status, 0, 255, 0);   break; // Vert
        case MODE_CONFIGURATION: set_color(status, 255, 255, 0); break; // Jaune
        case MODE_ECONOMIQUE:    set_color(status, 0, 0, 255);   break; // Bleu
        case MODE_MAINTENANCE:   set_color(status, 255, 165, 0); break; // Orange
    }
}
