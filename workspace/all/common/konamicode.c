#include "defines.h"
#include "api.h"
#include "konamicode.h"

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
 * Pass in current time.
 * Returns the index the user is within the konami code.
 * Zero if user has made an error, buffer time has elapsed, or button press is not recognized in konami code.
 * KONAMI_CODE_LENGTH if successful.
 */
int KONAMI_update(unsigned long now) {

    unsigned long timeSinceLastInput = now - lastKonamiInputTime;

    if (konamiCodeIndex > 0 && timeSinceLastInput > KONAMI_TIMEOUT_MS) {
        konamiCodeIndex = 0;
    }

    int buttonPressed = BTN_NONE;
    if (PAD_justPressed(BTN_UP)) buttonPressed = BTN_UP;
    else if (PAD_justPressed(BTN_DOWN)) buttonPressed = BTN_DOWN;
    else if (PAD_justPressed(BTN_LEFT)) buttonPressed = BTN_LEFT;
    else if (PAD_justPressed(BTN_RIGHT)) buttonPressed = BTN_RIGHT;
    else if (PAD_justPressed(BTN_A)) buttonPressed = BTN_A;
    else if (PAD_justPressed(BTN_B)) buttonPressed = BTN_B;
    
    if (buttonPressed == konamiSequence[konamiCodeIndex]) {
        konamiCodeIndex++;
        lastKonamiInputTime = now;
        if (konamiCodeIndex >= KONAMI_CODE_LENGTH) {
            konamiCodeIndex = 0;
            return KONAMI_CODE_LENGTH;
        }
    }
    
    return konamiCodeIndex;
}