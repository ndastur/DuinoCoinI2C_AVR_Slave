#include "led.h"
#include <avr/io.h>

#define TOGGLE_INTERVAL 200

#define LED_ON()  (PORTB |=  _BV(LED_PIN_BIT))
#define LED_OFF() (PORTB &= ~_BV(LED_PIN_BIT))

static uint8_t ledBlinksQueued = 0;
static uint8_t blinkCount = 0;
static bool ledState = false;
static unsigned long lastChange = 0;

void ledInit() {
  DDRB |= _BV(LED_PIN_BIT);  // Set pin as output
  LED_OFF();
  ledBlinksQueued = 0;
  blinkCount = 0;
  ledState = false;
  lastChange = millis();
}

void ledQueue(uint8_t count) {
  ledBlinksQueued = count;
  blinkCount = 0;
  ledState = false;
  LED_OFF();
}

bool ledBusy() {
  return ledBlinksQueued > 0;
}

void ledUpdate() {
  if (ledBlinksQueued == 0) {
    LED_OFF();
    return;
  }

  unsigned long now = millis();
  if (now - lastChange < TOGGLE_INTERVAL) return;  // 100ms toggle interval
  lastChange = now;

  if (ledState) {
    LED_OFF();
    ledState = false;
    blinkCount++;

    if (blinkCount >= ledBlinksQueued) {
      ledBlinksQueued = 0;
      blinkCount = 0;
    }
  } else {
    LED_ON();
    ledState = true;
  }
}

void ledBlinkBlocking(uint8_t count) {
    for(uint8_t c = 0; c < count; c++) {
        LED_ON();
        delay(TOGGLE_INTERVAL);
        LED_OFF();
        delay(TOGGLE_INTERVAL);
    }
    LED_OFF();
}