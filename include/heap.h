#ifndef HEAP_H
#define HEAP_H

#include <stddef.h>

/*
 * Gestion du tas (prosit « Il faut changer la pile ! ») :
 * malloc() agrandit le tas vers le haut, la pile descend depuis le haut de la RAM.
 * Le tas est limité pour ne jamais entrer dans la zone réservée à la pile
 * (_Min_Stack_Size du linker script) : malloc() renvoie NULL au lieu d'écraser la pile.
 */

size_t Heap_Used(void);       // Octets du tas utilisés par malloc()
size_t Heap_Available(void);  // Octets encore disponibles avant la zone de la pile

#endif // HEAP_H
