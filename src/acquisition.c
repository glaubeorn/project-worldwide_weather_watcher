#include "acquisition.h"
#include "config.h"
#include "stm32l4xx_hal.h"
#include "grove_bme680.h"
#include "grove_light.h"
#include "grove_rtc_ds1307.h"
#include "grove_gps_air530z.h"
#include <stdio.h>
#include <string.h>

#define RETRY_PERIOD_MS    200  // Délai entre 2 tentatives sur un capteur qui ne répond pas
#define TIMEOUTS_FOR_ERROR 2    // Mesures consécutives en timeout avant erreur

//static BME280_Handle bme280;
static bool running = false;
static uint32_t start_tick;
static uint32_t last_try_tick;
static bool env_done;
static bool light_done;
static bool gps_done;
static bool gps_requested;  // false en mode économique, une mesure sur deux
static uint32_t gps_count_at_start;
static Measurement current;

static uint8_t env_timeouts = 0;
static uint8_t light_timeouts = 0;
static uint8_t gps_timeouts = 0;
static AcquisitionHealth health = { true, true, true, true };

static bool env_enabled(void)
{
    return g_config.temp_air || g_config.hygr || g_config.pressure;
}

// Capteur BME280 : température, humidité, pression
static bool read_env(void)
{
    Env_Data d;

    if (!GroveBME680_ReadEnvironment(&d))
        return false;

    current.temp_valid = g_config.temp_air;
    current.temperature_c = d.temperature_c;

    // Hygrométrie ignorée hors de la plage de température HYGR_MINT..HYGR_MAXT
    current.hum_valid = g_config.hygr &&
                        d.temperature_c >= g_config.hygr_mint &&
                        d.temperature_c <= g_config.hygr_maxt;
    current.humidity_percent = d.humidity_percent;

    current.pres_valid = g_config.pressure;
    current.pressure_hpa = d.pressure_hpa;

    return true;
}

// Capteur de luminosité : ADC 12 bits ramené sur 0-1023
static bool read_light(void)
{
    // Capteur analogique : débranché, l'entrée est en l'air et donne une valeur au hasard
    if (!GroveLight_IsConnected())
        return false;

    uint16_t raw = GroveLight_ReadRaw();

    if (raw == 0xFFFF)
        return false;

    current.light = raw >> 2;
    current.light_valid = true;

    if (current.light < g_config.lumin_low)
        current.light_level = LIGHT_LEVEL_LOW;
    else if (current.light > g_config.lumin_high)
        current.light_level = LIGHT_LEVEL_HIGH;
    else
        current.light_level = LIGHT_LEVEL_MEDIUM;

    return true;
}

// GPS : attend une nouvelle trame GGA reçue depuis le début de l'acquisition
static bool read_gps(void)
{
    GPS_Data gps;

    if (GroveGPS_GetGGACount() == gps_count_at_start || !GroveGPS_GetLast(&gps))
        return false;

    current.gps_responded = true;
    current.gps_valid = gps.valid;
    current.latitude = gps.latitude;
    current.longitude = gps.longitude;
    current.satellites = gps.satellites;
    return true;
}

// Compte les mesures consécutives en timeout d'un capteur
static void update_timeouts(uint8_t *count, bool enabled, bool done)
{
    if (!enabled || done)
        *count = 0;
    else if (*count < TIMEOUTS_FOR_ERROR)
        (*count)++;
}

static bool data_coherent(const Measurement *m)
{
    if (m->temp_valid &&
        (m->temperature_c < g_config.min_temp_air || m->temperature_c > g_config.max_temp_air))
        return false;

    if (m->pres_valid &&
        (m->pressure_hpa < g_config.pressure_min || m->pressure_hpa > g_config.pressure_max))
        return false;

    return true;
}

void Acquisition_Start(bool with_gps)
{
    memset(&current, 0, sizeof(current));

    current.time_valid = GroveRTC_GetDateTime(&current.time);
    health.rtc_ok = current.time_valid;

    env_done = !env_enabled();
    light_done = !g_config.lumin;
    gps_requested = with_gps;
    gps_done = !with_gps;
    current.gps_skipped = !with_gps;
    gps_count_at_start = GroveGPS_GetGGACount();

    start_tick = HAL_GetTick();
    last_try_tick = start_tick - RETRY_PERIOD_MS; // Première tentative immédiate
    running = true;
}

bool Acquisition_Poll(Measurement *m)
{
    if (!running)
        return false;

    uint32_t now = HAL_GetTick();

    if (now - last_try_tick >= RETRY_PERIOD_MS) {
        last_try_tick = now;
        if (!env_done)   env_done = read_env();
        if (!light_done) light_done = read_light();
    }

    // Le GPS envoie une trame par seconde : vérifié à chaque passage
    if (!gps_done) gps_done = read_gps();

    // Cahier des charges : un capteur est attendu jusqu'à TIMEOUT, puis sa donnée vaut "NA"
    bool timed_out = (now - start_tick) >= (uint32_t)g_config.timeout_s * 1000UL;
    if (!(env_done && light_done && gps_done) && !timed_out)
        return false;

    // Fin de l'acquisition : les capteurs n'ayant pas répondu restent à "NA"
    update_timeouts(&env_timeouts, env_enabled(), env_done);
    update_timeouts(&light_timeouts, g_config.lumin, light_done);
    if (gps_requested) // GPS non interrogé : son compteur de timeouts est conservé
        update_timeouts(&gps_timeouts, true, gps_done);

    health.gps_ok = gps_timeouts < TIMEOUTS_FOR_ERROR;
    health.sensor_ok = env_timeouts < TIMEOUTS_FOR_ERROR && light_timeouts < TIMEOUTS_FOR_ERROR;
    health.data_ok = data_coherent(&current);

    *m = current;
    running = false;
    return true;
}

bool Acquisition_IsRunning(void)
{
    return running;
}

AcquisitionHealth Acquisition_GetHealth(void)
{
    return health;
}

// printf sans support float : valeur avec 1 décimale, ou "NA"
static void format_1dec(char *buf, size_t len, bool valid, float value)
{
    if (!valid) {
        snprintf(buf, len, "NA");
        return;
    }

    int x10 = (int)(value * 10.0f + (value >= 0.0f ? 0.5f : -0.5f));
    const char *sign = (x10 < 0) ? "-" : "";
    if (x10 < 0) x10 = -x10;
    snprintf(buf, len, "%s%d.%d", sign, x10 / 10, x10 % 10);
}

// Coordonnée en degrés décimaux avec 6 décimales (précision ~10 cm)
static void format_coord(char *buf, size_t len, double value)
{
    long micro = (long)(value * 1000000.0 + (value >= 0.0 ? 0.5 : -0.5));
    const char *sign = (micro < 0) ? "-" : "";
    if (micro < 0) micro = -micro;
    snprintf(buf, len, "%s%ld.%06ld", sign, micro / 1000000L, micro % 1000000L);
}

static const char *light_level_name(LightLevel level)
{
    switch (level) {
        case LIGHT_LEVEL_LOW:    return "FAIBLE";
        case LIGHT_LEVEL_MEDIUM: return "MOYENNE";
        case LIGHT_LEVEL_HIGH:   return "FORTE";
        default:                 return "NA";
    }
}

// Une ligne "  Libelle : valeur unite" du bloc affiché sur le moniteur série
static void print_field(const char *label, bool valid, float value, const char *unit)
{
    char buf[16];

    if (valid) {
        format_1dec(buf, sizeof(buf), true, value);
        printf("  %-12s: %s %s\r\n", label, buf, unit);
    } else {
        printf("  %-12s: indisponible\r\n", label);
    }
}

void Acquisition_Print(const Measurement *m)
{
    if (m->time_valid) {
        printf("----- Mesure du %02u/%02u/%04u a %02u:%02u:%02u -----\r\n",
               m->time.day, m->time.month, m->time.year,
               m->time.hours, m->time.minutes, m->time.seconds);
    } else {
        printf("----- Mesure (heure indisponible) -----\r\n");
    }

    print_field("Temperature", m->temp_valid, m->temperature_c, "C");
    print_field("Humidite", m->hum_valid, m->humidity_percent, "%");
    print_field("Pression", m->pres_valid, m->pressure_hpa, "hPa");

    if (m->light_valid) {
        static const char *const level_text[] = { "?", "faible", "moyenne", "forte" };
        printf("  %-12s: %u / 1023 (%s)\r\n", "Luminosite", m->light, level_text[m->light_level]);
    } else {
        printf("  %-12s: indisponible\r\n", "Luminosite");
    }

    if (m->gps_skipped) {
        printf("  %-12s: non mesuree (mode economique : 1 mesure sur 2)\r\n", "Position");
    } else if (m->gps_valid) {
        char lat[16], lon[16];
        format_coord(lat, sizeof(lat), m->latitude < 0 ? -m->latitude : m->latitude);
        format_coord(lon, sizeof(lon), m->longitude < 0 ? -m->longitude : m->longitude);
        printf("  %-12s: %s %c, %s %c (%u satellites)\r\n", "Position",
               lat, m->latitude < 0 ? 'S' : 'N', lon, m->longitude < 0 ? 'W' : 'E', m->satellites);
    } else if (m->gps_responded) {
        printf("  %-12s: recherche des satellites... (%u en vue)\r\n", "Position", m->satellites);
    } else {
        printf("  %-12s: indisponible\r\n", "Position");
    }
    printf("\r\n");
}

const char *Acquisition_HeaderLine(void)
{
    return "DATE HEURE;TEMP_AIR(C);HYGR(%);PRESSURE(hPa);LUMIN(0-1023);LUMIN_NIVEAU;LATITUDE;LONGITUDE";
}

void Acquisition_FormatLine(const Measurement *m, char *buf, size_t len)
{
    char time_str[32];
    char temp[16], hum[16], pres[16], light[8], lat[16], lon[16];

    if (m->time_valid) {
        snprintf(time_str, sizeof(time_str), "%04u-%02u-%02u %02u:%02u:%02u",
                 m->time.year, m->time.month, m->time.day,
                 m->time.hours, m->time.minutes, m->time.seconds);
    } else {
        snprintf(time_str, sizeof(time_str), "NA");
    }

    format_1dec(temp, sizeof(temp), m->temp_valid, m->temperature_c);
    format_1dec(hum, sizeof(hum), m->hum_valid, m->humidity_percent);
    format_1dec(pres, sizeof(pres), m->pres_valid, m->pressure_hpa);

    if (m->light_valid)
        snprintf(light, sizeof(light), "%u", m->light);
    else
        snprintf(light, sizeof(light), "NA");

    if (m->gps_valid) {
        format_coord(lat, sizeof(lat), m->latitude);
        format_coord(lon, sizeof(lon), m->longitude);
    } else {
        snprintf(lat, sizeof(lat), "NA");
        snprintf(lon, sizeof(lon), "NA");
    }

    snprintf(buf, len, "%s;%s;%s;%s;%s;%s;%s;%s", time_str, temp, hum, pres, light,
             m->light_valid ? light_level_name(m->light_level) : "NA", lat, lon);
}
