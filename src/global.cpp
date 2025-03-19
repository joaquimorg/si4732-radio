#include <Arduino.h>
#include "global.h"
#include <U8g2lib.h>

hw_timer_t* timer = NULL;

/**
 * brief	HAL functions to toggle EXTCOMIN pin
 */
void IRAM_ATTR hal_extcom_toggle(void) {
    if (digitalRead(GFX_DISPLAY_EXTCOMIN) == HIGH)
        digitalWrite(GFX_DISPLAY_EXTCOMIN, LOW);
    else
        digitalWrite(GFX_DISPLAY_EXTCOMIN, HIGH);
}

void hal_extcom_start() {

    pinMode(GFX_DISPLAY_EXTCOMIN, OUTPUT);
    digitalWrite(GFX_DISPLAY_EXTCOMIN, LOW);
    timer = timerBegin(1400000);
    timerAttachInterrupt(timer, &hal_extcom_toggle);
    timerAlarm(timer, 1400000, true, 0);
}

const char* getStrValue(const char* str, uint8_t index) {
    return u8x8_GetStringLineStart(index, str);
}