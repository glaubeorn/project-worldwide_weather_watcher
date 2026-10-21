#include "buttons.h"
#include "modes.h"
#include "board_pins.h"
#include <stdio.h>

// ⚠️ Active LOW (1 au repos, 0 appuyé)
#define BTN_ACTIVE_LEVEL   GPIO_PIN_RESET 

typedef enum {
    BTN_STATE_RELEASED,  // Relâché
    BTN_STATE_PRESSED,   // Appui en cours
    BTN_STATE_HANDLED    // Action exécutée (verrouillé jusqu'au relâchement)
} ButtonState;

static ButtonState red_state = BTN_STATE_RELEASED;
static ButtonState green_state = BTN_STATE_RELEASED;

static uint32_t red_start_time = 0;
static uint32_t green_start_time = 0;

static SystemMode previous_mode = MODE_STANDARD;
static uint32_t configuration_last_activity = 0;

/**
 * @brief Initialisation GPIO avec PULLUP (1 au repos, 0 lors de l'appui)
 */
void buttons_init(void) {
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    GPIO_InitStruct.Pin = BTN_RED_PIN;
    HAL_GPIO_Init(BTN_RED_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = BTN_GREEN_PIN;
    HAL_GPIO_Init(BTN_GREEN_PORT, &GPIO_InitStruct);
}

/**
 * @brief Activité en mode configuration : relance le délai de 30 min
 */
void buttons_config_activity(void) {
    configuration_last_activity = HAL_GetTick();
}

/**
 * @brief Test du bouton rouge au démarrage (Mode Configuration)
 */
void check_boot_mode(void) {
    buttons_init();

    HAL_Delay(10);

    if (HAL_GPIO_ReadPin(BTN_RED_PORT, BTN_RED_PIN) == BTN_ACTIVE_LEVEL) {
        mode_actuel = MODE_CONFIGURATION;
        configuration_last_activity = HAL_GetTick();
        printf("[BOOT] Bouton Rouge enfonce au Reset -> MODE CONFIGURATION\r\n");
    } else {
        mode_actuel = MODE_STANDARD;
        printf("[BOOT] Demarrage nominal -> MODE STANDARD\r\n");
    }
    print_mode(mode_actuel);
}

/**
 * @brief Gestion des appuis de 5s pour basculer les modes
 */
void process_button_presses(void) {
    uint32_t now = HAL_GetTick();

    if (mode_actuel == MODE_CONFIGURATION &&
        (now - configuration_last_activity) >= CONFIGURATION_TIMEOUT_MS) {
        mode_actuel = MODE_STANDARD;
        print_mode(mode_actuel);
        printf("[TIMEOUT] 30 minutes sans activite -> MODE STANDARD\r\n");
    }

    // ==========================================
    // 1. BOUTON VERT (PA9) -> Entrée / sortie du mode économique
    // ==========================================
    uint8_t green_pressed = (HAL_GPIO_ReadPin(BTN_GREEN_PORT, BTN_GREEN_PIN) == BTN_ACTIVE_LEVEL);

    if (green_pressed) {
        switch (green_state) {
            case BTN_STATE_RELEASED:
                green_state = BTN_STATE_PRESSED;
                green_start_time = now;
                if (mode_actuel == MODE_CONFIGURATION) {
                    configuration_last_activity = now;
                }
                printf("[BOUTON] Vert appuye... Maintenez 5s\r\n");
                break;

            case BTN_STATE_PRESSED:
                if ((now - green_start_time) >= LONG_PRESS_TIME_MS) {
                    // Vert 5 s : entrée / sortie du mode économique
                    if (mode_actuel == MODE_STANDARD) {
                        mode_actuel = MODE_ECONOMIQUE;
                        printf("[ACTION] Appui 5s Vert -> Passage en MODE ECONOMIQUE\r\n");
                        print_mode(mode_actuel);
                    } else if (mode_actuel == MODE_ECONOMIQUE) {
                        mode_actuel = MODE_STANDARD;
                        printf("[ACTION] Appui 5s Vert -> Retour en MODE STANDARD\r\n");
                        print_mode(mode_actuel);
                    } else {
                        printf("[ACTION] Appui 5s Vert ignore : MODE ECONOMIQUE accessible depuis STANDARD uniquement\r\n");
                    }
                    green_state = BTN_STATE_HANDLED;
                }
                break;

            case BTN_STATE_HANDLED:
                break;
        }
    } else {
        if (green_state == BTN_STATE_PRESSED) {
            printf("[BOUTON] Vert relache trop tôt (%lu ms < 5000 ms)\r\n", now - green_start_time);
        }
        green_state = BTN_STATE_RELEASED;
    }

    // ==========================================
    // 2. BOUTON ROUGE (PA8) -> Entrée / sortie du mode maintenance
    // ==========================================
    uint8_t red_pressed = (HAL_GPIO_ReadPin(BTN_RED_PORT, BTN_RED_PIN) == BTN_ACTIVE_LEVEL);

    if (red_pressed) {
        switch (red_state) {
            case BTN_STATE_RELEASED:
                red_state = BTN_STATE_PRESSED;
                red_start_time = now;
                if (mode_actuel == MODE_CONFIGURATION) {
                    configuration_last_activity = now;
                }
                printf("[BOUTON] Rouge appuye... Maintenez 5s\r\n");
                break;

            case BTN_STATE_PRESSED:
                if ((now - red_start_time) >= LONG_PRESS_TIME_MS) {
                    /*
                     * Rouge 5 s : entrée / sortie du mode maintenance
                     * - standard ou économique -> maintenance
                     * - maintenance -> mode précédent (standard ou économique)
                     */
                    if (mode_actuel == MODE_MAINTENANCE) {
                        mode_actuel = previous_mode;
                        printf("[ACTION] Appui 5s Rouge -> Sortie de Maintenance\r\n");
                        print_mode(mode_actuel);
                    } else if (mode_actuel == MODE_STANDARD || mode_actuel == MODE_ECONOMIQUE) {
                        previous_mode = mode_actuel;
                        mode_actuel = MODE_MAINTENANCE;
                        printf("[ACTION] Appui 5s Rouge -> Passage en MODE MAINTENANCE\r\n");
                        print_mode(mode_actuel);
                    } else {
                        printf("[ACTION] Appui 5s Rouge ignore dans MODE CONFIGURATION\r\n");
                    }
                    red_state = BTN_STATE_HANDLED;
                }
                break;

            case BTN_STATE_HANDLED:
                break;
        }
    } else {
        if (red_state == BTN_STATE_PRESSED) {
            printf("[BOUTON] Rouge relache trop tôt (%lu ms < 5000 ms)\r\n", now - red_start_time);
        }
        red_state = BTN_STATE_RELEASED;
    }
}

/**
 * @brief Annonce du mode actif sur le moniteur série
 * (la LED est mise à jour par SystemStatus_Update, seul module qui la pilote)
 */
void print_mode(SystemMode mode) {
    switch (mode) {
        case MODE_STANDARD:      printf("[MODE] Standard (LED verte)\r\n");      break;
        case MODE_CONFIGURATION: printf("[MODE] Configuration (LED jaune)\r\n"); break;
        case MODE_ECONOMIQUE:    printf("[MODE] Economique (LED bleue)\r\n");    break;
        case MODE_MAINTENANCE:   printf("[MODE] Maintenance (LED orange)\r\n");  break;
    }
}