#include "config.h"
#include "slavedevice.h"
#include "duco_hash.h"
#include <Wire.h>

uint8_t _addr;

MiningJob currentJob;
MiningStats stats;

// bool jobDataPacketsReady = false;
// bool doingWork = false;    // Flag for busy doing work

volatile uint8_t rxBuf[MAX_RX_BUFFER];    // Max data length can i2c can send on Wire
uint8_t rxBytesIn = 0;

// Response buffer (32 byte Wire buffer limit, but on the tiny only 16)
volatile uint8_t responseBuffer[MAX_TX_BUFFER];
volatile uint8_t responseLength = 0;

static constexpr uint8_t LOOP_STATE_IDLE = 0;
static constexpr uint8_t LOOP_STATE_PROCESS_PKT_IN = 1;
static constexpr uint8_t LOOP_STATE_DO_WORK = 2;
uint8_t LoopState = LOOP_STATE_IDLE;

// Forward declarations
uint16_t work();
void _clearArray(volatile uint8_t *ary, uint8_t size);
void receiveEvent(int numBytes);
void requestEvent();

static void disableInternalPullupsOnI2C()
{
  // On ATmega328P, Wire enables internal pull-ups by setting PORTC4/5.
  // Clear them so the bus idles at the external 3.3 V pull-ups only.
  // SDA = PC4, SCL = PC5
  PORTC &= ~(_BV(PORTC4) | _BV(PORTC5));
}

// static inline bool memcpy_volatile(volatile uint8_t *a, const volatile uint8_t *b, size_t len) {
//   for (size_t i = 0; i < len; ++i) {
//     a[i] = b[i];
//   }
//   return true;
// }

// static inline bool memcmp_volatile(const volatile uint8_t *a, const volatile uint8_t *b, size_t len) {
//   for (size_t i = 0; i < len; ++i) {
//       if (a[i] != b[i]) return false;
//   }
//   return true;
// }

static inline uint8_t crc8_maxim(const uint8_t* data, size_t len) {
  uint8_t crc = 0x00;
  for (size_t i = 0; i < len; ++i) {
    uint8_t in = data[i];
    for (uint8_t b = 0; b < 8; ++b) {
      uint8_t mix = (crc ^ in) & 0x01;
      crc >>= 1;
      if (mix) crc ^= 0x8C; // 0x8C is 0x31 reflected (for right-shift)
      in >>= 1;
    }
  }
  return crc;
}

static void bytes_to_hex(const uint8_t* in, size_t len, char* out40) {
  static const char* L="0123456789abcdef";
  for (size_t i=0;i<len;++i){ out40[2*i]=L[in[i]>>4]; out40[2*i+1]=L[in[i]&0xF]; }
  out40[2*len]='\0';
}

// DUCO-S1A hasher
uint16_t work(uint8_t diff, const char *prevHash, uint8_t *expectedHash)
{
  if (diff > 655)
    return 0;

  static duco_hash_state_t hash;
  duco_hash_init(&hash, prevHash);

  char nonceStr[10 + 1];
  for (uint16_t nonce = 0; nonce < (uint16_t)diff*100+1; nonce++) {
    ultoa(nonce, nonceStr, 10);
    uint8_t const * hash_bytes = duco_hash_try_nonce(&hash, nonceStr);
    if (memcmp(hash_bytes, expectedHash, 20) == 0) {
      return nonce;
    }
    // if (WDT_EN) {
    //   if (runEvery(2000)) wdt_reset();
    // }
  }

  return 0;
}

void _clearArray(volatile uint8_t *ary, uint8_t size) {
      while (size--)
        ary[size-1]=0;
}

static void dumpCurrentMiningJob() {
  #ifdef SERIAL_PRINT
    char strBuf[180];
    SERIALPRINT_LN("[I2C] Current Mining Job Data");

    // hex buffers need 2*len + 1 bytes
    char prevHex[40 + 1];
    char expHex[40 + 1];

    // bytes_to_hex reads the bytes — cast away volatile to const for read-only access
    bytes_to_hex((const uint8_t*)currentJob.prevHash, 20, prevHex);
    bytes_to_hex((const uint8_t*)currentJob.expectedHash, 20, expHex);

    snprintf(strBuf, sizeof(strBuf),
      "   prevHash: %s\n   expectedHash: %s\n   difficulty: %u\n   foundNonce: %u\n   hashTime %ums\n",
      prevHex, expHex, currentJob.difficulty,
      currentJob.foundNonce,
      currentJob.jobTimeTakenMs);
    SERIALPRINT_LN( strBuf );  
  #endif
}

/// @brief Start the I2C bus  
/// @param i2cAddr 
void slave_begin(uint8_t i2cAddr) {
  _addr = i2cAddr;
    // Initialize all buffers
  _clearArray(responseBuffer, sizeof(responseBuffer));
  responseLength = 0;

  currentJob.difficulty = 0;
  rxBytesIn = 0;

    // Initialize Wire as slave
  Wire.begin(I2C_ADDR);
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);

  // Kill the default internal 5 V pull-ups that Wire turns on
  disableInternalPullupsOnI2C();
}

// ------------------- Main Loop -------------------
void slave_loop() {

  // Might just be for ATTiny85 ??
  //millis(); // ????? For some reason need this to work the i2c

  switch (LoopState)
  {
  case LOOP_STATE_PROCESS_PKT_IN:
    memcpy(currentJob.raw, (const uint8_t *)rxBuf, MINING_JOB_RAW_LEN);
    currentJob.foundNonce = 0;
    LoopState = LOOP_STATE_DO_WORK;
    dumpCurrentMiningJob();   // only if SERIAL_PRINT defined
    break;

  case LOOP_STATE_DO_WORK: {
    char prevHashStr[40];
    bytes_to_hex(currentJob.prevHash, 20, prevHashStr); /// the hash algo works on the string
    unsigned long startTime = millis();
    uint16_t result = work(currentJob.difficulty, 
      prevHashStr,
      currentJob.expectedHash
      );
    currentJob.jobTimeTakenMs = millis() - startTime;

    if(result != 0) {
      SERIALPRINT_LN(result);
      SERIALPRINT_LN("[DUCO] Found Share !");
      currentJob.foundNonce = result;
      stats.sharesFound++;
      LoopState = LOOP_STATE_IDLE;
      dumpCurrentMiningJob();   // only if SERIAL_PRINT defined
    }
    break;
    }

  default:
    break;
  }



}

// ---- I2C handlers ----
void receiveEvent(int numBytes) {
  if (numBytes == 0) return;
  uint8_t command = Wire.read();
  numBytes--;
  
  // Clear response buffer and length
  responseLength = 0;
  _clearArray(responseBuffer, sizeof(responseBuffer));
  
  switch (command) {
    // ===== Test Commands (0x01-0x0F) =====
    
    case CMD_PING:
      SERIALPRINT_LN(F("[I2C] CMD_PING"));
      responseBuffer[0] = 0xAA;
      responseLength = 1;
      break;
            
    case CMD_GET_VERSION:
      SERIALPRINT_LN(F("[I2C] CMD_GET_VERSION"));
      responseBuffer[0] = VER_MAJOR;
      responseBuffer[1] = VER_MINOR;
      responseLength = 2;
      break;
    
    case CMD_GET_HEAP:    // not implemented yet
      SERIALPRINT_LN(F("[I2C] CMD_GET_HEAP"));
      responseBuffer[0] = 0xFC;
      responseBuffer[1] = 0xFA;
      responseBuffer[2] = 0xFC;
      responseBuffer[3] = 0xFA;
      responseLength = 4;
      break;

    case CMD_GET_UPTIME: {
      SERIALPRINT_LN(F("[I2C] CMD_GET_UPTIME."));
      uint32_t ms = millis();
      responseBuffer[0] = (uint8_t)(ms & 0xFF);
      responseBuffer[1] = (uint8_t)((ms >> 8) & 0xFF);
      responseBuffer[2] = (uint8_t)((ms >> 16) & 0xFF);
      responseBuffer[3] = (uint8_t)((ms >> 24) & 0xFF);
      responseLength = 4;
      break;
    }

    case CMD_ECHO: {
      SERIALPRINT_LN(F("[I2C] CMD_ECHO"));
      for (uint8_t i = 0; i < 8; i++) {
        responseBuffer[i] = (Wire.available()) ? Wire.read() : 0x00;
      }
      responseLength = 8;
      break;
    }

    // Test sending data to the device, in the tests max size is 16 including the address, command etc
    // Bus Pirate test command
    // [0x10 0x90 0x02 0x03 0x02 0x03 0x02 0x03 0x02 0x06 0x02 0x03 0x02 0x03 0x62 0x77 r:4]
    // Returns: AA 0E FA 15
    case CMD_TEST_SEND: {
      SERIALPRINT_LN(F("[I2C] CMD_TEST_SEND"));
      responseLength = 4;

      uint8_t buf[128];
      uint8_t c = 0;
      uint8_t checksum = 0;   // really simple addition checksum that is allowed to overflow
      while (Wire.available() && c <128) {
        buf[c] = Wire.read();
        checksum += buf[c];
        c++;
        SERIALPRINT(".");
      }
      SERIALPRINT(F("Total bytes received: ")); SERIALPRINT_LN(c);

      responseBuffer[0] = 0xAA;
      responseBuffer[1] = c;
      responseBuffer[2] = checksum;
      responseBuffer[3] = crc8_maxim(buf, c);
      break;
    }

    // ===== Data Transfer Commands =====

    case CMD_BEGIN_DATA:
      SERIALPRINT_LN(F("[I2C] CMD_BEGIN_DATA"));
      responseLength = 1;
      if(LoopState == LOOP_STATE_DO_WORK) {
        SERIALPRINT_LN(F("  Error - doing work"));
        responseBuffer[0] = ERROR_NOT_READY;
      }
      else {
        // Reset job state
        currentJob.foundNonce = 0;
        rxBytesIn = 0;
        responseBuffer[0] = 0xAA;
      }
      break;

    // Just receive data from the master and put it in our Rx buffer
    case CMD_SEND_DATA: {
      SERIALPRINT_LN(F("[I2C] CMD_SEND_DATA"));
      responseLength = 3;     // Always start with the expected response size

      if(numBytes < 2) {
        responseBuffer[0] = ERROR_FRAME_TOO_SMALL;
        SERIALPRINT_LN(F("    - Error packet too small"));
        break;
      }

      uint8_t seq = Wire.read();
      uint8_t data = Wire.read();
      while (Wire.available()) Wire.read(); // clear anything else

      SERIALPRINT("    Got seq: ");
      SERIALPRINT(seq);
      SERIALPRINT(" Data: ");
      SERIALPRINT_HEX(data);
      SERIALPRINT_LN();

      if(seq != rxBytesIn) {
        responseBuffer[0] = ERROR_OUT_OF_SEQ;
        responseBuffer[1] = rxBytesIn;
        responseBuffer[2] = seq;
        break;
      }
      rxBuf[rxBytesIn++] = data;
      // Respond with ok
      responseBuffer[0] = 0xAA;
      responseBuffer[1] = seq;
      responseBuffer[2] = data;
      break;
    }

    case CMD_END_DATA: {
      SERIALPRINT_LN(F("[I2C] CMD_END_DATA"));
      responseLength = 1;     // Always start with the expected response size
      if(numBytes < 1 || !Wire.available()) {
        responseBuffer[0] = ERROR_NO_CRC;
        break;
      }

      currentJob.crc8 = crc8_maxim((const uint8_t*)rxBuf, rxBytesIn);
      uint8_t sentCRC = Wire.read();
      SERIALPRINT("    Got crc8: 0x");
      SERIALPRINT_HEX(sentCRC);
      SERIALPRINT(" exp: ");
      SERIALPRINT_HEX(currentJob.crc8);
      SERIALPRINT_LN();

      // if(0 == currentJob.crc8) {
      //   responseBuffer[0] = ERROR_NOT_READY;
      //   break;
      // }

      if(sentCRC != currentJob.crc8) {
        SERIALPRINT_LN("CRC WRONG");
        responseBuffer[0] = ERROR_CRC_MISMATCH;
        break;
      }

      // SERIALPRINT_LN("CRC OK");
      LoopState = LOOP_STATE_PROCESS_PKT_IN;
      responseBuffer[0] = 0xAA;
      }
      break;

    case CMD_TEST_DUMP_DATA:
      SERIALPRINT_LN(F("[I2C] CMD_TEST_DUMP_DATA"));
      responseLength = 0;
      for(uint8_t i = 0; i < rxBytesIn; i++) {
        char hex[4];
        sprintf(hex, "%.2X ", rxBuf[i]);
        SERIALPRINT( hex );
      }
      SERIALPRINT_LN(" |END");
      break;
    
    case CMD_GET_IS_IDLE:
      SERIALPRINT_LN(F("[I2C] CMD_GET_IS_IDLE"));
      responseLength = 1;
      responseBuffer[0] = (LoopState == LOOP_STATE_IDLE) ? 0xAA : 0xEE;
      break;

    case CMD_GET_JOB_STATUS:
      SERIALPRINT_LN(F("[I2C] CMD_GET_JOB_STATUS"));
      responseLength = 4;
      responseBuffer[0] = (LoopState != LOOP_STATE_DO_WORK && currentJob.foundNonce != 0) ? 0xAA : 0xDD;
      responseBuffer[1] = (uint8_t)(currentJob.foundNonce & 0xFF);
      responseBuffer[2] = (uint8_t)((currentJob.foundNonce >> 8) & 0xFF);
      responseBuffer[3] = currentJob.jobTimeTakenMs;
      break;
      
    case CMD_GET_JOB_DATA:
      SERIALPRINT_LN(F("[I2C] CMD_GET_JOB_DATA"));
      responseLength = 18;
      // Return job data for verification (18 bytes total)
      // First 8 bytes of prevHash
      memcpy((void*)responseBuffer, (void*)currentJob.prevHash, 7);
      // First 8 bytes of expectedHash
      memcpy((void*)(responseBuffer + 8), (void*)currentJob.expectedHash, 7);
      // Difficulty
      responseBuffer[14] = currentJob.difficulty;
      // Packet status
      responseBuffer[15] = rxBytesIn;
      break;
      
    default:
      responseBuffer[0] = 0xFF; // Unknown command
      responseLength = 1;
      break;
  }
  
  // Consume any remaining bytes to clear buffer
  while (Wire.available()) {
    Wire.read();
  }
}

void requestEvent() {
  // Send the prepared response
  if (responseLength > 0) {
    Wire.write((uint8_t*)responseBuffer, responseLength);

    SERIALPRINT(F("[I2C] Sending Response Data: "));
    #if defined(SERIAL_PRINT)
      for(uint8_t i = 0; i < responseLength; i++) {
          char hex[4];
          sprintf(hex, "%.2X ", responseBuffer[i]);
          SERIALPRINT( hex );
      }
    SERIALPRINT_LN();
    #endif
  }
}
