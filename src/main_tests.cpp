#if defined(RUN_TEST)

#include "config.h"
#include "slavedevice.h"
#include "runevery.h"
#include "duco_hash.h"

#include <Arduino.h>
#include <Wire.h>

RunEvery TestLoop(10000);
static constexpr unsigned int TEST_ITERATIONS = 1000;

struct TEST_VECTORS_DUCOWORK {
  char lastBlockHash[41];
  char newBlockHash[41];
  uint8_t lastBlockHash20[20];
  uint8_t newBlockHash20[20];
  uint8_t difficulty;
  uint16_t expected_nonce;
};

// Initialise an array of test vectors
const TEST_VECTORS_DUCOWORK testVectors[] = {
  {
    "bf55bad9a75c5b375a1457b0a252d75d60abce13",
    "e6a97a9227ad70219a95323a822a7074d813248b",
    {0xbf,0x55,0xba,0xd9,0xa7,0x5c,0x5b,0x37,0x5a,0x14,0x57,0xb0,0xa2,0x52,0xd7,0x5d,0x60,0xab,0xce,0x13},
    {0xe6,0xa9,0x7a,0x92,0x27,0xad,0x70,0x21,0x9a,0x95,0x32,0x3a,0x82,0x2a,0x70,0x74,0xd8,0x13,0x24,0x8b},
    10,
    818
  },
  {
    "195a9a7486e54acf0dcd7d0964b4d92cad35580b",
    "6e12d8481d8afd7c7f06d11c874a9d040f78b846",
    {0x19,0x5a,0x9a,0x74,0x86,0xe5,0x4a,0xcf,0x0d,0xcd,0x7d,0x09,0x64,0xb4,0xd9,0x2c,0xad,0x35,0x58,0x0b},
    {0x6e,0x12,0xd8,0x48,0x1d,0x8a,0xfd,0x7c,0x7f,0x06,0xd1,0x1c,0x87,0x4a,0x9d,0x04,0x0f,0x78,0xb8,0x46},
    10,
    528
  },
  {
    "f3fde8081a484d47130c651e788daf6b7afd3634",
    "ebcb0745e17b6ea473c2a0e2d0d196cbbadb5f2a",
    {0xf3,0xfd,0xe8,0x08,0x1a,0x48,0x4d,0x47,0x13,0x0c,0x65,0x1e,0x78,0x8d,0xaf,0x6b,0x7a,0xfd,0x36,0x34},
    {0xeb,0xcb,0x07,0x45,0xe1,0x7b,0x6e,0xa4,0x73,0xc2,0xa0,0xe2,0xd0,0xd1,0x96,0xcb,0xba,0xdb,0x5f,0x2a},
    10,
    806
  }
};
const size_t NUM_TEST_VECTORS = sizeof(testVectors) / sizeof(testVectors[0]);

void test_origduco();
uint16_t test_origduco_findnonce();
void test_fastduco();

// // Give each Nano a unique 7-bit address (e.g. 0x30..0x39 for your 10 nodes)
// #ifndef I2C_ADDR
// #define I2C_ADDR 0x30
// #endif

// // Tiny mailbox
// volatile uint8_t lastCmd = 0;
// volatile uint8_t rxBuf[16];
// volatile uint8_t rxLen = 0;
// uint8_t txBuf[16];
// uint8_t txLen = 0;

// // ----- Callbacks -----
// void onReceiveService(int howMany) {
//   Serial.println(F("Nano I2C Slave Receive Service"));

//   rxLen = 0;
//   while (Wire.available() && rxLen < sizeof(rxBuf)) {
//     rxBuf[rxLen++] = (uint8_t)Wire.read();
//   }
//   if (rxLen > 0) {
//     lastCmd = rxBuf[0];
//     // Very simple protocol:
//     // CMD 0x01 = assign job: [0x01, jobId, payloadLen, ...payload]
//     if (rxLen >= 3 && lastCmd == 0x01) {
//       uint8_t jobId = rxBuf[1];
//       uint8_t plen  = rxBuf[2];
//       // Do the "work" outside ISR; here we just stash jobId
//       // For demo, prepare a tiny result now
//       txBuf[0] = 1;        // ready flag
//       txBuf[1] = jobId;    // echo jobId
//       txBuf[2] = 1;        // result length
//       txBuf[3] = 0x42;     // result byte
//       txLen = 4;
//     }
//   }
// }

// void onRequestService() {
//   Serial.println(F("Nano I2C Slave Request Service"));

//   // Master will typically have sent a CMD_STATUS? (0x02) just before read
//   // Reply with [ready, jobId, len, ...data] or [0] if nothing
//   if (txLen) {
//     for (uint8_t i = 0; i < txLen; ++i) Wire.write(txBuf[i]);
//     txLen = 0;  // one-shot
//   } else {
//     uint8_t notReady = 0;
//     Wire.write(&notReady, 1);
//   }
// }

static void disableInternalPullupsOnI2C()
{
  // On ATmega328P, Wire enables internal pull-ups by setting PORTC4/5.
  // Clear them so the bus idles at the external 3.3 V pull-ups only.
  // SDA = PC4, SCL = PC5
  PORTC &= ~(_BV(PORTC4) | _BV(PORTC5));
}

static void bytes_to_hex(const uint8_t* in, size_t len, char* out40) {
  static const char* L="0123456789abcdef";
  for (size_t i=0;i<len;++i){ out40[2*i]=L[in[i]>>4]; out40[2*i+1]=L[in[i]&0xF]; }
  out40[2*len]='\0';
}

void setup() {
  // Optional serial for debug
  SERIALBEGIN(115200);
  delay(10);
  Serial.println(F("Nano I2C Slave up"));

  Serial.print(F("I2C addr: 0x"));
  Serial.println(I2C_ADDR, HEX);

  // // I2C slave at I2C_ADDR
  // Wire.begin(I2C_ADDR);           // Nano as SLAVE
  // Wire.onReceive(onReceiveService);
  // Wire.onRequest(onRequestService);

  // Kill the default internal 5 V pull-ups that Wire turns on
  disableInternalPullupsOnI2C();

  uint16_t n = test_origduco_findnonce();
  if(n != testVectors[0].expected_nonce) {
    SERIALPRINT_LN(F("Failed to find expected nonce"))
  }
}

void loop() {
  slave_loop();

  if(TestLoop.due()) {
    SERIALPRINT_LN(F("********* START TESTS *********"));
    if(test_origduco_findnonce() != testVectors[0].expected_nonce) {
      SERIALPRINT_LN(F("Failed to find expected nonce"));
    }
    #if defined(SHA1_DUCO)
      test_origduco();
    #endif
    //test_fastduco();
    SERIALPRINT_LN(F("*********  END TESTS  *********\n"));
  }
  delay(2);
}

void test_origduco() {
  SERIALPRINT_LN(F("Starting DUCO ORIG ..."));

  unsigned int iter = TEST_ITERATIONS;
  static duco_hash_state_t hash;

  unsigned long start = millis();
  duco_hash_init(&hash, testVectors[0].lastBlockHash);

  for(int x=0; x < iter; x++) {
    char nonceStr[10 + 1];
    ultoa(testVectors[0].expected_nonce, nonceStr, 10);
    uint8_t const * hash_bytes = duco_hash_try_nonce(&hash, nonceStr);
    if (memcmp(hash_bytes, testVectors[0].newBlockHash20, 20) != 0) {
      SERIALPRINT_LN("Hash with expected nonce failed, which is weird !");

      char out[41];
      bytes_to_hex(hash_bytes, 20, out);
      SERIALPRINT_LN(out);
    }
  }
  unsigned long stop = millis();
  unsigned long t = stop - start;

  char strBuf[128];
  double hps = ((double)iter / ((double)t/1000));
  sprintf(strBuf, "Iterations: = %u  Time: %lu ms. Hashes: %.3f /s", iter, t, hps);
  SERIALPRINT_LN( strBuf );
}

uint16_t test_origduco_findnonce() {
  SERIALPRINT("Starting DUCO find nonce... ");

  unsigned long start = millis();

  static duco_hash_state_t hash;
  duco_hash_init(&hash, testVectors[0].lastBlockHash);

  char nonceStr[10 + 1];
  for (uint16_t nonce = 0; nonce < (uint16_t)testVectors[0].difficulty*100+1; nonce++) {
    ultoa(nonce, nonceStr, 10);
    uint8_t const * hash_bytes = duco_hash_try_nonce(&hash, nonceStr);

    if (memcmp(hash_bytes, testVectors[0].newBlockHash20, 20) == 0) {
      SERIALPRINT("Found nonce: ");
      SERIALPRINT_LN(nonce);
      return nonce;
    }
  }

  SERIALPRINT_LN("Failed to find nonce: ");
  return 0;
}

void test_fastduco() {
  SERIALPRINT_LN("Starting DUCO FAST");

  // int iter = TEST_ITERATIONS;

  // unsigned long start = millis();
  // for(int x=0; x < iter; x++) {
  //   //const char *m = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  //   String m = testVectors[0].lastBlockHash + String(testVectors[0].expected_nonce);

  //   uint8_t hash_bytes[20];
  //   char hex[41];
  //   sha1_t85((const uint8_t*)m.c_str(), m.length() , hash_bytes);
    
  //   if (memcmp(hash_bytes, testVectors[0].newBlockHash20, 20) != 0) {
  //     SERIALPRINT_LN("Hash with expected nonce failed, which is weird !");
  //     SERIALPRINT_LN(" -  last+nonce: " + m);
  //     char out[41];
  //     bytes_to_hex(hash_bytes, 20, out);
  //     SERIALPRINT_LN(out);
  //     break;
  //   }
  // }
  // unsigned long stop = millis();
  // unsigned long t = stop - start;

  // SERIALPRINT_LN(String("Iterations: ") + String(iter) + " Time: " + String(t) + "ms. Hashes: " +String(iter / (t/1000)) + "/s" );
}

#endif