#include "defines.h"
#include "konamicode.h"

#define KONAMI_CODE_LENGTH 10
#define KONAMI_TIMEOUT_MS 1000 // 1 second timeout

/**
 * This file allows listening for a Konami Code Sequence.
 */

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

static int konamiCodeIndex = 0;
static unsigned long lastKonamiInputTime = 0;

/**
 * Initialize the listener.
 */
void KONAMI_init(void) {
    konamiCodeIndex = 0;
    lastKonamiInputTime = 0;
}

/**
 * Pass in the buttonPressed.
 * Returns the index the user is within the konami code. 10 if successful.
 */
int KONAMI_update(unsigned long now, int buttonPressed) {

    unsigned long timeSinceLastInput = now - lastKonamiInputTime;
    
    if (konamiCodeIndex > 0 && timeSinceLastInput > KONAMI_TIMEOUT_MS) {
        konamiCodeIndex = 0;
    }
    
    if (buttonPressed == konamiSequence[konamiCodeIndex]) {
        konamiCodeIndex++;
        lastKonamiInputTime = now;
        
        if (konamiCodeIndex >= KONAMI_CODE_LENGTH) {
            konamiCodeIndex = 0;
            return 10;
        }
    } 

    else if (buttonPressed != BTN_NONE) {
        konamiCodeIndex = 0;
    }
    
    return konamiCodeIndex;
}