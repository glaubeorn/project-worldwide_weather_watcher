#ifndef GROVE_LIGHT_H
#define GROVE_LIGHT_H

#include <stdbool.h>
#include <stdint.h>

void GroveLight_Init(void);
uint16_t GroveLight_ReadRaw(void);

// false si l'entrée est en l'air (capteur débranché)
bool GroveLight_IsConnected(void);

#endif
