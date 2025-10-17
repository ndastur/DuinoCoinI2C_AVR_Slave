#ifndef _SLAVEDEVICE_H
#define _SLAVEDEVICE_H

#include "config.h"
#include <Arduino.h>
#include <stdint.h>
#include <Wire.h>

// Max size of received data
static constexpr uint8_t MAX_RX_BUFFER    = 64;
static constexpr uint8_t MAX_TX_BUFFER    = 16;
// The length of data prior to payload, [data len] and [seq] 
static constexpr uint8_t DATA_PREFIX_LENGTH = 2;

static constexpr uint8_t CMD_PING         = 0x01;
static constexpr uint8_t CMD_GET_VERSION  = 0x02;
static constexpr uint8_t CMD_GET_HEAP     = 0x05;
static constexpr uint8_t CMD_GET_UPTIME   = 0x06;
static constexpr uint8_t CMD_GET_CHIP     = 0x07;
static constexpr uint8_t CMD_ECHO         = 0x10;

static constexpr uint8_t CMD_BEGIN_DATA   = 0x20;
static constexpr uint8_t CMD_SEND_DATA    = 0x22;
static constexpr uint8_t CMD_END_DATA     = 0x24;

static constexpr uint8_t CMD_GET_IS_IDLE    = 0x30;
static constexpr uint8_t CMD_GET_JOB_STATUS = 0x32;
static constexpr uint8_t CMD_GET_JOB_DATA   = 0x35;


static constexpr uint8_t CMD_TEST_SEND  = 0x90;
static constexpr uint8_t CMD_TEST_DUMP_DATA  = 0x92;

static constexpr uint8_t ERROR_GENERAL          = 0xE1;
static constexpr uint8_t ERROR_NOT_READY        = 0xE2;
static constexpr uint8_t ERROR_FRAME_TOO_SMALL  = 0xE4;
static constexpr uint8_t ERROR_OUT_OF_SEQ       = 0xE5;
static constexpr uint8_t ERROR_CRC_MISMATCH     = 0xE6;
static constexpr uint8_t ERROR_NO_CRC           = 0xE7;
static constexpr uint8_t ERROR_JOB_OVERRUN      = 0xE8;
static constexpr uint8_t ERROR_JOB_END_BAD      = 0xE9;
static constexpr uint8_t ERROR_                 = 0xEE;

static constexpr uint8_t HASH_BUFFER_SIZE = 20;

// Mining job structure
static constexpr uint8_t MINING_JOB_RAW_LEN = 20 + 20 +1;
struct MiningJob {
  union {
    struct {
      uint8_t prevHash[HASH_BUFFER_SIZE];      // 20 bytes
      uint8_t expectedHash[HASH_BUFFER_SIZE];  // 20 bytes
      uint8_t difficulty;        // 1 byte
    };
    uint8_t raw[MINING_JOB_RAW_LEN];             // 20 + 20 + 1
  };
  uint8_t crc8 = 0;
  uint16_t foundNonce = 0;
  uint8_t jobTimeTakenMs = 0;
};

struct MiningStats {
  uint16_t sharesFound = 0;
};


// Function prototypes
void slave_begin(uint8_t i2cAddr);
void slave_loop();        // call frequently in main loop

#endif    // _SLAVEDEVICE_H