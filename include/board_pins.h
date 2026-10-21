#ifndef BOARD_PINS_H
#define BOARD_PINS_H

#include "stm32l4xx_hal.h"

/*
 * Grove Chainable RGB LED :
 * - Port Grove D2 du shield
 * - PA10 → CIN (horloge P9813)
 * - PB3  → DIN (données P9813)
 */
#define RGB_DATA_PORT   GPIOB
#define RGB_DATA_PIN    GPIO_PIN_3

#define RGB_CLK_PORT    GPIOA
#define RGB_CLK_PIN     GPIO_PIN_10

/*
 * Grove Light Sensor v1.3
 * Port Grove A2 du shield (PA4 = ADC1_IN9).
 */
#ifndef LIGHT_ADC_CHANNEL
#define LIGHT_ADC_CHANNEL  ADC_CHANNEL_9
#define LIGHT_GPIO_PORT    GPIOA
#define LIGHT_GPIO_PIN     GPIO_PIN_4
#endif

/*
 * Grove Dual Button v1.0
 * Port Grove D7 du shield (actifs à l'état bas, pull-up interne) :
 * - PA8 (D7) : bouton rouge
 * - PA9 (D8) : bouton vert
 */
#define BTN_RED_PORT   GPIOA
#define BTN_RED_PIN    GPIO_PIN_8

#define BTN_GREEN_PORT GPIOA
#define BTN_GREEN_PIN  GPIO_PIN_9

/*
 * Grove GPS Air530Z
 * Port Grove A0 du shield, UART4 à 9600 bauds (AF8) avec inversion TX/RX :
 * - PA0 (fil jaune) : réception, reliée au TX du GPS
 * - PA1 (fil blanc) : émission, reliée au RX du GPS
 */
#define GPS_UART_INSTANCE UART4
#define GPS_GPIO_PORT     GPIOA
#define GPS_GPIO_PINS     (GPIO_PIN_0 | GPIO_PIN_1)

/*
 * Grove RTC v1.2 DS1307 + BME280
 * Tous deux sur I2C.
 */
#define GROVE_I2C_HANDLE hi2c1

/*
 * SD Card Shield Seeed v4.3
 * SPI Arduino : D11/D12/D13 et CS sur D4.
 */
#define SD_CS_PORT GPIOB
#define SD_CS_PIN  GPIO_PIN_5

#define SD_SPI_HANDLE hspi1

#endif