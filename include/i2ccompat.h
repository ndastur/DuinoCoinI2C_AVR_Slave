#pragma once
#include <Arduino.h>

// ---------- Board detection ----------
#if defined(ARDUINO_AVR_ATTINYX5) || defined(__AVR_ATtiny85__) || defined(__AVR_ATtiny45__)
  #define I2C_USING_TINYWIRE
#endif

#ifdef I2C_USING_TINYWIRE
  #include <TinyWireS.h>     // Slave on ATtiny85 (USI)
  // TinyWireS I2C pins are fixed by the USI hardware (SDA=P0, SCL=P2 on Digispark)
#else
  #include <Wire.h>          // Slave on Nano/ATmega328P (TWI)
#endif

// ---------- Unified types ----------
typedef void (*I2COnReceiveCB)(uint8_t count);
typedef void (*I2COnRequestCB)(void);

// ---------- Unified API ----------
namespace i2c {

inline void begin(uint8_t address) {
#ifdef I2C_USING_TINYWIRE
  TinyWireS.begin(address);
  // Disable internal pullups on ATTiny85 I2C pins (PB0=SDA, PB2=SCL)
  // so we don't fight external bus pull-ups.
  PORTB &= ~(_BV(PB0) | _BV(PB2));
#else
  Wire.begin(address);
  // Disable internal pullups on Nano A4/A5 (SDA/SCL)
  PORTC &= ~(_BV(PORTC4) | _BV(PORTC5));
#endif
}

inline void onReceive(I2COnReceiveCB cb) {
#ifdef I2C_USING_TINYWIRE
  TinyWireS.onReceive(reinterpret_cast<void (*)(uint8_t)>(cb));
#else
  Wire.onReceive(reinterpret_cast<void (*)(int)>(cb));
#endif
}

inline void onRequest(I2COnRequestCB cb) {
#ifdef I2C_USING_TINYWIRE
  TinyWireS.onRequest(cb);
#else
  Wire.onRequest(cb);
#endif
}

inline uint8_t available() {
#ifdef I2C_USING_TINYWIRE
  return TinyWireS.available();
#else
  return Wire.available();
#endif
}

inline uint8_t read() {
#ifdef I2C_USING_TINYWIRE
  return TinyWireS.receive();
#else
  return Wire.read();
#endif
}

inline void write(uint8_t b) {
#ifdef I2C_USING_TINYWIRE
  TinyWireS.send(b);
#else
  Wire.write(b);
#endif
}

// Must be called periodically on TinyWireS to handle STOP conditions
inline void poll() {
#ifdef I2C_USING_TINYWIRE
  TinyWireS_stop_check(); // no-op on Wire
#endif
}

} // namespace i2c
