<img width="3780" height="945" alt="Lib190_Logo" src="https://github.com/user-attachments/assets/e741df39-a80f-4a59-8ff0-9ba5fc2facf8" />

# Lib1090
Lib1090 is a simple Arduino library that turns an LR2021 into a 1090 MHz ADS-B receiver.

# Hardware
The LR2021 from Semtech is not meant for receiving ADS-B data at 1090 MHz. It's primarily a LoRa and NTN transceiver for the 150-960 MHz and 1500-2500 MHz bands, however the LO can be programmed anywhere from 150 to 2500 MHz.

Furthermore the LR2021 is one of the few off the shelve transceiver chips capable of receiving OOK modulated signals at a rate of 2 mbit/s (manchester encoded, thus effective datarate is 1 mbit/s) in the correct format.

## RF matching
Most LR2021 modules are designed for a number of bands, none of which cover 1090 MHz required for ADS-B reception. While these modules will receive data, the range will be greatly reduced.

The exact characteristics of the LR2021's PA and LNA are not public information, which means that making a matching circuit based on just the datasheet alone is not possible. Application not AN923.2 from Silicon Labs provides us a partial anwser. It says that the LNA in some of their EFR32-series RF-microcontrollers use a high impedance design. In this configuration there is no RF switch between the RX and TX pins of the transceiver. To be able to do this you need to have a high-impedance input for the RX pin. That makes a complex conjugate match basically impossible though. The reason why they chose this design is so that the chip can be used with a direct-tie configuration. They say that the matching circuit should aim for the highest possible voltage gain rather than a high power gain.

More information about how the required components are calculated can be found in the application now, specifically section 3.10.2 "RX Matching Network Design for EFR32xG23/28 and EFR32xG25". The situation seemed to match what was measured on the LR2021. During testing it was found that the following matching circuit worked well for 1090 MHz:
<img width="1352" height="595" alt="image" src="https://github.com/user-attachments/assets/c55ae967-74d6-4be2-92b1-81fe74d077d4" />
It's worth mentioning that C1 and C2 are most of the time not needed, only if a match is not possible (due to high parasitics for example) should they be populated. The Value of L1 can also depend on the parasitics of the board used, but anything in the range of 12 to 15 nH seemed to work well during testing.
