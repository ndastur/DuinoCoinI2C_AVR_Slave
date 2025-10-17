#ifndef _DEVICE_H
#define _DEVICE_H
#pragma once

#include <Arduino.h>

extern "C" char* sbrk(int incr);
typedef struct __freelist {
  size_t sz;
  struct __freelist* nx;
} __freelist_t;
extern __freelist_t* __flp;

static int freeListSize() {
  int total = 0;
  for (__freelist_t* p = __flp; p; p = p->nx) total += p->sz + 2;
  return total;
}

uint16_t freeSRAM() {
  char top;
  extern char __bss_end;
  extern char* __brkval;
  if (__brkval == 0) return (&top - &__bss_end);
  return (&top - __brkval) + freeListSize();
}

uint8_t readSignatureByte(uint16_t addr) {
  // Prefer boot_signature_byte_get if available; else fallback using LPM/SIGRD.
  #if defined(boot_signature_byte_get)
    return boot_signature_byte_get(addr);
  #else
    // Generic fallback for AVR with SIGRD capability
    // Many cores still provide boot_signature_byte_get; this path is rarely used.
    // If your core lacks SIGRD, just return 0x00.
    #if defined(SPMCSR) || defined(SPMCR)
      // Not all cores expose an easy API; safest generic return.
      // (Leave as 0 for odd cores; Nano/ATtiny85 will typically hit boot_signature_byte_get path.)
      return 0x00;
    #else
      return 0x00;
    #endif
  #endif
}

void getChipId(uint8_t &s0, uint8_t &s1, uint8_t &s2) {
  // AVR signature bytes: 0x0000, 0x0001, 0x0002 OR at 0x0000, 0x0002, 0x0004 depending on device.
  // To be robust, read 0x0000, 0x0001, 0x0002 first; if 0x0001 looks 0x00, try spaced addresses.
  s0 = readSignatureByte(0x0000);
  s1 = readSignatureByte(0x0001);
  s2 = readSignatureByte(0x0002);

  if (s1 == 0x00 && s2 == 0x00) { // fall back for devices mapping every 2 bytes
    s1 = readSignatureByte(0x0002);
    s2 = readSignatureByte(0x0004);
  }
}

#endif