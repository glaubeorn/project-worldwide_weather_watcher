#ifndef MODES_H
#define MODES_H

// Énumération des 4 modes de la station météo 3W
typedef enum {
    MODE_STANDARD,
    MODE_CONFIGURATION,
    MODE_ECONOMIQUE,
    MODE_MAINTENANCE
} SystemMode;

// ⚠️ LIGNE À AJOUTER : Rend la variable accessible dans tous les fichiers .c qui incluent modes.h
extern SystemMode mode_actuel;

#endif // MODES_H
