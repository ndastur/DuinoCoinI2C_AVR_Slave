#pragma once
#include <Arduino.h>

// Simple non-blocking LED blinker for ATtiny85
// --------------------------------------------------
// Usage:
//   ledInit();              // Call once in setup()
//   ledQueue(3);            // Queue 3 blinks
//   ledUpdate();            // Call regularly in loop()
//   if (!ledBusy()) {...}   // Check if finished
//
// Adjust LED_PIN_BIT if your LED is not on PB1
// --------------------------------------------------

#define LED_PIN_BIT PB1

void ledInit();
void ledQueue(uint8_t count);
bool ledBusy();
void ledUpdate();
void ledBlinkBlocking(uint8_t count);