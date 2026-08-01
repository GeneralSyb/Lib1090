#ifndef LIB1090_DRIVER_H_
#define LIB1090_DRIVER_H_
/* C++ Includes */

/* Arduino Includes */
#include <Arduino.h>
#include <SPI.h>

/* Project Includes */

/* Global Defines */
#define LR2021_NO_ERROR   0
#define LR2021_TIMEOUT   -1

#define ADSB_NO_ERROR           0
#define ADSB_RX_TIMEOUT        -1
#define ADSB_CRC_ERROR         -2
#define ADSB_SPURIOUS_DIO      -3
#define ADSB_INCORRECT_LENGTH  -4

#define ADSB_CRC24_POLY 0x00FFF409UL

/* Global tyepdef & enums */

typedef struct {
    uint8_t bytes[14];
    uint8_t len;
    int16_t rssi;
    uint8_t lqi;
} adsb_raw_frame_t;

/* Classes & Functions */
class Lib1090Driver {
public:
    /* Lib1090 Class */
    Lib1090Driver(SPIClass &spi, uint8_t nssPin, uint8_t busyPin, uint8_t resetPin, uint8_t dioPin, uint8_t dioNum, SPISettings spiSettings = SPISettings(1000000, MSBFIRST, SPI_MODE0));

    /* Some board may need custom oscillator load capacitor trim settings set before starting anything else */
    int setXoscCpTrim(uint8_t xta, uint8_t xtb, uint8_t additionalStartTime);

    /* Init function for starting ADS-B reception */
    void begin(uint8_t rxBoost, uint8_t gain);

    /* Call this function to retrieve ADS-B frame after RX interrupt */
    int8_t receiveFrame(adsb_raw_frame_t *out);

    /* Runtime LR2021 settings adjustment function */
    // Manual gain, 0..13, 0 = automatic gain control
    void setAgcGainManual(uint8_t gainStep);

    // Manual LNA gain adjustment, 0..7
    void setRxBoost(uint8_t rxBoost);

    /* Get status of the LR2021 */
    String logStatus(const char *label);

    uint8_t dioPin() const {return _dio; }

private:
    SPIClass &_spi;
    SPISettings _spiSettings;
    uint8_t _nss, _busy, _reset, _dio, _dionum;

    int waitBusy(uint32_t timeoutMs);
    int spiWrite(const uint8_t *bytes, uint16_t len);
    int spiRead(const uint8_t *req, uint16_t reqLen, uint8_t *rsp, uint16_t rspLen);

    int clearErrors();
    int clearIrq(uint32_t irqs);
    uint32_t getAndClearIrq();
    int setDioFunction(uint8_t dioNum, uint8_t dioFunc, uint8_t pullDrive);
    int setDioIrqConfig(uint8_t dioNum, uint32_t irqs);

    int setPacketType(uint8_t packetType);
    int setRfFrequency(uint32_t frequency);
    void setRxPath(uint8_t rxPath, uint8_t rxBoost);
    int calibFe();
    int setRx(uint32_t rxTimeout);
    uint16_t getRxPktLength();

    int setOokModulationParams(uint32_t bitrate, uint8_t pulseShape, uint8_t rxBw);
    int setOokPacketParams(uint16_t preLenTx, uint8_t addrComp, uint8_t pktFormat, uint16_t pldLen, uint8_t crc, uint8_t encoding);
    int setOokCrcParams(uint32_t polynom, uint32_t init);
    int setOokSyncword(uint32_t syncword, uint8_t bitOrder, uint8_t nbBits);
    int setOokDetector(uint16_t preamblePattern, uint8_t patternLength, uint8_t patternNumRepeats, uint8_t swIsRaw, uint8_t sfdKind, uint8_t sfdLength);
    int getOokPacketStatus(uint16_t *pktLenOut, int16_t *rssiDbmOut, uint8_t *lqiOut);
    int readRxFifo(uint8_t *buffer, uint16_t length);
    void forceCrcOut();
    void clearRxFifo();
};

/* ADS-B Helpers */
uint32_t AdsbCrc24(const uint8_t *data, uint8_t len);
bool CheckAdsbFrame(const uint8_t *frame, uint8_t len);

#endif
