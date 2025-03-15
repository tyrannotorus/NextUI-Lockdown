#include "defines.h"
#include "konamicode.h"

#define KONAMI_CODE_LENGTH 10
#define KONAMI_TIMEOUT 1000 // 1 second timeout

// Konami code sequence
static const int konamiSequence[KONAMI_CODE_LENGTH] = {
    BTN_UP,
    BTN_UP,
    BTN_DOWN,
    BTN_DOWN,
    BTN_LEFT,
    BTN_RIGHT,
    BTN_LEFT,
    BTN_RIGHT,
    BTN_B,
    BTN_A
};

// Track the current state of the Konami code input
static int konamiCodeIndex = 0;
static unsigned long lastKonamiInputTime = 0;

void KONAMI_init(void) {
    konamiCodeIndex = 0;
    lastKonamiInputTime = 0;
}

int KONAMI_update(unsigned long now, int buttonPressed) {
    // Check for timeout
    unsigned long timeSinceLastInput = now - lastKonamiInputTime;
    
    // Reset if timeout has elapsed and we've started the sequence
    if (konamiCodeIndex > 0 && timeSinceLastInput > KONAMI_TIMEOUT) {
        konamiCodeIndex = 0;
    }
    
    // If button pressed matches the next in sequence
    if (buttonPressed == konamiSequence[konamiCodeIndex]) {
        konamiCodeIndex++;
        lastKonamiInputTime = now;
        
        // Check if the full sequence was entered
        if (konamiCodeIndex >= KONAMI_CODE_LENGTH) {
            konamiCodeIndex = 0; // Reset for next time
            return 10; // Konami code successfully entered
        }
    } 
    // If any button was pressed but it's not the correct one
    else if (buttonPressed != BTN_NONE) {
        konamiCodeIndex = 0; // Reset the sequence
    }
    
    return konamiCodeIndex; // Konami code not completed
}

void KONAMI_reset(void) {
    konamiCodeIndex = 0;
}