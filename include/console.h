#ifndef CONSOLE_H
#define CONSOLE_H

/*
 * Console du mode configuration sur le moniteur série (USART2, 115200 bauds).
 * Commandes du cahier des charges : PARAM=valeur, RESET, VERSION, CLOCK, DATE, DAY.
 * Commandes d'affichage : CONFIG (réglages actuels) et HELP (liste des commandes).
 */

// Démarre la réception des caractères (interruption USART2)
void Console_Init(void);

// À appeler dans la boucle principale : affiche et exécute les commandes tapées
void Console_Process(void);

// Liste des commandes (commande HELP et entrée en mode configuration)
void Console_PrintHelp(void);

#endif // CONSOLE_H
