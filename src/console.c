#include "console.h"
#include "config.h"
#include "version.h"
#include "modes.h"
#include "buttons.h"
#include "grove_rtc_ds1307.h"
#include "grove_gps_air530z.h"
#include "stm32l4xx_hal.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

extern UART_HandleTypeDef huart2;

#define RX_BUF_SIZE 128
#define LINE_MAX    64

// Caractères reçus par interruption, en attente de traitement
static volatile uint8_t rx_buf[RX_BUF_SIZE];
static volatile uint16_t rx_head = 0;
static volatile uint16_t rx_tail = 0;
static uint8_t rx_byte;

// Ligne en cours de saisie
static char line[LINE_MAX];
static uint8_t line_len = 0;

static const char *const day_names[] = { "MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN" };

void Console_Init(void)
{
    setvbuf(stdout, NULL, _IONBF, 0); // printf envoyé immédiatement (écho caractère par caractère)
    HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
}

void Console_PrintHelp(void)
{
    printf("Commandes du mode configuration :\r\n");
    printf("  PARAM=valeur          ex : LOG_INTERVAL=10, LUMIN_LOW=200, TEMP_AIR=0\r\n");
    printf("  CONFIG                affiche tous les parametres\r\n");
    printf("  RESET                 remet les valeurs par defaut\r\n");
    printf("  VERSION               version du programme et numero de lot\r\n");
    printf("  CLOCK=HH:MM:SS        regle l'heure\r\n");
    printf("  DATE=MM,JJ,AAAA       regle la date\r\n");
    printf("  DAY=MON..SUN          regle le jour de la semaine\r\n");
    printf("  HELP                  affiche cette aide\r\n");
}

/* ---------- Réglage de la RTC ---------- */

static bool leap_year(unsigned year)
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

static unsigned days_in_month(unsigned month, unsigned year)
{
    static const uint8_t days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    return (month == 2 && leap_year(year)) ? 29 : days[month - 1];
}

// Lit l'heure actuelle, applique la modification, puis réécrit la RTC
static bool rtc_update(const RTC_DateTime *changes, bool time, bool date, bool day)
{
    RTC_DateTime dt;

    if (!GroveRTC_GetDateTime(&dt)) {
        printf("ERREUR : horloge RTC injoignable\r\n");
        return false;
    }

    if (time) { dt.hours = changes->hours; dt.minutes = changes->minutes; dt.seconds = changes->seconds; }
    if (date) { dt.month = changes->month; dt.day = changes->day; dt.year = changes->year; }
    if (day)  { dt.dayOfWeek = changes->dayOfWeek; }

    if (!GroveRTC_SetDateTime(&dt)) {
        printf("ERREUR : ecriture RTC impossible\r\n");
        return false;
    }

    printf("OK : %s %02u/%02u/%04u %02u:%02u:%02u\r\n", day_names[(dt.dayOfWeek + 6) % 7],
           dt.day, dt.month, dt.year, dt.hours, dt.minutes, dt.seconds);
    return true;
}

static void cmd_clock(const char *value)
{
    unsigned h, m, s;
    char extra;
    RTC_DateTime dt;

    if (sscanf(value, "%u:%u:%u%c", &h, &m, &s, &extra) != 3 || h > 23 || m > 59 || s > 59) {
        printf("ERREUR : format CLOCK=HEURE{0-23}:MINUTE{0-59}:SECONDE{0-59}\r\n");
        return;
    }
    dt.hours = (uint8_t)h;
    dt.minutes = (uint8_t)m;
    dt.seconds = (uint8_t)s;
    rtc_update(&dt, true, false, false);
}

static void cmd_date(const char *value)
{
    unsigned month, day, year;
    char extra;
    RTC_DateTime dt;

    if (sscanf(value, "%u,%u,%u%c", &month, &day, &year, &extra) != 3 ||
        month < 1 || month > 12 || year < 2000 || year > 2099 ||
        day < 1 || day > days_in_month(month, year)) {
        printf("ERREUR : format DATE=MOIS{1-12},JOUR{1-31},ANNEE{2000-2099}\r\n");
        return;
    }
    dt.month = (uint8_t)month;
    dt.day = (uint8_t)day;
    dt.year = (uint16_t)year;
    rtc_update(&dt, false, true, false);
}

static void cmd_day(const char *value)
{
    RTC_DateTime dt;

    for (uint8_t i = 0; i < 7; i++) {
        if (strcmp(value, day_names[i]) == 0) {
            dt.dayOfWeek = i + 1; // DS1307 : 1 = lundi ... 7 = dimanche
            rtc_update(&dt, false, false, true);
            return;
        }
    }
    printf("ERREUR : format DAY={MON,TUE,WED,THU,FRI,SAT,SUN}\r\n");
}

/* ---------- Paramètres ---------- */

static void save_config(void)
{
    if (Config_Save())
        printf("OK : enregistre\r\n");
    else
        printf("ERREUR : ecriture en flash impossible\r\n");
}

static void cmd_param(const char *name, const char *value)
{
    long min, max;

    // Le cahier des charges écrit aussi "LOG_INTERVALL"
    if (strcmp(name, "LOG_INTERVALL") == 0)
        name = "LOG_INTERVAL";

    switch (Config_Set(name, value)) {
        case CONFIG_SET_OK:
            printf("%s = %s\r\n", name, value);
            save_config();
            break;
        case CONFIG_SET_UNKNOWN:
            printf("ERREUR : parametre %s inconnu (CONFIG pour la liste)\r\n", name);
            break;
        case CONFIG_SET_BAD_VALUE:
            printf("ERREUR : valeur numerique attendue\r\n");
            break;
        case CONFIG_SET_OUT_OF_RANGE:
            Config_GetRange(name, &min, &max);
            printf("ERREUR : %s doit etre entre %ld et %ld\r\n", name, min, max);
            break;
        case CONFIG_SET_INCOHERENT:
            printf("ERREUR : le seuil bas doit rester inferieur au seuil haut\r\n");
            break;
    }
}

/* ---------- Exécution d'une ligne ---------- */

static void execute(char *cmd)
{
    if (mode_actuel != MODE_CONFIGURATION) {
        printf("Commandes disponibles en mode configuration uniquement\r\n");
        printf("(redemarrer la carte en maintenant le bouton rouge)\r\n");
        return;
    }

    buttons_config_activity(); // Relance le délai de 30 min sans activité

    char *eq = strchr(cmd, '=');
    if (eq != 0) {
        *eq = '\0';
        const char *name = cmd;
        const char *value = eq + 1;

        if (strcmp(name, "CLOCK") == 0)     cmd_clock(value);
        else if (strcmp(name, "DATE") == 0) cmd_date(value);
        else if (strcmp(name, "DAY") == 0)  cmd_day(value);
        else                                cmd_param(name, value);
        return;
    }

    if (strcmp(cmd, "RESET") == 0) {
        Config_LoadDefaults();
        printf("Parametres remis aux valeurs par defaut\r\n");
        save_config();
    } else if (strcmp(cmd, "VERSION") == 0) {
        printf("Station meteo 3W - version %s - lot %s\r\n", FW_VERSION, BATCH_NUMBER);
    } else if (strcmp(cmd, "CONFIG") == 0) {
        Config_Print();
    } else if (strcmp(cmd, "HELP") == 0) {
        Console_PrintHelp();
    } else {
        printf("ERREUR : commande inconnue (HELP pour la liste)\r\n");
    }
}

void Console_Process(void)
{
    // Réception relancée si elle a été interrompue (erreur de ligne)
    if (huart2.RxState == HAL_UART_STATE_READY)
        HAL_UART_Receive_IT(&huart2, &rx_byte, 1);

    while (rx_tail != rx_head) {
        char c = (char)rx_buf[rx_tail];
        rx_tail = (rx_tail + 1) % RX_BUF_SIZE;

        if (c == '\r' || c == '\n') {
            if (line_len > 0) {
                printf("\r\n");
                line[line_len] = '\0';
                execute(line);
                line_len = 0;
            }
        } else if (c == '\b' || c == 0x7F) {
            if (line_len > 0) {
                line_len--;
                printf("\b \b");
            }
        } else if (isprint((unsigned char)c) && c != ' ' && line_len < LINE_MAX - 1) {
            line[line_len++] = (char)toupper((unsigned char)c);
            printf("%c", c);
        }
    }
}

/* ---------- Interruptions USART2 et répartition des callbacks UART ---------- */

void USART2_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart2);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        uint16_t next = (rx_head + 1) % RX_BUF_SIZE;
        if (next != rx_tail) { // Caractère perdu si le tampon est plein
            rx_buf[rx_head] = rx_byte;
            rx_head = next;
        }
        HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
    } else {
        GroveGPS_UART_RxCplt(huart);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
        HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
    else
        GroveGPS_UART_Error(huart);
}
