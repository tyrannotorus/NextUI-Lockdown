#ifndef KONAMICODE_H
#define KONAMICODE_H

#include <stdbool.h>

// Initialize the Konami code detection system
void KONAMI_init(void);

// Update the Konami code state
// Returns true if the Konami code was successfully entered
int KONAMI_update(unsigned long now, int buttonPressed);

// Reset the Konami code state
void KONAMI_reset(void);

#endif