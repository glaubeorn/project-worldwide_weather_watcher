#include "datalog.h"
#include "config.h"
#include "ff.h"
#include "diskio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINE_SIZE        160 // Une ligne CSV et son retour à la ligne

static FATFS fs;
static bool mounted = false;
static bool sd_ok = true;
static bool sd_space_ok = true;

/*
 * Tampons de l'écriture, alloués UNE seule fois dans le tas au démarrage
 * (prosit « Il faut changer la pile ! ») : ils ne sont plus sur la pile
 * (FIL fait environ 600 octets) et ne sont jamais libérés, car ils servent
 * pendant toute la vie du programme ; malloc / free répétés fragmenteraient le tas.
 */
static FIL *fil = NULL;
static char *line = NULL;

static const char *fs_type_name(BYTE type)
{
    switch (type) {
        case FS_FAT12: return "FAT12";
        case FS_FAT16: return "FAT16";
        case FS_FAT32: return "FAT32";
        case FS_EXFAT: return "exFAT";
        default:       return "?";
    }
}

static bool mount(void)
{
    if (mounted)
        return true;

    FRESULT fr = f_mount(&fs, "", 1);
    if (fr != FR_OK) {
        f_mount(NULL, "", 0);
        if (fr == FR_NO_FILESYSTEM)
            printf("[SD] ERREUR : carte non formatee en FAT32 / exFAT\r\n");
        else
            printf("[SD] ERREUR : carte absente ou illisible (code %d)\r\n", (int)fr);
        return false;
    }

    mounted = true;
    return true;
}

// Erreur d'accès : la carte sera remontée à la prochaine mesure
static void access_error(const char *action, FRESULT fr)
{
    printf("[SD] ERREUR %s (code %d)\r\n", action, (int)fr);
    f_mount(NULL, "", 0);
    mounted = false;
    sd_ok = false;
}

void DataLog_Init(void)
{
    // Allocation unique des tampons dans le tas, avec test de NULL obligatoire
    fil = malloc(sizeof(FIL));
    line = malloc(LINE_SIZE);
    if (fil == NULL || line == NULL) {
        printf("[SD] ERREUR : memoire insuffisante dans le tas, enregistrement desactive\r\n");
        free(fil);
        free(line);
        fil = NULL;
        line = NULL;
        sd_ok = false;
        return;
    }

    sd_ok = mount();
    if (!sd_ok)
        return;

    DWORD sectors = 0;
    disk_ioctl(0, GET_SECTOR_COUNT, &sectors);
    printf("[SD] Carte montee : %s, %lu Mo\r\n", fs_type_name(fs.fs_type),
           (unsigned long)(sectors / 2048UL));
}

// Nom du fichier : AAMMJJ_N.LOG (000000_N.LOG si l'heure est indisponible)
static void file_name(char *name, size_t len, const Measurement *m, unsigned revision)
{
    if (m->time_valid)
        snprintf(name, len, "%02u%02u%02u_%u.LOG", m->time.year % 100, m->time.month, m->time.day, revision);
    else
        snprintf(name, len, "000000_%u.LOG", revision);
}

// Fichier plein : archivé sous le premier numéro de révision libre
static bool archive(const char *name0, const Measurement *m)
{
    char name[20];
    FILINFO fno;
    unsigned revision = 1;

    do {
        file_name(name, sizeof(name), m, revision++);
    } while (f_stat(name, &fno) == FR_OK);

    FRESULT fr = f_rename(name0, name);
    if (fr != FR_OK) {
        access_error("archivage du fichier", fr);
        return false;
    }
    printf("[SD] %s plein -> archive en %s\r\n", name0, name);
    return true;
}

static bool write_text(FIL *file, const char *text)
{
    UINT len = (UINT)strlen(text);
    UINT written = 0;
    FRESULT fr = f_write(file, text, len, &written);

    if (fr != FR_OK) {
        access_error("ecriture", fr);
        return false;
    }
    if (written < len) {
        printf("[SD] ERREUR : carte pleine\r\n");
        sd_space_ok = false;
        return false;
    }
    return true;
}

void DataLog_Write(const Measurement *m)
{
    if (fil == NULL || line == NULL || !mount()) { // Tampons non alloués ou carte absente
        sd_ok = false;
        return;
    }

    Acquisition_FormatLine(m, line, LINE_SIZE - 2);
    strcat(line, "\r\n");

    char name0[20];
    file_name(name0, sizeof(name0), m, 0);

    // Taille actuelle du fichier de révision 0
    FILINFO fno;
    FSIZE_t size = 0;
    FRESULT fr = f_stat(name0, &fno);
    if (fr == FR_OK)
        size = fno.fsize;
    else if (fr != FR_NO_FILE) {
        access_error("lecture du fichier", fr);
        return;
    }

    if (size > 0 && size + strlen(line) > g_config.file_max_size) {
        if (!archive(name0, m))
            return;
        size = 0;
    }

    fr = f_open(fil, name0, FA_OPEN_APPEND | FA_WRITE);
    if (fr == FR_DENIED) {
        printf("[SD] ERREUR : carte pleine\r\n");
        sd_space_ok = false;
        return;
    }
    if (fr != FR_OK) {
        access_error("ouverture du fichier", fr);
        return;
    }

    bool ok = true;
    if (size == 0) {
        ok = write_text(fil, Acquisition_HeaderLine()) && write_text(fil, "\r\n");
    }
    ok = ok && write_text(fil, line);

    fr = f_close(fil);
    if (fr != FR_OK) {
        access_error("fermeture du fichier", fr);
        return;
    }

    if (ok) {
        sd_ok = true;
        sd_space_ok = true;
        printf("  -> enregistre dans %s\r\n\r\n", name0);
    }
}

void DataLog_Release(void)
{
    if (mounted) {
        f_mount(NULL, "", 0);
        mounted = false;
    }
}

bool DataLog_IsOk(void)
{
    return sd_ok;
}

bool DataLog_HasSpace(void)
{
    return sd_space_ok;
}
