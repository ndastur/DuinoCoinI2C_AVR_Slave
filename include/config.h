#ifndef _CONFIG_H
#define _CONFIG_H

#include <stdint.h>

static constexpr unsigned int VER_MAJOR = 1;
static constexpr unsigned int VER_MINOR = 4;

// Set your 7-bit I2C address here (avoid reserved addresses)
static constexpr uint8_t I2C_ADDR = 0x30;

#if defined(SERIAL_PRINT)
  #define SERIALBEGIN(x)            Serial.begin(x)
  #define SERIALPRINT(x)            Serial.print(x)
  #define SERIALPRINT_LN(x)         Serial.println(x)
  #define SERIALPRINT_HEX(x)        Serial.print(x, HEX)
#else
  #define SERIALBEGIN(x)
  #define SERIALPRINT(x)
  #define SERIALPRINT_LN(x)
  #define SERIALPRINT_HEX(x)
#endif

#if defined(DEBUG_PRINT)
  #define DEBUGPRINT(x)         SERIALPRINT(x)
  #define DEBUGPRINT_LN(x)      SERIALPRINT_LN(x)
  #define DEBUGPRINT_HEX(x)     SERIALPRINT_HEX(x)
#else
  #define DEBUGPRINT(x)
  #define DEBUGPRINT_LN(x)
  #define DEBUGPRINT_HEX(x)
#endif


#endif