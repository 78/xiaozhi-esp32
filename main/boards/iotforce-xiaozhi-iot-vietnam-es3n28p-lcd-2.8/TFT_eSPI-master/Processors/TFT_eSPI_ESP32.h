        ////////////////////////////////////////////////////
        // TFT_eSPI driver functions for ESP32 processors //
        ////////////////////////////////////////////////////

#ifndef _TFT_eSPI_ESP32H_
#define _TFT_eSPI_ESP32H_

// Processor ID reported by getSetup()
#define PROCESSOR_ID 0x32

// Include processor specific header
#include "soc/spi_reg.h"
#include "driver/spi_master.h"
#include "hal/gpio_ll.h"

#if !defined(CONFIG_IDF_TARGET_ESP32C3) && !defined(CONFIG_IDF_TARGET_ESP32S2) && !defined(CONFIG_IDF_TARGET_ESP32)
  #define CONFIG_IDF_TARGET_ESP32
#endif

// Fix IDF problems with ESP32C3
#if CONFIG_IDF_TARGET_ESP32C3
  // Fix ESP32C3 IDF bug for missing definition
  #ifndef REG_SPI_BASE
    #define REG_SPI_BASE(i)     (DR_REG_SPI1_BASE + (((i)>1) ? (((i)* 0x1000) + 0x20000) : (((~(i)) & 1)* 0x1000 )))
  #endif

  // Fix ESP32C3 IDF bug for name change
  #ifndef SPI_MOSI_DLEN_REG
    #define SPI_MOSI_DLEN_REG(x) SPI_MS_DLEN_REG(x)
  #endif

  // Fix ESP32C3 specific register reference
  #define out_w1tc out_w1tc.val
  #define out_w1ts out_w1ts.val
#endif

// SUPPORT_TRANSACTIONS is mandatory for ESP32 so the hal mutex is toggled
#if !defined (SUPPORT_TRANSACTIONS)
  #define SUPPORT_TRANSACTIONS
#endif

/*
ESP32:
FSPI not defined
HSPI = 2, uses SPI2
VSPI = 3, uses SPI3

ESP32-S2:
FSPI = 1, uses SPI2
HSPI = 2, uses SPI3
VSPI not defined

ESP32 C3:
FSPI = 0, uses SPI2 ???? To be checked
HSPI = 1, uses SPI3 ???? To be checked
VSPI not defined

For ESP32/S2/C3:
SPI1_HOST = 0
SPI2_HOST = 1
SPI3_HOST = 2
*/

// ESP32 specific SPI port selection
#ifdef USE_HSPI_PORT
  #ifdef CONFIG_IDF_TARGET_ESP32
    #define SPI_PORT HSPI  //HSPI is port 2 on ESP32
  #else
    #define SPI_PORT 3     //HSPI is port 3 on ESP32 S2
  #endif
#elif defined(USE_FSPI_PORT)
    #define SPI_PORT 2 //FSPI(ESP32 S2)
#else
  #ifdef CONFIG_IDF_TARGET_ESP32
    #define SPI_PORT VSPI
  #else
    #define SPI_PORT 2 //FSPI(ESP32 S2)
  #endif
#endif

#ifdef RPI_DISPLAY_TYPE
  #define CMD_BITS (16-1)
#else
  #define CMD_BITS (8-1)
#endif

// Initialise processor specific SPI functions, used by init()
#define INIT_TFT_DATA_BUS // Not used

// Define a generic flag for 8-bit parallel
#if defined (ESP32_PARALLEL) // Specific to ESP32 for backwards compatibility
  #if !defined (TFT_PARALLEL_8_BIT)
    #define TFT_PARALLEL_8_BIT // Generic parallel flag
  #endif
#endif

// Ensure ESP32 specific flag is defined for 8-bit parallel
#if defined (TFT_PARALLEL_8_BIT)
  #if !defined (ESP32_PARALLEL)
    #define ESP32_PARALLEL
  #endif
#endif

// Processor specific code used by SPI bus transaction startWrite and endWrite functions
#if !defined (ESP32_PARALLEL)
  #if (TFT_SPI_MODE == SPI_MODE1) || (TFT_SPI_MODE == SPI_MODE2)
    #define SET_BUS_WRITE_MODE *_spi_user = SPI_USR_MOSI | SPI_CK_OUT_EDGE
    #define SET_BUS_READ_MODE  *_spi_user = SPI_USR_MOSI | SPI_USR_MISO | SPI_DOUTDIN | SPI_CK_OUT_EDGE
  #else
    #define SET_BUS_WRITE_MODE *_spi_user = SPI_USR_MOSI
    #define SET_BUS_READ_MODE  *_spi_user = SPI_USR_MOSI | SPI_USR_MISO | SPI_DOUTDIN
  #endif
#else
    // Not applicable to parallel bus
    #define SET_BUS_WRITE_MODE
    #define SET_BUS_READ_MODE
#endif

// Code to check if DMA is busy, used by SPI bus transaction transaction and endWrite functions
#if !defined(TFT_PARALLEL_8_BIT) && !defined(SPI_18BIT_DRIVER)
  #define ESP32_DMA
  // Code to check if DMA is busy, used by SPI DMA + transaction + endWrite functions
  #define DMA_BUSY_CHECK  dmaWait()
#else
  #define DMA_BUSY_CHECK
#endif

#if defined(TFT_PARALLEL_8_BIT)
  #define SPI_BUSY_CHECK
#else
  #define SPI_BUSY_CHECK while (*_spi_cmd&SPI_USR)
#endif

// If smooth font is used then it is likely SPIFFS will be needed
#ifdef SMOOTH_FONT
  // Call up the SPIFFS (SPI FLASH Filing System) for the anti-aliased fonts
  #define FS_NO_GLOBALS
  #include <FS.h>
  #include "SPIFFS.h" // ESP32 only
  #define FONT_FS_AVAILABLE
#endif


////////////////////////////////////////////////////////////////////////////////////////
// Define the DC (TFT Data/Command or Register Select (RS))pin drive code
////////////////////////////////////////////////////////////////////////////////////////
#ifndef TFT_DC
  #define DC_C // No macro allocated so it generates no code
  #define DC_D // No macro allocated so it generates no code
#else
  #if defined (TFT_PARALLEL_8_BIT)
    // TFT_DC, by design, must be in range 0-31 for single register parallel write
    #if (TFT_DC >= 0) &&  (TFT_DC < 32)
      #define DC_C GPIO.out_w1tc = (1 << TFT_DC)
      #define DC_D GPIO.out_w1ts = (1 << TFT_DC)
    #elif (TFT_DC >= 32)
      #define DC_C GPIO.out1_w1tc.val = (1 << (TFT_DC- 32))
      #define DC_D GPIO.out1_w1ts.val = (1 << (TFT_DC- 32))
    #else
      #define DC_C
      #define DC_D
    #endif
  #else
    #if (TFT_DC >= 32)
      #ifdef RPI_DISPLAY_TYPE  // RPi displays need a slower DC change
        #define DC_C GPIO.out1_w1ts.val = (1 << (TFT_DC - 32)); \
                     GPIO.out1_w1tc.val = (1 << (TFT_DC - 32))
        #define DC_D GPIO.out1_w1tc.val = (1 << (TFT_DC - 32)); \
                     GPIO.out1_w1ts.val = (1 << (TFT_DC - 32))
      #else
        #define DC_C GPIO.out1_w1tc.val = (1 << (TFT_DC - 32))//;GPIO.out1_w1tc.val = (1 << (TFT_DC - 32))
        #define DC_D GPIO.out1_w1ts.val = (1 << (TFT_DC - 32))//;GPIO.out1_w1ts.val = (1 << (TFT_DC - 32))
      #endif
    #elif (TFT_DC >= 0)
      #if defined (RPI_DISPLAY_TYPE)
        #if defined (ILI9486_DRIVER)
          // RPi ILI9486 display needs a slower DC change
          #define DC_C GPIO.out_w1tc = (1 << TFT_DC); \
                       GPIO.out_w1tc = (1 << TFT_DC)
          #define DC_D GPIO.out_w1tc = (1 << TFT_DC); \
                       GPIO.out_w1ts = (1 << TFT_DC)
        #else
          // Other RPi displays need a slower C->D change
          #define DC_C GPIO.out_w1tc = (1 << TFT_DC)
          #define DC_D GPIO.out_w1tc = (1 << TFT_DC); \
                       GPIO.out_w1ts = (1 << TFT_DC)
        #endif
      #else
        #define DC_C GPIO.out_w1tc = (1 << TFT_DC)//;GPIO.out_w1tc = (1 << TFT_DC)
        #define DC_D GPIO.out_w1ts = (1 << TFT_DC)//;GPIO.out_w1ts = (1 << TFT_DC)
      #endif
    #else
      #define DC_C
      #define DC_D
    #endif
  #endif
#endif

////////////////////////////////////////////////////////////////////////////////////////
// Define the CS (TFT chip select) pin drive code
////////////////////////////////////////////////////////////////////////////////////////
#ifndef TFT_CS
  #define TFT_CS -1  // Keep DMA code happy
  #define CS_L       // No macro allocated so it generates no code
  #define CS_H       // No macro allocated so it generates no code
#else
  #if defined (TFT_PARALLEL_8_BIT)
    #if TFT_CS >= 32
        #define CS_L GPIO.out1_w1tc.val = (1 << (TFT_CS - 32))
        #define CS_H GPIO.out1_w1ts.val = (1 << (TFT_CS - 32))
    #elif TFT_CS >= 0
        #define CS_L GPIO.out_w1tc = (1 << TFT_CS)
        #define CS_H GPIO.out_w1ts = (1 << TFT_CS)
    #else
      #define CS_L
      #define CS_H
    #endif
  #else
    #if (TFT_CS >= 32)
      #ifdef RPI_DISPLAY_TYPE  // RPi display needs a slower CS change
        #define CS_L GPIO.out1_w1ts.val = (1 << (TFT_CS - 32)); \
                     GPIO.out1_w1tc.val = (1 << (TFT_CS - 32))
        #define CS_H GPIO.out1_w1tc.val = (1 << (TFT_CS - 32)); \
                     GPIO.out1_w1ts.val = (1 << (TFT_CS - 32))
      #else
        #define CS_L GPIO.out1_w1tc.val = (1 << (TFT_CS - 32)); GPIO.out1_w1tc.val = (1 << (TFT_CS - 32))
        #define CS_H GPIO.out1_w1ts.val = (1 << (TFT_CS - 32))//;GPIO.out1_w1ts.val = (1 << (TFT_CS - 32))
      #endif
    #elif (TFT_CS >= 0)
      #ifdef RPI_DISPLAY_TYPE  // RPi display needs a slower CS change
        #define CS_L GPIO.out_w1ts = (1 << TFT_CS); GPIO.out_w1tc = (1 << TFT_CS)
        #define CS_H GPIO.out_w1tc = (1 << TFT_CS); GPIO.out_w1ts = (1 << TFT_CS)
      #else
        #define CS_L GPIO.out_w1tc = (1 << TFT_CS); GPIO.out_w1tc = (1 << TFT_CS)
        #define CS_H GPIO.out_w1ts = (1 << TFT_CS)//;GPIO.out_w1ts = (1 << TFT_CS)
      #endif
    #else
      #define CS_L
      #define CS_H
    #endif
  #endif
#endif

////////////////////////////////////////////////////////////////////////////////////////
// Define the WR (TFT Write) pin drive code
////////////////////////////////////////////////////////////////////////////////////////
#if defined (TFT_WR)
  #if (TFT_WR >= 32)
    // Note: it will be ~1.25x faster if the TFT_WR pin uses a GPIO pin lower than 32
    #define WR_L GPIO.out1_w1tc.val = (1 << (TFT_WR - 32))
    #define WR_H GPIO.out1_w1ts.val = (1 << (TFT_WR - 32))
  #elif (TFT_WR >= 0)
    // TFT_WR, for best performance, should be in range 0-31 for single register parallel write
    #define WR_L GPIO.out_w1tc = (1 << TFT_WR)
    #define WR_H GPIO.out_w1ts = (1 << TFT_WR)
  #else
    #define WR_L
    #define WR_H
  #endif
#else
  #define WR_L
  #define WR_H
#endif

////////////////////////////////////////////////////////////////////////////////////////
// Define the touch screen chip select pin drive code
////////////////////////////////////////////////////////////////////////////////////////
#ifndef TOUCH_CS
  #define T_CS_L // No macro allocated so it generates no code
  #define T_CS_H // No macro allocated so it generates no code
#else // XPT2046 is slow, so use slower digitalWrite here
  #define T_CS_L digitalWrite(TOUCH_CS, LOW)
  #define T_CS_H digitalWrite(TOUCH_CS, HIGH)
#endif

////////////////////////////////////////////////////////////////////////////////////////
// Make sure SPI default pins are assigned if not specified by user or set to -1
////////////////////////////////////////////////////////////////////////////////////////
#if !defined (TFT_PARALLEL_8_BIT)

  #ifdef USE_HSPI_PORT

    #ifndef TFT_MISO
      #define TFT_MISO -1
    #endif

    #ifndef TFT_MOSI
      #define TFT_MOSI 13
    #endif
    #if (TFT_MOSI == -1)
      #undef TFT_MOSI
      #define TFT_MOSI 13
    #endif

    #ifndef TFT_SCLK
      #define TFT_SCLK 14
    #endif
    #if (TFT_SCLK == -1)
      #undef TFT_SCLK
      #define TFT_SCLK 14
    #endif

  #else // VSPI port

    #ifndef TFT_MISO
      #define TFT_MISO -1
    #endif

    #ifndef TFT_MOSI
      #define TFT_MOSI 23
    #endif
    #if (TFT_MOSI == -1)
      #undef TFT_MOSI
      #define TFT_MOSI 23
    #endif

    #ifndef TFT_SCLK
      #define TFT_SCLK 18
    #endif
    #if (TFT_SCLK == -1)
      #undef TFT_SCLK
      #define TFT_SCLK 18
    #endif

    #if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S2)
      #if (TFT_MISO == -1)
        #undef TFT_MISO
        #define TFT_MISO TFT_MOSI
      #endif
    #endif

  #endif

#endif

////////////////////////////////////////////////////////////////////////////////////////
// Define the parallel bus interface chip pin drive code
////////////////////////////////////////////////////////////////////////////////////////
#if defined (TFT_PARALLEL_8_BIT)

  // Create a bit set lookup table for data bus - wastes 1kbyte of RAM but speeds things up dramatically
  // can then use e.g. GPIO.out_w1ts = set_mask(0xFF); to set data bus to 0xFF
  #define PARALLEL_INIT_TFT_DATA_BUS               \
  for (int32_t c = 0; c<256; c++)                  \
  {                                                \
    xset_mask[c] = 0;                              \
    if ( c & 0x01 ) xset_mask[c] |= (1 << TFT_D0); \
    if ( c & 0x02 ) xset_mask[c] |= (1 << TFT_D1); \
    if ( c & 0x04 ) xset_mask[c] |= (1 << TFT_D2); \
    if ( c & 0x08 ) xset_mask[c] |= (1 << TFT_D3); \
    if ( c & 0x10 ) xset_mask[c] |= (1 << TFT_D4); \
    if ( c & 0x20 ) xset_mask[c] |= (1 << TFT_D5); \
    if ( c & 0x40 ) xset_mask[c] |= (1 << TFT_D6); \
    if ( c & 0x80 ) xset_mask[c] |= (1 << TFT_D7); \
  }                                                \

  // Mask for the 8 data bits to set pin directions
  #define GPIO_DIR_MASK ((1 << TFT_D0) | (1 << TFT_D1) | (1 << TFT_D2) | (1 << TFT_D3) | (1 << TFT_D4) | (1 << TFT_D5) | (1 << TFT_D6) | (1 << TFT_D7))

  #if (TFT_WR >= 32)
    // Data bits and the write line are cleared sequentially
    #define GPIO_OUT_CLR_MASK (GPIO_DIR_MASK); WR_L
  #elif (TFT_WR >= 0)
    // Data bits and the write line are cleared to 0 in one step (1.25x faster)
    #define GPIO_OUT_CLR_MASK (GPIO_DIR_MASK | (1 << TFT_WR))
  #else
    #define GPIO_OUT_CLR_MASK
  #endif

  // A lookup table is used to set the different bit patterns, this uses 1kByte of RAM
  #define set_mask(C) xset_mask[C] // 63fps Sprite rendering test 33% faster, graphicstest only 1.8% faster than shifting in real time

  // Real-time shifting alternative to above to save 1KByte RAM, 47 fps Sprite rendering test
  /*#define set_mask(C) (((C)&0x80)>>7)<<TFT_D7 | (((C)&0x40)>>6)<<TFT_D6 | (((C)&0x20)>>5)<<TFT_D5 | (((C)&0x10)>>4)<<TFT_D4 | \
                        (((C)&0x08)>>3)<<TFT_D3 | (((C)&0x04)>>2)<<TFT_D2 | (((C)&0x02)>>1)<<TFT_D1 | (((C)&0x01)>>0)<<TFT_D0
  //*/

  // Write 8 bits to TFT
  #define tft_Write_8(C)  GPIO.out_w1tc = GPIO_OUT_CLR_MASK; GPIO.out_w1ts = set_mask((uint8_t)(C)); WR_H

  #if defined (SSD1963_DRIVER)

    // Write 18-bit color to TFT
    #define tft_Write_16(C) GPIO.out_w1tc = GPIO_OUT_CLR_MASK; GPIO.out_w1ts = set_mask((uint8_t) (((C) & 0xF800)>> 8)); WR_H; \
                            GPIO.out_w1tc = GPIO_OUT_CLR_MASK; GPIO.out_w1ts = set_mask((uint8_t) (((C) & 0x07E0)>> 3)); WR_H; \
                            GPIO.out_w1tc = GPIO_OUT_CLR_MASK; GPIO.out_w1ts = set_mask((uint8_t) (((C) & 0x001F)<< 3)); WR_H

    // 18-bit color write with swapped bytes
    #define tft_Write_16S(C) Cswap = ((C) >>8 | (C) << 8); tft_Write_16(Cswap)

  #else

    #ifdef PSEUDO_16_BIT
      // One write strobe for both bytes
      #define tft_Write_16(C)  GPIO.out_w1tc = GPIO_OUT_CLR_MASK; GPIO.out_w1ts = set_mask((uint8_t) ((C) >> 0)); WR_H
      #define tft_Write_16S(C) GPIO.out_w1tc = GPIO_OUT_CLR_MASK; GPIO.out_w1ts = set_mask((uint8_t) ((C) >> 8)); WR_H
    #else
      // Write 16 bits to TFT
      #define tft_Write_16(C) GPIO.out_w1tc = GPIO_OUT_CLR_MASK; GPIO.out_w1ts = set_mask((uint8_t) ((C) >> 8)); WR_H; \
                              GPIO.out_w1tc = GPIO_OUT_CLR_MASK; GPIO.out_w1ts = set_mask((uint8_t) ((C) >> 0)); WR_H

      // 16-bit write with swapped bytes
      #define tft_Write_16S(C) GPIO.out_w1tc = GPIO_OUT_CLR_MASK; GPIO.out_w1ts = set_mask((uint8_t) ((C) >> 0)); WR_H; \
                               GPIO.out_w1tc = GPIO_OUT_CLR_MASK; GPIO.out_w1ts = set_mask((uint8_t) ((C) >> 8)); WR_H
    #endif

  #endif

  // Write 32 bits to TFT
  #define tft_Write_32(C) GPIO.out_w1tc = GPIO_OUT_CLR_MASK; GPIO.out_w1ts = set_mask((uint8_t) ((C) >> 24)); WR_H; \
                          GPIO.out_w1tc = GPIO_OUT_CLR_MASK; GPIO.out_w1ts = set_mask((uint8_t) ((C) >> 16)); WR_H; \
                          GPIO.out_w1tc = GPIO_OUT_CLR_MASK; GPIO.out_w1ts = set_mask((uint8_t) ((C) >>  8)); WR_H; \
                          GPIO.out_w1tc = GPIO_OUT_CLR_MASK; GPIO.out_w1ts = set_mask((uint8_t) ((C) >>  0)); WR_H

  // Write two concatenated 16-bit values to TFT
  #define tft_Write_32C(C,D) GPIO.out_w1tc = GPIO_OUT_CLR_MASK; GPIO.out_w1ts = set_mask((uint8_t) ((C) >> 8)); WR_H; \
                             GPIO.out_w1tc = GPIO_OUT_CLR_MASK; GPIO.out_w1ts = set_mask((uint8_t) ((C) >> 0)); WR_H; \
                             GPIO.out_w1tc = GPIO_OUT_CLR_MASK; GPIO.out_w1ts = set_mask((uint8_t) ((D) >> 8)); WR_H; \
                             GPIO.out_w1tc = GPIO_OUT_CLR_MASK; GPIO.out_w1ts = set_mask((uint8_t) ((D) >> 0)); WR_H

  // Write 16-bit value twice to TFT - used by drawPixel()
  #define tft_Write_32D(C) GPIO.out_w1tc = GPIO_OUT_CLR_MASK; GPIO.out_w1ts = set_mask((uint8_t) ((C) >> 8)); WR_H; \
                           GPIO.out_w1tc = GPIO_OUT_CLR_MASK; GPIO.out_w1ts = set_mask((uint8_t) ((C) >> 0)); WR_H; \
                           GPIO.out_w1tc = GPIO_OUT_CLR_MASK; GPIO.out_w1ts = set_mask((uint8_t) ((C) >> 8)); WR_H; \
                           GPIO.out_w1tc = GPIO_OUT_CLR_MASK; GPIO.out_w1ts = set_mask((uint8_t) ((C) >> 0)); WR_H

   // Read pin
  #ifdef TFT_RD
    #if (TFT_RD >= 32)
      #define RD_L GPIO.out1_w1tc.val = (1 << (TFT_RD - 32))
      #define RD_H GPIO.out1_w1ts.val = (1 << (TFT_RD - 32))
    #elif (TFT_RD >= 0)
      #define RD_L GPIO.out_w1tc = (1 << TFT_RD)
      //#define RD_L digitalWrite(TFT_WR, LOW)
      #define RD_H GPIO.out_w1ts = (1 << TFT_RD)
      //#define RD_H digitalWrite(TFT_WR, HIGH)
    #else
      #define RD_L
      #define RD_H
    #endif
  #else
    #define TFT_RD -1
    #define RD_L
    #define RD_H
  #endif

////////////////////////////////////////////////////////////////////////////////////////
// Macros to write commands/pixel colour data to a SPI ILI948x TFT
////////////////////////////////////////////////////////////////////////////////////////
#elif  defined (SPI_18BIT_DRIVER) // SPI 18-bit colour

  // Write 8 bits to TFT
  #define tft_Write_8(C)   spi.transfer(C)

  // Convert 16-bit colour to 18-bit and write in 3 bytes
  #define tft_Write_16(C)  spi.transfer(((C) & 0xF800)>>8); \
                           spi.transfer(((C) & 0x07E0)>>3); \
                           spi.transfer(((C) & 0x001F)<<3)

  // Future option for transfer without wait
  #define tft_Write_16N(C) tft_Write_16(C)

  // Convert swapped byte 16-bit colour to 18-bit and write in 3 bytes
  #define tft_Write_16S(C) spi.transfer((C) & 0xF8); \
                           spi.transfer(((C) & 0xE000)>>11 | ((C) & 0x07)<<5); \
                           spi.transfer(((C) & 0x1F00)>>5)

  // Write 32 bits to TFT
  #define tft_Write_32(C)  spi.write32(C)

  // Write two concatenated 16-bit values to TFT
  #define tft_Write_32C(C,D) spi.write32((C)<<16 | (D))

  // Write 16-bit value twice to TFT
  #define tft_Write_32D(C)  spi.write32((C)<<16 | (C))

////////////////////////////////////////////////////////////////////////////////////////
// Macros to write commands/pixel colour data to an Raspberry Pi TFT
////////////////////////////////////////////////////////////////////////////////////////
#elif  defined (RPI_DISPLAY_TYPE)

  // ESP32 low level SPI writes for 8, 16 and 32-bit values
  // to avoid the function call overhead
  #define TFT_WRITE_BITS(D, B) \
  WRITE_PERI_REG(SPI_MOSI_DLEN_REG(SPI_PORT), B-1); \
  WRITE_PERI_REG(SPI_W0_REG(SPI_PORT), D); \
  SET_PERI_REG_MASK(SPI_CMD_REG(SPI_PORT), SPI_USR); \
  while (READ_PERI_REG(SPI_CMD_REG(SPI_PORT))&SPI_USR);

  // Write 8 bits
  #define tft_Write_8(C) TFT_WRITE_BITS((C)<<8, 16)

  // Write 16 bits with corrected endianness for 16-bit colours
  #define tft_Write_16(C) TFT_WRITE_BITS((C)<<8 | (C)>>8, 16)

  // Future option for transfer without wait
  #define tft_Write_16N(C) tft_Write_16(C)

  // Write 16 bits
  #define tft_Write_16S(C) TFT_WRITE_BITS(C, 16)

  // Write 32 bits
  #define tft_Write_32(C) TFT_WRITE_BITS(C, 32)

  // Write two address coordinates
  #define tft_Write_32C(C,D)  TFT_WRITE_BITS((C)<<24 | (C), 32); \
                              TFT_WRITE_BITS((D)<<24 | (D), 32)

  // Write same value twice
  #define tft_Write_32D(C) tft_Write_32C(C,C)

////////////////////////////////////////////////////////////////////////////////////////
// Macros for all other SPI displays
////////////////////////////////////////////////////////////////////////////////////////
#else
/* Old macros
  // ESP32 low level SPI writes for 8, 16 and 32-bit values
  // to avoid the function call overhead
  #define TFT_WRITE_BITS(D, B) \
  WRITE_PERI_REG(SPI_MOSI_DLEN_REG(SPI_PORT), B-1); \
  WRITE_PERI_REG(SPI_W0_REG(SPI_PORT), D); \
  SET_PERI_REG_MASK(SPI_CMD_REG(SPI_PORT), SPI_USR); \
  while (READ_PERI_REG(SPI_CMD_REG(SPI_PORT))&SPI_USR);

  // Write 8 bits
  #define tft_Write_8(C) TFT_WRITE_BITS(C, 8)

  // Write 16 bits with corrected endianness for 16-bit colours
  #define tft_Write_16(C) TFT_WRITE_BITS((C)<<8 | (C)>>8, 16)

  // Write 16 bits
  #define tft_Write_16S(C) TFT_WRITE_BITS(C, 16)

  // Write 32 bits
  #define tft_Write_32(C) TFT_WRITE_BITS(C, 32)

  // Write two address coordinates
  #define tft_Write_32C(C,D) TFT_WRITE_BITS((uint16_t)((D)<<8 | (D)>>8)<<16 | (uint16_t)((C)<<8 | (C)>>8), 32)

  // Write same value twice
  #define tft_Write_32D(C) TFT_WRITE_BITS((uint16_t)((C)<<8 | (C)>>8)<<16 | (uint16_t)((C)<<8 | (C)>>8), 32)
//*/
//* Replacement slimmer macros
  #define TFT_WRITE_BITS(D, B) *_spi_mosi_dlen = B-1;    \
                               *_spi_w = D;             \
                               *_spi_cmd = SPI_USR;      \
                        while (*_spi_cmd & SPI_USR);

  // Write 8 bits
  #define tft_Write_8(C) TFT_WRITE_BITS(C, 8)

  // Write 16 bits with corrected endianness for 16-bit colours
  #define tft_Write_16(C) TFT_WRITE_BITS((C)<<8 | (C)>>8, 16)

  // Future option for transfer without wait
  #define tft_Write_16N(C) *_spi_mosi_dlen = 16-1;       \
                           *_spi_w = ((C)<<8 | (C)>>8); \
                           *_spi_cmd = SPI_USR;

  // Write 16 bits
  #define tft_Write_16S(C) TFT_WRITE_BITS(C, 16)

  // Write 32 bits
  #define tft_Write_32(C) TFT_WRITE_BITS(C, 32)

  // Write two address coordinates
  #define tft_Write_32C(C,D)  TFT_WRITE_BITS((uint16_t)((D)<<8 | (D)>>8)<<16 | (uint16_t)((C)<<8 | (C)>>8), 32)

  // Write same value twice
  #define tft_Write_32D(C) TFT_WRITE_BITS((uint16_t)((C)<<8 | (C)>>8)<<16 | (uint16_t)((C)<<8 | (C)>>8), 32)

//*/
#endif

#ifndef tft_Write_16N
  #define tft_Write_16N tft_Write_16
#endif

////////////////////////////////////////////////////////////////////////////////////////
// Macros to read from display using SPI or software SPI
////////////////////////////////////////////////////////////////////////////////////////
#if !defined (TFT_PARALLEL_8_BIT)
  // Read from display using SPI or software SPI
  // Use a SPI read transfer
  #define tft_Read_8() spi.transfer(0)
#endif

// Concatenate a byte sequence A,B,C,D to CDAB, P is a uint8_t pointer
#define DAT8TO32(P) ( (uint32_t)P[0]<<8 | P[1] | P[2]<<24 | P[3]<<16 )

#endif // Header end
