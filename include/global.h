#pragma once
#include <Arduino.h>

// Band Types
#define FM_BAND_TYPE 0
#define MW_BAND_TYPE 1
#define SW_BAND_TYPE 2
#define LW_BAND_TYPE 3

// Modes
#define FM  0
#define LSB 1
#define USB 2
#define AM  3


#define GFX_DISPLAY_EXTCOMIN  4

// Rotary encoder
#define ROTARY_ENCODER_A_PIN 8
#define ROTARY_ENCODER_B_PIN 6
#define ROTARY_ENCODER_BUTTON_PIN 10
#define ROTARY_ENCODER_VCC_PIN 13


// I2C bus pins
#define ESP32_I2C_SDA 39
#define ESP32_I2C_SCL 40

#define RESET_PIN 35

#define AUDIO_MUTE 36

#define AUDIO_INPUT 3

void hal_extcom_start();

const char* getStrValue(const char* str, uint8_t index);
