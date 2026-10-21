#include "heap.h"
#include <errno.h>
#include <stdint.h>

// Symboles du linker script (STM32L476RGTX_FLASH.ld)
extern uint8_t end;              // Fin des variables globales (.bss) : début du tas
extern uint8_t _estack;          // Haut de la RAM : début de la pile, qui descend
extern uint8_t _Min_Stack_Size;  // Taille réservée à la pile (l'adresse du symbole est sa valeur)

static uint8_t *heap_end = 0;    // Fin actuelle du tas

// Limite du tas : le bas de la zone réservée à la pile
static uint8_t *heap_limit(void)
{
    return &_estack - (uint32_t)&_Min_Stack_Size;
}

/*
 * Appelée par malloc() pour agrandir le tas.
 * Remplace la version de la bibliothèque C, qui ne vérifie rien.
 */
void *_sbrk(ptrdiff_t incr)
{
    if (heap_end == 0)
        heap_end = &end;

    if (heap_end + incr > heap_limit()) {
        errno = ENOMEM; // malloc() renverra NULL
        return (void *)-1;
    }

    uint8_t *previous = heap_end;
    heap_end += incr;
    return previous;
}

size_t Heap_Used(void)
{
    return heap_end ? (size_t)(heap_end - &end) : 0;
}

size_t Heap_Available(void)
{
    uint8_t *current = heap_end ? heap_end : &end;
    return (size_t)(heap_limit() - current);
}
