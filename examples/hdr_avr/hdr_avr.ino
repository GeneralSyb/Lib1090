/* Arduino Includes */
#include "Lib1090.h"
#include <SPI.h>

/* ---- Pin configuration ---- */
static const uint8_t PIN_SPI_SCK  = 40; // SPI Clock
static const uint8_t PIN_SPI_MOSI = 41; // SPI Master Out Slave In
static const uint8_t PIN_SPI_MISO = 42; // SPI Master In Slave Out
static const uint8_t PIN_LR_NSS   = 38; // LR2021 Chip Select
static const uint8_t PIN_LR_BUSY  = 47; // LR2021 Busy Input
static const uint8_t PIN_LR_RESET = 2;  // LR2021 NRESET Output
static const uint8_t PIN_LR_DIO   = 1;  // Microcontroller DIO Input
static const uint8_t LR_DIO_NUM   = 5;  // LR2021 IRQ DIO Number

/* Globals */
SPIClass LrSPI(FSPI); // Use FSPI or HSPI on ESP32-C6 or S3, platform dependent
Lib1090Driver lr(LrSPI, PIN_LR_NSS, PIN_LR_BUSY, PIN_LR_RESET, PIN_LR_DIO, LR_DIO_NUM);
volatile int nPackets = 0;

// IRAM_ATTR wrap in case it is being compiled for something other than an ESP32
#ifndef IRAM_ATTR
  #define IRAM_ATTR
#endif

void IRAM_ATTR onIRQ() {
  noInterrupts();
  nPackets = nPackets + 1;
  interrupts();
}

void sendAvrFrame(const adsb_raw_frame_t &frame) {
  static const char hexChars[] = "0123456789ABCDEF";
  Serial.print('*');
  for (uint8_t i = 0; i < frame.len; i++) {
    uint8_t b = frame.bytes[i];
    Serial.print(hexChars[b >> 4]);
    Serial.print(hexChars[b & 0x0F]);
  }
  Serial.println(';');
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Certain boards don't come with external trim capacitors, because the LR2021 has internal configurable trim capacitors.
  // In that case uncomment the line below and replace the values with the correct values for your oscillator.
  // See datasheet for more information.
  lr.setXoscCpTrim(10, 10, 100);
  LrSPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_LR_NSS); // Start SPI on custom pins
  lr.begin(7, 0); // Start LR2021 with maximum LNA gain and automatic gain control
  lr.setHdrSync(17); // DF17 preamble extension for AGC settling on strong signals
  attachInterrupt(digitalPinToInterrupt(PIN_LR_DIO), onIRQ, RISING); // Attach rising edge interrupt to IRQ pin
}

void loop() {
  noInterrupts();
  uint8_t pending = nPackets;
  interrupts();

  // Check if packets are ready for retrieval from RX FIFO
  if (pending != 0) {
    adsb_raw_frame_t f;
    int8_t result = lr.receiveFrame(&f);

    if (result == ADSB_NO_ERROR && CheckAdsbFrame(f.bytes, f.len)) {
      sendAvrFrame(f);
    }

    noInterrupts();
    nPackets--;
    interrupts();
  }
}
