#include "Lib1090.h"

/* C++ Includes */

/* Arduino Includes */
#include <string.h>

/* Project Includes */

/* Constants */
static const uint8_t  RX_BW_3076       = 0x00;
static const uint8_t  PULSE_SHAPE_NONE = 0x00;

static const uint8_t  ENCODING_MANCH_INV = 0x09;
static const uint8_t  CRC_3BYTE          = 0x03;
static const uint8_t  SFD_FALLING_EDGE   = 0x00;
static const uint8_t  BIT_ORDER_LSB      = 0x00;
static const uint8_t  ADDR_COMP_OFF      = 0x00;
static const uint8_t  PKT_FORMAT_FIXED   = 0x00;

static const uint8_t  PACKET_TYPE_OOK    = 0x0A;

static const uint8_t  DIO_FUNC_IRQ    = 0x01;
static const uint8_t  PULL_DRIVE_UP   = 0x02;

static const uint32_t IRQ_MASK_RX_DONE   = 0x00040000UL;
static const uint32_t IRQ_MASK_CRC_ERROR = 0x00400000UL;
static const uint32_t IRQ_MASK_TIMEOUT   = 0x00200000UL;
static const uint32_t IRQ_MASK_OOK_RX    = IRQ_MASK_RX_DONE | IRQ_MASK_CRC_ERROR | IRQ_MASK_TIMEOUT;


/* Constructor */
Lib1090Driver::Lib1090Driver(SPIClass &spi, uint8_t nssPin, uint8_t busyPin, uint8_t resetPin, uint8_t dioPin, uint8_t dioNum, SPISettings spiSettings) : _spi(spi), _spiSettings(spiSettings), _nss(nssPin), _busy(busyPin), _reset(resetPin), _dio(dioPin), _dionum(dioNum) {
}

/* Low Level SPI Helpers */
int Lib1090Driver::waitBusy(uint32_t timeoutMs)
{
    uint32_t t0 = millis();
    while (digitalRead(_busy) == HIGH)
        if (millis() - t0 > timeoutMs) return LR2021_TIMEOUT;
    return LR2021_NO_ERROR;
}

int Lib1090Driver::spiWrite(const uint8_t *bytes, uint16_t len)
{
    if (waitBusy(10) != LR2021_NO_ERROR) return -1;

    digitalWrite(_nss, LOW);
    _spi.beginTransaction(_spiSettings);
    _spi.writeBytes(bytes, len);
    _spi.endTransaction();
    digitalWrite(_nss, HIGH);

    /* Wait for command acceptance */
    uint32_t t0 = millis();
    while (digitalRead(_busy) == LOW) {
        if ((millis() - t0) > 10) break;
    }
    return ADSB_NO_ERROR;
}

int Lib1090Driver::spiRead(const uint8_t *req, uint16_t reqLen, uint8_t *rsp, uint16_t rspLen)
{
    if (waitBusy(10) != LR2021_NO_ERROR) return -1;
    digitalWrite(_nss, LOW);
    _spi.beginTransaction(_spiSettings);
    _spi.writeBytes(req, reqLen);
    _spi.endTransaction();
    digitalWrite(_nss, HIGH);

    if (waitBusy(10) != LR2021_NO_ERROR) return -1;
    digitalWrite(_nss, LOW);
    _spi.beginTransaction(_spiSettings);
    /* The LR2021 requires MOSI to be strictly 0x00 during read phases */
    memset(rsp, 0x00, rspLen);
    _spi.transfer(rsp, rspLen);
    _spi.endTransaction();
    digitalWrite(_nss, HIGH);
    return ADSB_NO_ERROR;
}

/* LR2021 Commands */
int Lib1090Driver::clearErrors()
{
    uint8_t cmd[2] = {0x01, 0x11};
    return spiWrite(cmd, 2);
}

int Lib1090Driver::clearIrq(uint32_t irqs)
{
    uint8_t cmd[6] = {
        0x01, 0x16,
        (uint8_t)(irqs >> 24), (uint8_t)(irqs >> 16),
        (uint8_t)(irqs >>  8), (uint8_t)(irqs)
    };
    return spiWrite(cmd, 6);
}

uint32_t Lib1090Driver::getAndClearIrq()
{
    uint8_t req[2] = {0x01, 0x17};
    uint8_t rsp[6] = {0};
    if (spiRead(req, 2, rsp, 6) != LR2021_NO_ERROR) return (uint32_t)-1;
    return ((uint32_t)rsp[2] << 24) | ((uint32_t)rsp[3] << 16)
         | ((uint32_t)rsp[4] <<  8) |  (uint32_t)rsp[5];
}

int Lib1090Driver::setDioFunction(uint8_t dioNum, uint8_t dioFunc, uint8_t pullDrive)
{
    uint8_t cmd[4] = {
        0x01, 0x12,
        (uint8_t)(dioNum & 0x0F),
        (uint8_t)(((dioFunc & 0x0F) << 4) | (pullDrive & 0x0F))
    };
    return spiWrite(cmd, 4);
}

int Lib1090Driver::setDioIrqConfig(uint8_t dioNum, uint32_t irqs)
{
    uint8_t cmd[7] = {
        0x01, 0x15,
        (uint8_t)(dioNum & 0x0F),
        (uint8_t)(irqs >> 24), (uint8_t)(irqs >> 16),
        (uint8_t)(irqs >>  8), (uint8_t)(irqs)
    };
    return spiWrite(cmd, 7);
}

int Lib1090Driver::setXoscCpTrim(uint8_t xta, uint8_t xtb, uint8_t additionalStartTime)
{
    uint8_t cmd[5] = {
        0x01, 0x31,
        (uint8_t)(xta & 0b00111111),
        (uint8_t)(xtb & 0b00111111),
        additionalStartTime
    };
    return spiWrite(cmd, 5);
}

int Lib1090Driver::setPacketType(uint8_t packetType)
{
    uint8_t cmd[3] = {0x02, 0x07, packetType};
    return spiWrite(cmd, 3);
}

int Lib1090Driver::setRfFrequency(uint32_t frequency)
{
    uint8_t cmd[6] = {
        0x02, 0x00,
        (uint8_t)(frequency >> 24), (uint8_t)(frequency >> 16),
        (uint8_t)(frequency >>  8), (uint8_t)(frequency)
    };
    return spiWrite(cmd, 6);
}

void Lib1090Driver::setRxPath(uint8_t rxPath, uint8_t rxBoost)
{
    uint8_t cmd[4] = {
        0x02, 0x01,
        (uint8_t)(rxPath  & 0x01),
        (uint8_t)(rxBoost & 0x07)
    };
    spiWrite(cmd, 4);
}

void Lib1090Driver::setAgcGainManual(uint8_t gainStep)
{
    uint8_t cmd[3] = {0x02, 0x1A, (uint8_t)(gainStep & 0x0F)};
    spiWrite(cmd, 3);
}

int Lib1090Driver::calibFe()
{
    uint8_t cmd[8] = {0x01, 0x23, 0, 0, 0, 0, 0, 0};
    return spiWrite(cmd, 8);
}

int Lib1090Driver::setRx(uint32_t rxTimeout)
{
    uint8_t cmd[5] = {
        0x02, 0x0C,
        (uint8_t)((rxTimeout >> 16) & 0xFF),
        (uint8_t)((rxTimeout >>  8) & 0xFF),
        (uint8_t)( rxTimeout        & 0xFF)
    };
    return spiWrite(cmd, 5);
}

uint16_t Lib1090Driver::getRxPktLength()
{
    uint8_t req[2] = {0x02, 0x12};
    uint8_t rsp[4] = {0};
    if (spiRead(req, 2, rsp, 4) != LR2021_NO_ERROR) return (uint16_t)-1;
    return ((uint16_t)rsp[2] << 8) | (uint16_t)rsp[3];
}

int Lib1090Driver::setOokModulationParams(uint32_t bitrate, uint8_t pulseShape, uint8_t rxBw)
{
    uint8_t cmd[8] = {
        0x02, 0x81,
        (uint8_t)(bitrate >> 24), (uint8_t)(bitrate >> 16),
        (uint8_t)(bitrate >>  8), (uint8_t)(bitrate),
        (uint8_t)(pulseShape & 0x0F),
        rxBw
    };
    return spiWrite(cmd, 8);
}

int Lib1090Driver::setOokPacketParams(uint16_t preLenTx, uint8_t addrComp, uint8_t pktFormat, uint16_t pldLen, uint8_t crc, uint8_t encoding)
{
    uint8_t cmd[8] = {
        0x02, 0x82,
        (uint8_t)(preLenTx >> 8),
        (uint8_t)(preLenTx),
        (uint8_t)(((addrComp & 0x3) << 2) | (pktFormat & 0x3)),
        (uint8_t)(pldLen >> 8),
        (uint8_t)(pldLen),
        (uint8_t)(((crc & 0x0F) << 4) | (encoding & 0x0F))
    };
    return spiWrite(cmd, 8);
}

int Lib1090Driver::setOokCrcParams(uint32_t polynom, uint32_t init)
{
    uint8_t cmd[10] = {
        0x02, 0x83,
        (uint8_t)(polynom >> 24), (uint8_t)(polynom >> 16),
        (uint8_t)(polynom >>  8), (uint8_t)(polynom),
        (uint8_t)(init    >> 24), (uint8_t)(init    >> 16),
        (uint8_t)(init    >>  8), (uint8_t)(init)
    };
    return spiWrite(cmd, 10);
}

int Lib1090Driver::setOokSyncword(uint32_t syncword, uint8_t bitOrder, uint8_t nbBits)
{
    uint8_t cmd[7] = {
        0x02, 0x84,
        (uint8_t)(syncword >> 24), (uint8_t)(syncword >> 16),
        (uint8_t)(syncword >>  8), (uint8_t)(syncword),
        (uint8_t)(((bitOrder & 0x1) << 7) | (nbBits & 0x7F))
    };
    return spiWrite(cmd, 7);
}

int Lib1090Driver::setOokDetector(uint16_t preamblePattern, uint8_t patternLength, uint8_t patternNumRepeats, uint8_t swIsRaw, uint8_t sfdKind, uint8_t sfdLength)
{
    uint8_t flag6 = 0;
    if (swIsRaw) flag6 |= 0x20;
    flag6 |= (uint8_t)((sfdKind & 0x1) << 4);
    flag6 |= (sfdLength & 0x0F);

    uint8_t cmd[7] = {
        0x02, 0x88,
        (uint8_t)(preamblePattern >> 8),
        (uint8_t)(preamblePattern),
        (uint8_t)(patternLength & 0x0F),
        (uint8_t)(patternNumRepeats & 0x1F),
        flag6
    };
    return spiWrite(cmd, 7);
}

int Lib1090Driver::getOokPacketStatus(uint16_t *pktLenOut, int16_t *rssiDbmOut, uint8_t *lqiOut)
{
    uint8_t req[2] = {0x02, 0x87};
    uint8_t rsp[8] = {0};
    if (spiRead(req, 2, rsp, 8) != ADSB_NO_ERROR) return -1;
    if (pktLenOut)  *pktLenOut  = ((uint16_t)rsp[2] << 8) | rsp[3];
    if (rssiDbmOut) {
        uint16_t rssiRaw = ((uint16_t)rsp[4] << 1) | ((rsp[6] >> 2) & 0x1);
        *rssiDbmOut = -(int16_t)(rssiRaw >> 1);
    }
    if (lqiOut) *lqiOut = rsp[7];
    return ADSB_NO_ERROR;
}

int Lib1090Driver::readRxFifo(uint8_t *buffer, uint16_t length)
{
    static const uint8_t opcode[2] = {0x00, 0x01};
    if (waitBusy(10) != LR2021_NO_ERROR) return -1;

    digitalWrite(_nss, LOW);
    _spi.beginTransaction(_spiSettings);
    _spi.writeBytes(opcode, 2);
    /* The LR2021 requires MOSI to be strictly 0x00 during FIFO read */
    memset(buffer, 0x00, length);
    _spi.transfer(buffer, length);
    _spi.endTransaction();
    digitalWrite(_nss, HIGH);
    return ADSB_NO_ERROR;
}

void Lib1090Driver::forceCrcOut()
{
    uint8_t cmd[13] = {
        0x01, 0x05,
        0xF3, 0x08, 0x44,
        0x01, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };
    spiWrite(cmd, 13);
}

void Lib1090Driver::clearRxFifo()
{
    uint8_t cmd[2] = {0x01, 0x1E};
    spiWrite(cmd, 2);
}

String Lib1090Driver::logStatus(const char *label)
{
    String result;
    uint8_t req[2] = {0x01, 0x10};
    uint8_t rsp[4] = {0};

    if (spiRead(req, 2, rsp, 4) != LR2021_NO_ERROR) {
        result = "[LR2021] ";
        result += label;
        result += ": status read failed";
        return result;
    }

    uint16_t stat      = ((uint16_t)rsp[0] << 8) | rsp[1];
    uint16_t errorStat = ((uint16_t)rsp[2] << 8) | rsp[3];
    uint8_t  cmdStatus = (stat >> 9) & 0x07;
    uint8_t  chipMode  =  stat       & 0x07;

    static const char *cmdStr[]  = {"CMD_FAIL","CMD_PERR","CMD_OK","CMD_DAT","?","?","?","?"};
    static const char *modeStr[] = {"SLEEP","STDBY_RC","STDBY_XOSC","FS","RX","TX","?","?"};

    result = "[LR2021] ";
    result += label;
    result += ": CmdStatus=";
    result += cmdStr[cmdStatus & 0x7];
    result += " ChipMode=";
    result += modeStr[chipMode & 0x7];

    if (errorStat) {
        result += "  Errors 0x";
        if (errorStat < 0x1000) result += "0";
        if (errorStat < 0x0100) result += "0";
        if (errorStat < 0x0010) result += "0";
        result += String(errorStat, HEX);
        result += ":";
        if (errorStat & (1 <<  0)) result += " HF_XOSC_START";
        if (errorStat & (1 <<  1)) result += " LF_XOSC_START";
        if (errorStat & (1 <<  2)) result += " PLL_LOCK";
        if (errorStat & (1 <<  3)) result += " LF_RC_CALIB";
        if (errorStat & (1 <<  4)) result += " HF_RC_CALIB";
        if (errorStat & (1 <<  5)) result += " PLL_CALIB";
        if (errorStat & (1 <<  6)) result += " AAF_CALIB";
        if (errorStat & (1 <<  7)) result += " IMG_CALIB";
        if (errorStat & (1 <<  8)) result += " CHIP_BUSY";
        if (errorStat & (1 <<  9)) result += " RXFREQ_NO_FE_CAL";
        if (errorStat & (1 << 10)) result += " ADC_CALIB";
        if (errorStat & (1 << 11)) result += " PA_OFFSET_CALIB";
    }

    return result;
}

void Lib1090Driver::begin(uint8_t rxBoost, uint8_t gain)
{
    // Set pinmodes
    pinMode(_nss,   OUTPUT);
    pinMode(_reset, OUTPUT);
    pinMode(_busy,  INPUT);
    pinMode(_dio,   INPUT);

    // Reset LR2021
    digitalWrite(_nss, HIGH);
    digitalWrite(_reset, LOW);
    delay(10);
    digitalWrite(_reset, HIGH);
    delay(50); /* LR2021 startup time */

    // Clear errors before starting
    clearErrors();

    // Optional for some boards: set oscillator trim values, 10, 10, 100 is a good starting point for board without capacitors. See datasheet for more information.
    // setXoscCpTrim(10, 10, 100);

    // Set packet type to OOK (ADS-B uses Pulse Position Coding, which is basically OOK with manchester encoding)
    setPacketType(PACKET_TYPE_OOK);

    // Set RF frequency to 1090 Mhz
    setRfFrequency(1090000000UL);

    // Calibrate RX front end with selected frequency
    calibFe();

    // Wait until calibration is properly finished
    waitBusy(100);

    // Set the RX path to the low band, with selected RX boost (recommended is 0 for minimum power consumption, 7 for maximum boost)
    setRxPath(0x00, rxBoost);

    // Calibrate RX front end with selected settings
    calibFe();

    // Wait until calibration is properly finished
    waitBusy(100);

    // Set manual gain (recommended is 0 for automatic gain control)
    setAgcGainManual(gain);

    // Set modulation parameters
    setOokModulationParams(2000000UL, PULSE_SHAPE_NONE, RX_BW_3076);

    // Set OOK parameters
    setOokPacketParams(8, ADDR_COMP_OFF, PKT_FORMAT_FIXED, 11, CRC_3BYTE, ENCODING_MANCH_INV);

    // Force LR2021 to put received CRC into RX FIFO so that it doesn't have to be recalculated for AVR or Beast for example
    forceCrcOut();

    // Set OOK CRC
    setOokCrcParams(0x01FFF409UL, 0x00000000UL);

    // Set OOK syncword (no syncword used in this case)
    setOokSyncword(0x00000000UL, BIT_ORDER_LSB, 0);

    // Set OOK settings
    setOokDetector(0x0285, 15, 0, 0, SFD_FALLING_EDGE, 0);

    // Set function of DIO pin
    setDioFunction(_dionum, DIO_FUNC_IRQ, PULL_DRIVE_UP);

    // Clear OOK RX irq
    clearIrq(IRQ_MASK_OOK_RX);

    // Setting IRQ mode to output
    setDioIrqConfig(_dionum, IRQ_MASK_OOK_RX);

    // Set RX mode with maximum timeout
    setRx(0xFFFFFF);

    // Clear outstanding interrupts
    getAndClearIrq();
}

int8_t Lib1090Driver::receiveFrame(adsb_raw_frame_t *out)
{
    uint32_t irqs = getAndClearIrq();

    if (irqs & IRQ_MASK_TIMEOUT) return ADSB_RX_TIMEOUT;

    if (!_hdrActive && (irqs & IRQ_MASK_CRC_ERROR)) {
        clearRxFifo();
        return ADSB_CRC_ERROR;
    }
    if (!(irqs & IRQ_MASK_RX_DONE)) return ADSB_SPURIOUS_DIO;

    uint16_t scratch;
    getOokPacketStatus(&scratch, &out->rssi, &out->lqi);

    uint16_t pktLen = getRxPktLength();

    if (_hdrActive) {
        uint8_t raw[14] = {0};
        uint8_t rawLen = (pktLen > 14) ? 14 : (uint8_t)pktLen;
        readRxFifo(raw, rawLen);
        reconstructFrame(_hdrDfBits, raw, rawLen, out->bytes, 14);
        out->len = 14;
    } else {
        if (pktLen == 0 || pktLen > 14) return ADSB_INCORRECT_LENGTH;
        out->len = (uint8_t)pktLen;
        readRxFifo(out->bytes, out->len);
    }

    return ADSB_NO_ERROR;
}

void Lib1090Driver::setRxBoost(uint8_t rxBoost) {
    setRxPath(0x00, rxBoost);
}

int Lib1090Driver::setHdrSync(uint8_t df)
{
    _hdrDfBits = (df == 18) ? 0x12 : 0x11; // 0b10010 / 0b10001
    _hdrActive = true;
    return setOokSyncword(_hdrDfBits, BIT_ORDER_MSB, 5);
}

int Lib1090Driver::clearHdrSync()
{
    _hdrActive = false;
    return setOokSyncword(0, BIT_ORDER_LSB, 0);
}

/* ADS-B Helpers */
uint32_t AdsbCrc24(const uint8_t *data, uint8_t len) {
    uint32_t crc = 0;

    for (uint8_t i = 0; i < len; i++) {
        crc ^= ((uint32_t)data[i]) << 16;
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x00800000UL) {
                crc = (crc << 1) ^ ADSB_CRC24_POLY;
            } else {
                crc <<= 1;
            }
        }
        crc &= 0x00FFFFFFUL;
    }

    return crc;
}

bool CheckAdsbFrame(const uint8_t *frame, uint8_t len) {
    if (len < 11u) return false;
    if (AdsbCrc24(frame, 14) != 0) return false;

    uint8_t df = frame[0] >> 3;
    return (df == 17u || df == 18u);
}

void reconstructFrame(uint8_t df5, const uint8_t *fifoBytes, uint8_t fifoLen, uint8_t *out, uint8_t outLen)
{
    uint32_t acc = df5 & 0x1F;
    uint8_t accBits = 5;
    uint8_t outIdx = 0;

    for (uint8_t i = 0; i < fifoLen && outIdx < outLen; i++) {
        acc = (acc << 8) | fifoBytes[i];
        accBits += 8;
        while (accBits >= 8 && outIdx < outLen) {
            accBits -= 8;
            out[outIdx++] = (uint8_t)(acc >> accBits);
        }
    }
}
