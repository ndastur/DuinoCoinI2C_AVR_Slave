/*
 * I2C Slave for DuinoCoin mining
 * Wire buffer: 16 bytes
 * 
 * Wiring:
 * PB0 (Pin 5) - SDA
 * PB2 (Pin 7) - SCL
 * 
 * Add 4.7k pullup resistors on both SDA and SCL lines
 */
#if !defined(RUN_TEST)

#include "config.h"
#include "led.h"
#include "device.h"
#include "runevery.h"
#include "duco_hash.h"
#include "slavedevice.h"

#include <EEPROM.h>
#include <Arduino.h>

bool startUpError = false;

void setup() {
  // Optional serial for debug
  SERIALBEGIN(115200);
  delay(10);
  SERIALPRINT_LN(F("Nano I2C Slave starting..."));

  // Startup indication - blocking is OK here
  ledBlinkBlocking(2);

  uint8_t addr;
  if(I2C_ADDR == 0) {
    // Get the address from EEPROM
    addr = EEPROM.read(0);
    if(addr < 8) {
      startUpError = true;
      return;
    }
    else {
      addr = I2C_ADDR;
    }
  }
  else {
    // config has an addres set, so probably want to save in EEPROM unless it's the same
    if(I2C_ADDR != EEPROM.read(0)) {
      EEPROM.write(0, I2C_ADDR);
    }
    addr = I2C_ADDR;
  }
  
  SERIALPRINT(F("i2c address: 0x"));
  SERIALPRINT_HEX(addr);
  SERIALPRINT_LN();
  slave_begin(I2C_ADDR);
}

RunEvery ErrorLed(1500);

void loop() {
  if(startUpError) {
    if(ErrorLed.due()) {
        ledBlinkBlocking(4);
    }
    return;
  }
  
  slave_loop();

  // Small delay to prevent tight looping
  delay(1);
}

#endif
