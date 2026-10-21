#ifndef DATALOG_H
#define DATALOG_H

#include <stdbool.h>
#include "acquisition.h"

/*
 * Enregistrement des mesures sur la carte SD (cahier des charges, mode standard) :
 * - une ligne horodatée par mesure, au format CSV (séparateur ';') ;
 * - fichier AAMMJJ_0.LOG (000000_0.LOG sans horloge RTC) ;
 * - fichier plein (FILE_MAX_SIZE) : archivé en AAMMJJ_1.LOG, _2... puis
 *   l'écriture reprend au début d'un nouveau AAMMJJ_0.LOG.
 */

// Monte la carte au démarrage
void DataLog_Init(void);

// Ajoute la mesure au fichier du jour (remonte la carte si elle a été retirée)
void DataLog_Write(const Measurement *m);

// Libère la carte (mode maintenance) : elle peut être retirée sans risque
void DataLog_Release(void);

bool DataLog_IsOk(void);      // false : erreur d'accès ou d'écriture (LED rouge / blanc, blanc x2)
bool DataLog_HasSpace(void);  // false : carte pleine (LED rouge / blanc)

#endif // DATALOG_H
