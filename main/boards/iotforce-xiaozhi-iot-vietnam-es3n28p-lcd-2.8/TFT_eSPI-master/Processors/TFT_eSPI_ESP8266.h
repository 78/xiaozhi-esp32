        //////////////////////////////////////////////////////
        // TFT_eSPI driver functions for ESP8266 processors //
        //////////////////////////////////////////////////////

#ifndef _TFT_eSPI_ESP8266H_
#define _TFT_eSPI_ESP8266H_

// Processor ID reported by getSetup()
#define PROCESSOR_ID 0x8266

// Include processor specific header
// None

// Processor specific code used by SPI bus transaction startWrite and endWrite functions
#define SET_BUS_WRITE_MODE SPI1U=SPI1U_WRITE
#define SET_BUS_READ_MODE  SPI1U=SPI1U_READ

// Code to check if DMA is busy, used by SPI bus transaction transaction and endWrite functions
#define DMA_BUSY_CHECK // DMA not available, leave blank

// Initialise processor specific SPI functions, used by init()
#if (!defined (SUPPORT_TRANSACTIONS) && defined (ARDUINO_ARCH_ESP8266))
  #define INIT_TFT_DATA_BUS \
    spi.setBitOrder(MSBFIRST); \
    spi.setDataMode(TFT_SPI_MODE); \
    spi.setFrequency(SPI_FREQUENCY);
  #else
    #define INIT_TFT_DATA_BUS
#endif

// If smooth fonts are enabled the filing system may need to be loaded
#ifdef SMOOTH_FONT
  // Call up the SPIFFS FLASH filing system for the anti-aliased fonts
  #define FS_NO_GLOBALS
  #include <FS.h>
  #define FONT_FS_AVAILABLE
#endif

// Do not allow parallel mode for ESP8266
#ifdef ESP32_PARALLEL
  #undef ESP32_PARALLEL
#endif
#ifdef TFT_PARALLEL_8_BIT
  #undef TFT_PARALLEL_8_BIT
#endif

////////////////////////////////////////////////////////////////////////////////////////
// Define the DC (TFT Data/Command or Register Select (RS))pin drive code
////////////////////////////////////////////////////////////////////////////////////////
#ifndef TFT_DC
  #define DC_C // No macro allocated so it generates no code
  #define DC_D // No macro allocated so it generates no code
#else
  #if (TFT_DC == 16)
    #define DC_C digitalWrite(TFT_DC, LOW)
    #define DC_D digitalWrite(TFT_DC, HIGH)
  #else
    #define DC_C GPOC=dcpinmask
    #define DC_D GPOS=dcpinmask
  #endif
#endif

////////////////////////////////////////////////////////////////////////////////////////
// Define the CS (TFT chip select) pin drive code
////////////////////////////////////////////////////////////////////////////////////////
#ifndef TFT_CS
  #define CS_L // No macro allocated so it generates no code
  #define CS_H // No macro allocated so it generates no code
#else
  #if (TFT_CS == 16)
    #define CS_L digitalWrite(TFT_CS, LOW)
    #define CS_H digitalWrite(TFT_CS, HIGH)
  #else
    #define CS_L GPOC=cspinmask
    #define CS_H GPOS=cspinmask
  #endif
#endif

////////////////////////////////////////////////////////////////////////////////////////
// Define the WR (TFT Write) pin drive code
////////////////////////////////////////////////////////////////////////////////////////
#ifdef TFT_WR
  #define WR_L GPOC=wrpinmask
  #define WR_H GPOS=wrpinmask
#endif

////////////////////////////////////////////////////////////////////////////////////////
// Define the touch screen chip select pin drive code
////////////////////////////////////////////////////////////////////////////////////////
#ifndef TOUCH_CS
  #define T_CS_L // No macro allocated so it generates no code
  #define T_CS_H // No macro allocated so it generates no code
#else
  #define T_CS_L digitalWrite(TOUCH_CS, LOW)
  #define T_CS_H digitalWrite(TOUCH_CS, HIGH)
#endif

////////////////////////////////////////////////////////////////////////////////////////
// Make sure TFT_MISO is defined if not used to avoid an error message
////////////////////////////////////////////////////////////////////////////////////////
#ifndef TFT_MISO
  #define TFT_MISO -1
#endif

////////////////////////////////////////////////////////////////////////////////////////
// ESP8266 specific SPI macros
////////////////////////////////////////////////////////////////////////////////////////
#if defined (TFT_SPI_OVERLAP)
  #undef TFT_CS
  #define SPI1U_WRITE (SPIUMOSI | SPIUSSE | SPIUCSSETUP | SPIUCSHOLD)
  #define SPI1U_READ  (SPIUMOSI | SPIUSSE | SPIUCSSETUP | SPIUCSHOLD | SPIUDUPLEX)
#else
  #define SPI1U_WRITE (SPIUMOSI | SPIUSSE)
  #define SPI1U_READ  (SPIUMOSI | SPIUSSE | SPIUDUPLEX)
#endif

////////////////////////////////////////////////////////////////////////////////////////
// Macros to write commands/pixel colour data to a SPI ILI948x TFT
////////////////////////////////////////////////////////////////////////////////////////
#if  defined (SPI_18BIT_DRIVER) // SPI 18-bit colour

  // Write 8 bits to TFT
  #define tft_Write_8(C)   spi.transfer(C)

  // Convert 16-bit colour to 18-bit and write in 3 bytes
  #define tft_Write_16(C)  spi.transfer(((C) & 0xF800)>>8); \
                           spi.transfer(((C) & 0x07E0)>>3); \
                           spi.transfer(((C) & 0x001F)<<3)

  // Convert swapped byte 16-bit colour to 18-bit and write in 3 bytes
  #define tft_Write_16S(C) spi.transfer((C) & 0xF8); \
                           spi.transfer(((C) & 0xE000)>>11 | ((C) & 0x07)<<5); \
                           spi.transfer(((C) & 0x1F00)>>5)

  // Write 32 bits to TFT
  #define tft_Write_32(C)  spi.write32(C)

  // Write two address coordinates
  #define tft_Write_32C(C,D) spi.write32((C)<<16 | (D))
  
  // Write same value twice
  #define tft_Write_32D(C) spi.write32((C)<<16 | (C))

////////////////////////////////////////////////////////////////////////////////////////
// Macros to write commands/pixel colour data to an Raspberry Pi TFT
////////////////////////////////////////////////////////////////////////////////////////
#elif  defined (RPI_DISPLAY_TYPE)
  // Command is 16 bits
  #define CMD_BITS 16

  // ESP8266 low level SPI writes for 8, 16 and 32-bit values
  // to avoid the function call overhead
  #define TFT_WRITE_BITS(D, B) \
  SPI1U1 = ((B-1) << SPILMOSI); \
  SPI1W0 = D; \
  SPI1CMD |= SPIBUSY; \
  while(SPI1CMD & SPIBUSY) {}

  #define tft_Write_8(C)     TFT_WRITE_BITS((uint16_t)(C)<<8, CMD_BITS)

  #define tft_Write_16(C)    TFT_WRITE_BITS((C)>>8 | (C)<<8, 16)

  #define tft_Write_16S(C)   TFT_WRITE_BITS(C, 16)

  #define tft_Write_32(C)    TFT_WRITE_BITS(C, 32)

  #define tft_Write_32C(C,D) SPI1U1 = ((64-1) << SPILMOSI); \
                             SPI1W0 = ((C)<<24) | (C); \
                             SPI1W1 = ((D)<<24) | (D); \
                             SPI1CMD |= SPIBUSY; \
                             while(SPI1CMD & SPIBUSY) {;}

  #define tft_Write_32D(C) tft_Write_32C(C,C)

////////////////////////////////////////////////////////////////////////////////////////
// Macros for all other SPI displays
////////////////////////////////////////////////////////////////////////////////////////
#else
  // Command is 8 bits
  #define CMD_BITS 8

  #define tft_Write_8(C) \
  SPI1U1 = ((CMD_BITS-1) << SPILMOSI) | ((CMD_BITS-1) << SPILMISO); \
  SPI1W0 = (C)<<(CMD_BITS - 8); \
  SPI1CMD |= SPIBUSY; \
  while(SPI1CMD & SPIBUSY) {;}

  #define tft_Write_16(C) \
  SPI1U1 = (15 << SPILMOSI) | (15 << SPILMISO); \
  SPI1W0 = ((C)<<8 | (C)>>8); \
  SPI1CMD |= SPIBUSY; \
  while(SPI1CMD & SPIBUSY) {;}

  #define tft_Write_16N(C) \
  SPI1U1 = (15 << SPILMOSI) | (15 << SPILMISO); \
  SPI1W0 = ((C)<<8 | (C)>>8); \
  SPI1CMD |= SPIBUSY

  #define tft_Write_16S(C) \
  SPI1U1 = (15 << SPILMOSI) | (15 << SPILMISO); \
  SPI1W0 = C; \
  SPI1CMD |= SPIBUSY; \
  while(SPI1CMD & SPIBUSY) {;}

  #define tft_Write_32(C) \
  SPI1U1 = (31 << SPILMOSI) | (31 << SPILMISO); \
  SPI1W0 = C; \
  SPI1CMD |= SPIBUSY; \
  while(SPI1CMD & SPIBUSY) {;}

  #define tft_Write_32C(C,D) \
  SPI1U1 = (31 << SPILMOSI) | (31 << SPILMISO); \
  SPI1W0 = ((D)>>8 | (D)<<8)<<16 | ((C)>>8 | (C)<<8); \
  SPI1CMD |= SPIBUSY; \
  while(SPI1CMD & SPIBUSY) {;}

  #define tft_Write_32D(C) \
  SPI1U1 = (31 << SPILMOSI) | (31 << SPILMISO); \
  SPI1W0 = ((C)>>8 | (C)<<8)<<16 | ((C)>>8 | (C)<<8); \
  SPI1CMD |= SPIBUSY; \
  while(SPI1CMD & SPIBUSY) {;}

#endif

#ifndef tft_Write_16N
  #define tft_Write_16N tft_Write_16
#endif

////////////////////////////////////////////////////////////////////////////////////////
// Macros to read from display using SPI or software SPI
////////////////////////////////////////////////////////////////////////////////////////
#if defined (TFT_SDA_READ)
  // Use a bit banged function call for ESP8266 and bi-directional SDA pin
  #define TFT_eSPI_ENABLE_8_BIT_READ // Enable tft_Read_8(void);
  #define SCLK_L GPOC=sclkpinmask
  #define SCLK_H GPOS=sclkpinmask
#else
  // Use a SPI read transfer
  #define tft_Read_8() spi.transfer(0)
#endif

// Concatenate a byte sequence A,B,C,D to CDAB, P is a uint8_t pointer
#define DAT8TO32(P) ( (uint32_t)P[0]<<8 | P[1] | P[2]<<24 | P[3]<<16 )

#endif // Header end
