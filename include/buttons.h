#ifndef BUTTONS_H
#define BUTTONS_H

#include "stm32l4xx_hal.h"
#include "modes.h"

// Durée de maintien requise (5 secondes = 5000 ms)
#define LONG_PRESS_TIME_MS 5000
#define CONFIGURATION_TIMEOUT_MS (30UL * 60UL * 1000UL)

// Initialisation des broches GPIO pour le Port D7 Grove (PA8 et PA9)
void buttons_init(void);

// Test du bouton rouge au démarrage (Mode Configuration)
void check_boot_mode(void);

// Gestion des transitions de modes (appuis 5s)
void process_button_presses(void);

// Activité en mode configuration (commande reçue) : relance le délai de 30 min
void buttons_config_activity(void);

// Annonce du mode actif sur le moniteur série (la LED est gérée par system_status)
void print_mode(SystemMode mode);

#endif // BUTTONS_H
