#include "grove_gps_air530z.h"
#include "board_pins.h"
#include <string.h>
#include <stdlib.h>

UART_HandleTypeDef huart4;

#define GPS_LINE_MAX 96
#define GGA_FIELDS   15

static uint8_t rx_byte;
static char rx_line[GPS_LINE_MAX];   // Ligne en cours de réception (interruption)
static uint8_t rx_len = 0;
static char ready_line[GPS_LINE_MAX]; // Dernière ligne complète, à décoder
static volatile bool line_ready = false;

static GPS_Data last_gps;
static uint32_t gga_count = 0;

static double nmea_to_decimal(const char *value, char hemisphere)
{
    if (value == 0 || value[0] == '\0') return 0.0;

    // Format NMEA : ddmm.mmmm (latitude) ou dddmm.mmmm (longitude)
    double raw = atof(value);
    int degrees = (int)(raw / 100.0);
    double minutes = raw - (degrees * 100.0);
    double decimal = degrees + (minutes / 60.0);

    if (hemisphere == 'S' || hemisphere == 'W')
        decimal = -decimal;

    return decimal;
}

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

// Vérifie la somme de contrôle "*XX" (XOR des caractères entre '$' et '*')
static bool checksum_ok(const char *line)
{
    const char *star = strchr(line, '*');
    if (line[0] != '$' || star == 0 || hex_value(star[1]) < 0 || hex_value(star[2]) < 0)
        return false;

    uint8_t sum = 0;
    for (const char *p = line + 1; p < star; p++)
        sum ^= (uint8_t)*p;

    return sum == (uint8_t)((hex_value(star[1]) << 4) | hex_value(star[2]));
}

// Découpe sur ',' en gardant les champs vides (contrairement à strtok)
static int split_fields(char *line, char *fields[], int max_fields)
{
    int n = 0;
    char *p = line;

    while (n < max_fields) {
        fields[n++] = p;
        p = strchr(p, ',');
        if (p == 0) break;
        *p++ = '\0';
    }
    return n;
}

static void parse_gga(const char *line)
{
    char copy[GPS_LINE_MAX];
    char *fields[GGA_FIELDS];

    strncpy(copy, line, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = '\0';

    char *star = strchr(copy, '*');
    if (star) *star = '\0';

    // $GxGGA,heure,lat,N/S,lon,E/W,fix,satellites,hdop,altitude,...
    if (split_fields(copy, fields, GGA_FIELDS) < 8)
        return;

    strncpy(last_gps.raw_sentence, line, sizeof(last_gps.raw_sentence) - 1);
    last_gps.raw_sentence[sizeof(last_gps.raw_sentence) - 1] = '\0';

    last_gps.fix_quality = (uint8_t)atoi(fields[6]);
    last_gps.satellites  = (uint8_t)atoi(fields[7]);
    last_gps.valid = last_gps.fix_quality > 0 && fields[2][0] != '\0' && fields[4][0] != '\0';

    if (last_gps.valid) {
        last_gps.latitude  = nmea_to_decimal(fields[2], fields[3][0]);
        last_gps.longitude = nmea_to_decimal(fields[4], fields[5][0]);
    }

    gga_count++;
}

void GroveGPS_Init(void)
{
    /*
     * Port Grove A0 : le TX du module arrive sur PA0 (fil jaune).
     * Inversion TX/RX de l'UART4 : PA0 = UART4_RX, PA1 = UART4_TX.
     * L'init GPIO et NVIC est faite dans HAL_UART_MspInit (setup.c).
     */
    huart4.Instance = GPS_UART_INSTANCE;
    huart4.Init.BaudRate = 9600;
    huart4.Init.WordLength = UART_WORDLENGTH_8B;
    huart4.Init.StopBits = UART_STOPBITS_1;
    huart4.Init.Parity = UART_PARITY_NONE;
    huart4.Init.Mode = UART_MODE_TX_RX;
    huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart4.Init.OverSampling = UART_OVERSAMPLING_16;
    huart4.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart4.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_SWAP_INIT;
    huart4.AdvancedInit.Swap = UART_ADVFEATURE_SWAP_ENABLE;

    HAL_UART_Init(&huart4);
    HAL_UART_Receive_IT(&huart4, &rx_byte, 1);
}

void GroveGPS_Process(void)
{
    // Réception relancée si elle a été interrompue
    if (huart4.RxState == HAL_UART_STATE_READY)
        HAL_UART_Receive_IT(&huart4, &rx_byte, 1);

    if (!line_ready)
        return;

    if (checksum_ok(ready_line) &&
        (strncmp(ready_line, "$GPGGA", 6) == 0 || strncmp(ready_line, "$GNGGA", 6) == 0)) {
        parse_gga(ready_line);
    }

    line_ready = false;
}

uint32_t GroveGPS_GetGGACount(void)
{
    return gga_count;
}

bool GroveGPS_GetLast(GPS_Data *gps)
{
    if (gps == 0 || gga_count == 0)
        return false;

    *gps = last_gps;
    return true;
}

/* ---------- Interruptions UART4 ---------- */

void UART4_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart4);
}

// Un caractère reçu : on construit la ligne, puis on la transmet à GroveGPS_Process()
void GroveGPS_UART_RxCplt(UART_HandleTypeDef *huart)
{
    if (huart->Instance != GPS_UART_INSTANCE)
        return;

    char c = (char)rx_byte;

    if (c == '$')
        rx_len = 0;

    if (c == '\n') {
        // Ligne ignorée si la précédente n'a pas encore été décodée
        if (!line_ready && rx_len > 0) {
            memcpy(ready_line, rx_line, rx_len);
            ready_line[rx_len] = '\0';
            line_ready = true;
        }
        rx_len = 0;
    } else if (c != '\r' && rx_len < GPS_LINE_MAX - 1) {
        rx_line[rx_len++] = c;
    }

    HAL_UART_Receive_IT(&huart4, &rx_byte, 1);
}

// Erreur de réception (débordement, bruit...) : on relance l'écoute
void GroveGPS_UART_Error(UART_HandleTypeDef *huart)
{
    if (huart->Instance != GPS_UART_INSTANCE)
        return;

    rx_len = 0;
    HAL_UART_Receive_IT(&huart4, &rx_byte, 1);
}
