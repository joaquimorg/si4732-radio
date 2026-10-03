#include <Arduino.h>
#include "global.h"
#include <U8g2lib.h>

// The memory LCD needs its VCOM inverted periodically (EXTMODE = H) to avoid DC bias / image retention.
// A FreeRTOS timer keeps toggling even while loop() is blocked (seek, SSB patch loading).
#define EXTCOMIN_HALF_PERIOD_MS 500     // 1 Hz square wave

static TimerHandle_t extcomTimer = NULL;

static void hal_extcom_toggle(TimerHandle_t) {
    static bool level = false;
    level = !level;
    digitalWrite(GFX_DISPLAY_EXTCOMIN, level ? HIGH : LOW);
}

void hal_extcom_start() {
    if (extcomTimer != NULL) return;

    pinMode(GFX_DISPLAY_EXTCOMIN, OUTPUT);
    digitalWrite(GFX_DISPLAY_EXTCOMIN, LOW);
    extcomTimer = xTimerCreate("extcom", pdMS_TO_TICKS(EXTCOMIN_HALF_PERIOD_MS), pdTRUE, NULL, hal_extcom_toggle);
    if (extcomTimer != NULL)
        xTimerStart(extcomTimer, 0);
}

const char* getStrValue(const char* str, uint8_t index) {
    return u8x8_GetStringLineStart(index, str);
}