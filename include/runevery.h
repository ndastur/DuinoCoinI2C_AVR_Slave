// RunEvery.h
#ifndef RUNEVERY_H
#define RUNEVERY_H

#include <Arduino.h>

/*
  Usage:
  RunEvery blink(500); // ~500 ms

  void loop() {
    if (blink.due()) {
      // toggle LED
      }
  }
*/
struct RunEvery {
  unsigned long last = 0;   // 4 bytes
  unsigned long interval;   // 4 bytes

  explicit RunEvery(unsigned long ms) : interval(ms) {}

  inline bool due() {
    unsigned long now = millis();
    if (now - last >= interval) {
      last = now;
      return true;
    }
    return false;
  }
};

#endif // RUNEVERY_H
