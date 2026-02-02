        ////////////////////////////////////////////////////
        //       TFT_eSPI generic driver functions        //
        ////////////////////////////////////////////////////

// This is a generic driver for Arduino boards, it supports SPI interface displays
// 8-bit parallel interface to TFT is not supported for generic processors

#ifndef _TFT_eSPI_RP2040H_
#define _TFT_eSPI_RP2040H_

#ifndef ARDUINO_ARCH_MBED
  #include <LittleFS.h>
  #define FONT_FS_AVAILABLE
  #define SPIFFS LittleFS
#endif

// Required for both the official and community board packages
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"

// Processor ID reported by getSetup()
#define PROCESSOR_ID 0x2040

// Transactions always supported
#ifndef SUPPORT_TRANSACTIONS
  #define SUPPORT_TRANSACTIONS
#endif

// Include processor specific header
// None

#if defined (TFT_PARALLEL_8_BIT) || defined (TFT_PARALLEL_16_BIT) || defined (RP2040_PIO_SPI)
  #define RP2040_PIO_INTERFACE
  #define RP2040_PIO_PUSHBLOCK
#endif

#if !defined (RP2040_PIO_INTERFACE)// SPI
  // Use SPI0 as default if not defined
  #ifndef TFT_SPI_PORT
    #define TFT_SPI_PORT 0
  #endif

  #if (TFT_SPI_PORT == 0)
    #define SPI_X spi0
  #else
    #define SPI_X spi1
  #endif

  // Processor specific code used by SPI bus transaction begin/end_tft_write functions
  #define SET_BUS_WRITE_MODE spi_set_format(SPI_X,  8, (spi_cpol_t)(TFT_SPI_MODE >> 1), (spi_cpha_t)(TFT_SPI_MODE & 0x1), SPI_MSB_FIRST)
  #define SET_BUS_READ_MODE  spi_set_format(SPI_X,  8, (spi_cpol_t)0, (spi_cpha_t)0, SPI_MSB_FIRST)
#else
  // Processor specific code used by SPI bus transaction begin/end_tft_write functions
  #define SET_BUS_WRITE_MODE
  #define SET_BUS_READ_MODE
#endif

// Code to check if SPI or DMA is busy, used by SPI bus transaction startWrite and/or endWrite functions
#if !defined(SPI_18BIT_DRIVER)
  #define RP2040_DMA
  // Code to check if DMA is busy, used by SPI DMA + transaction + endWrite functions
  #define DMA_BUSY_CHECK dmaWait()
#else
  #define DMA_BUSY_CHECK
#endif

// Handle high performance MHS RPi display type
#if  defined (MHS_DISPLAY_TYPE)  && !defined (RPI_DISPLAY_TYPE)
  #define RPI_DISPLAY_TYPE
#endif

#if !defined (RP2040_PIO_INTERFACE) // SPI

  #if  defined (MHS_DISPLAY_TYPE) // High speed RPi TFT type always needs 16-bit transfers
    // This swaps to 16-bit mode, used for commands so wait avoids clash with DC timing
    #define INIT_TFT_DATA_BUS hw_write_masked(&spi_get_hw(SPI_X)->cr0, (16 - 1) << SPI_SSPCR0_DSS_LSB, SPI_SSPCR0_DSS_BITS)
  #else
    // Initialise processor specific SPI functions, used by init()
    #define INIT_TFT_DATA_BUS  // Not used
  #endif

  // Wait for tx to end, flush rx FIFO, clear rx overrun
  #define SPI_BUSY_CHECK while (spi_get_hw(SPI_X)->sr & SPI_SSPSR_BSY_BITS) {};     \
                         while (spi_is_readable(SPI_X)) (void)spi_get_hw(SPI_X)->dr; \
                         spi_get_hw(SPI_X)->icr = SPI_SSPICR_RORIC_BITS

  // To be safe, SUPPORT_TRANSACTIONS is assumed mandatory
  #if !defined (SUPPORT_TRANSACTIONS)
    #define SUPPORT_TRANSACTIONS
  #endif
#else

  // Different controllers have different minimum write cycle periods, so the PIO clock is changed accordingly
  // The PIO clock is a division of the CPU clock so scales when the processor is overclocked
  // PIO write frequency = (CPU clock/(4 * RP2040_PIO_CLK_DIV))
  // The write cycle periods below assume a 125MHz CPU clock speed
  #if defined (TFT_PARALLEL_8_BIT) || defined (TFT_PARALLEL_16_BIT)
    #if defined (RP2040_PIO_CLK_DIV)
      #if (RP2040_PIO_CLK_DIV > 0)
        #define DIV_UNITS RP2040_PIO_CLK_DIV
        #define DIV_FRACT 0
      #else
        #define DIV_UNITS 3
        #define DIV_FRACT 0
      #endif
    #elif defined (TFT_PARALLEL_16_BIT)
      // Different display drivers have different minimum write cycle times
      #if defined (HX8357C_DRIVER) || defined (SSD1963_DRIVER)
        #define DIV_UNITS 1 // 32ns write cycle time SSD1963, HX8357C (maybe HX8357D?)
      #elif defined (ILI9486_DRIVER) || defined (HX8357B_DRIVER) || defined (HX8357D_DRIVER)
        #define DIV_UNITS 2 // 64ns write cycle time ILI9486, HX8357D, HX8357B
      #else // ILI9481 needs a slower cycle time
        #define DIV_UNITS 3 // 96ns write cycle time
      #endif
      #define DIV_FRACT 0
    #else // 8-bit parallel mode default 64ns write cycle time
      #define DIV_UNITS 2
      #define DIV_FRACT 0 // Note: Fractional values done with clock period dithering
    #endif
  #endif

  // Initialise TFT data bus
  #if defined (TFT_PARALLEL_8_BIT) || defined (TFT_PARALLEL_16_BIT)
    #define INIT_TFT_DATA_BUS pioinit(DIV_UNITS, DIV_FRACT);
  #elif defined (RP2040_PIO_SPI)
    #define INIT_TFT_DATA_BUS pioinit(SPI_FREQUENCY);
  #endif

  #define SPI_BUSY_CHECK

  // Set the state machine clock divider (from integer and fractional parts - 16:8) 
  #define PARALLEL_INIT_TFT_DATA_BUS // Not used

#endif


// If smooth fonts are enabled the filing system may need to be loaded
#if defined (SMOOTH_FONT) && !defined (ARDUINO_ARCH_MBED)
  // Call up the filing system for the anti-aliased fonts
  //#define FS_NO_GLOBALS
  #include <FS.h>
#endif

////////////////////////////////////////////////////////////////////////////////////////
// Define the DC (TFT Data/Command or Register Select (RS))pin drive code
////////////////////////////////////////////////////////////////////////////////////////
#ifndef TFT_DC
  #define DC_C // No macro allocated so it generates no code
  #define DC_D // No macro allocated so it generates no code
#else
  #if !defined (RP2040_PIO_INTERFACE)// SPI
    //#define DC_C sio_hw->gpio_clr = (1ul << TFT_DC)
    //#define DC_D sio_hw->gpio_set = (1ul << TFT_DC)
    #if  defined (RPI_DISPLAY_TYPE) && !defined (MHS_DISPLAY_TYPE)
      #define DC_C digitalWrite(TFT_DC, LOW);
      #define DC_D digitalWrite(TFT_DC, HIGH);
    #else
      #define DC_C sio_hw->gpio_clr = (1ul << TFT_DC)
      #define DC_D sio_hw->gpio_set = (1ul << TFT_DC)
    #endif
  #else
    // PIO takes control of TFT_DC
    // Must wait for data to flush through before changing DC line
    #define DC_C  WAIT_FOR_STALL; \
                  tft_pio->sm[pio_sm].instr = pio_instr_clr_dc

    #ifndef RM68120_DRIVER
      // Flush has happened before this and mode changed back to 16-bit
      #define DC_D  tft_pio->sm[pio_sm].instr = pio_instr_set_dc
    #else
      // Need to wait for stall since RM68120 commands are 16-bit
      #define DC_D  WAIT_FOR_STALL; tft_pio->sm[pio_sm].instr = pio_instr_set_dc
    #endif
  #endif
#endif

////////////////////////////////////////////////////////////////////////////////////////
// Define the CS (TFT chip select) pin drive code
////////////////////////////////////////////////////////////////////////////////////////
#ifndef TFT_CS
  #define CS_L // No macro allocated so it generates no code
  #define CS_H // No macro allocated so it generates no code
#else
  #if !defined (RP2040_PIO_INTERFACE) // SPI
    #if  defined (RPI_DISPLAY_TYPE) && !defined (MHS_DISPLAY_TYPE)
      #define CS_L digitalWrite(TFT_CS, LOW);
      #define CS_H digitalWrite(TFT_CS, HIGH);
    #else
      #define CS_L sio_hw->gpio_clr = (1ul << TFT_CS)
      #define CS_H sio_hw->gpio_set = (1ul << TFT_CS)
    #endif
  #else // PIO interface display
    #define CS_L sio_hw->gpio_clr = (1ul << TFT_CS)
    #define CS_H WAIT_FOR_STALL; sio_hw->gpio_set = (1ul << TFT_CS)
  #endif
#endif

////////////////////////////////////////////////////////////////////////////////////////
// Make sure TFT_RD is defined if not used to avoid an error message
////////////////////////////////////////////////////////////////////////////////////////
// At the moment read is not supported for parallel mode, tie TFT signal high
#ifdef TFT_RD
  #if (TFT_RD >= 0)
    #define RD_L sio_hw->gpio_clr = (1ul << TFT_RD)
    //#define RD_L digitalWrite(TFT_WR, LOW)
    #define RD_H sio_hw->gpio_set = (1ul << TFT_RD)
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
// Define the WR (TFT Write) pin drive code
////////////////////////////////////////////////////////////////////////////////////////
#if !defined (TFT_PARALLEL_8_BIT) && !defined (TFT_PARALLEL_16_BIT) // SPI
  #ifdef TFT_WR
    #define WR_L digitalWrite(TFT_WR, LOW)
    #define WR_H digitalWrite(TFT_WR, HIGH)
  #endif
#else
  // The PIO manages the write line
#endif

////////////////////////////////////////////////////////////////////////////////////////
// Define the touch screen chip select pin drive code
////////////////////////////////////////////////////////////////////////////////////////
#if !defined (RP2040_PIO_INTERFACE)// SPI
  #if !defined TOUCH_CS || (TOUCH_CS < 0)
    #define T_CS_L // No macro allocated so it generates no code
    #define T_CS_H // No macro allocated so it generates no code
  #else
    #define T_CS_L digitalWrite(TOUCH_CS, LOW)
    #define T_CS_H digitalWrite(TOUCH_CS, HIGH)
  #endif
#else
  #ifdef TOUCH_CS
    #error Touch screen not supported in parallel or SPI PIO mode, use a separate library.
  #endif
#endif

////////////////////////////////////////////////////////////////////////////////////////
// Make sure TFT_MISO is defined if not used to avoid an error message
////////////////////////////////////////////////////////////////////////////////////////
#ifndef TFT_MISO
  #define TFT_MISO -1
#endif

////////////////////////////////////////////////////////////////////////////////////////
// Macros to write commands/pixel colour data to a SPI ILI948x TFT
////////////////////////////////////////////////////////////////////////////////////////
#if !defined (RP2040_PIO_INTERFACE) // SPI

  #if  defined (SPI_18BIT_DRIVER) // SPI 18-bit colour

    // Write 8 bits to TFT
    #define tft_Write_8(C)      spi_get_hw(SPI_X)->dr = (uint32_t)(C); \
                                while (spi_get_hw(SPI_X)->sr & SPI_SSPSR_BSY_BITS) {}; \

    //#define tft_Write_8(C)   spi.transfer(C);
    #define tft_Write_8N(B)   while (!spi_is_writable(SPI_X)){}; \
                             spi_get_hw(SPI_X)->dr = (uint8_t)(B)

    // Convert 16-bit colour to 18-bit and write in 3 bytes
    #define tft_Write_16(C)  tft_Write_8N(((C) & 0xF800)>>8); \
                             tft_Write_8N(((C) & 0x07E0)>>3); \
                             tft_Write_8N(((C) & 0x001F)<<3)

    // Convert 16-bit colour to 18-bit and write in 3 bytes
    #define tft_Write_16N(C)  tft_Write_8N(((C) & 0xF800)>>8); \
                              tft_Write_8N(((C) & 0x07E0)>>3); \
                              tft_Write_8N(((C) & 0x001F)<<3)

    // Convert swapped byte 16-bit colour to 18-bit and write in 3 bytes
    #define tft_Write_16S(C) tft_Write_8N((C) & 0xF8); \
                             tft_Write_8N(((C) & 0xE000)>>11 | ((C) & 0x07)<<5); \
                             tft_Write_8N(((C) & 0x1F00)>>5)
    // Write 32 bits to TFT
    #define tft_Write_32(C)  tft_Write_8N(C>>24); \
                             tft_Write_8N(C>>16); \
                             tft_Write_8N(C>>8);  \
                             tft_Write_8N(C)

    // Write two address coordinates
    #define tft_Write_32C(C,D) tft_Write_8N(C>>8); \
                               tft_Write_8N(C);    \
                               tft_Write_8N(D>>8); \
                               tft_Write_8N(D)

    // Write same value twice
    #define tft_Write_32D(C) tft_Write_8N(C>>8); \
                             tft_Write_8N(C);    \
                             tft_Write_8N(C>>8); \
                             tft_Write_8N(C)

  ////////////////////////////////////////////////////////////////////////////////////////
  // Macros to write commands/pixel colour data to other displays
  ////////////////////////////////////////////////////////////////////////////////////////
  #else
    #if  defined (MHS_DISPLAY_TYPE) // High speed RPi TFT type always needs 16-bit transfers
      // This swaps to 16-bit mode, used for commands so wait avoids clash with DC timing
      #define tft_Write_8(C)      while (spi_get_hw(SPI_X)->sr & SPI_SSPSR_BSY_BITS) {}; \
                                  hw_write_masked(&spi_get_hw(SPI_X)->cr0, (16 - 1) << SPI_SSPCR0_DSS_LSB, SPI_SSPCR0_DSS_BITS); \
                                  spi_get_hw(SPI_X)->dr = (uint32_t)((C) | ((C)<<8)); \
                                  while (spi_get_hw(SPI_X)->sr & SPI_SSPSR_BSY_BITS) {}; \

      // Note: the following macros do not wait for the end of transmission

      #define tft_Write_16(C)     while (!spi_is_writable(SPI_X)){}; spi_get_hw(SPI_X)->dr = (uint32_t)(C)

      #define tft_Write_16N(C)    while (!spi_is_writable(SPI_X)){}; spi_get_hw(SPI_X)->dr = (uint32_t)(C)

      #define tft_Write_16S(C)    while (!spi_is_writable(SPI_X)){}; spi_get_hw(SPI_X)->dr = (uint32_t)(C)<<8 | (C)>>8

      #define tft_Write_32(C)     spi_get_hw(SPI_X)->dr = (uint32_t)((C)>>16); spi_get_hw(SPI_X)->dr = (uint32_t)(C)

      #define tft_Write_32C(C,D)  spi_get_hw(SPI_X)->dr = (uint32_t)(C); spi_get_hw(SPI_X)->dr = (uint32_t)(D)

      #define tft_Write_32D(C)    spi_get_hw(SPI_X)->dr = (uint32_t)(C); spi_get_hw(SPI_X)->dr = (uint32_t)(C)

    #elif  defined (RPI_DISPLAY_TYPE) // RPi TFT type always needs 16-bit transfers
      #define tft_Write_8(C)   spi.transfer(C); spi.transfer(C)
      #define tft_Write_16(C)  spi.transfer((uint8_t)((C)>>8));spi.transfer((uint8_t)((C)>>0))
      #define tft_Write_16N(C) spi.transfer((uint8_t)((C)>>8));spi.transfer((uint8_t)((C)>>0))
      #define tft_Write_16S(C) spi.transfer((uint8_t)((C)>>0));spi.transfer((uint8_t)((C)>>8))

      #define tft_Write_32(C) \
        tft_Write_16((uint16_t) ((C)>>16)); \
        tft_Write_16((uint16_t) ((C)>>0))

      #define tft_Write_32C(C,D) \
        spi.transfer(0); spi.transfer((C)>>8); \
        spi.transfer(0); spi.transfer((C)>>0); \
        spi.transfer(0); spi.transfer((D)>>8); \
        spi.transfer(0); spi.transfer((D)>>0)

      #define tft_Write_32D(C) \
        spi.transfer(0); spi.transfer((C)>>8); \
        spi.transfer(0); spi.transfer((C)>>0); \
        spi.transfer(0); spi.transfer((C)>>8); \
        spi.transfer(0); spi.transfer((C)>>0)

    #elif  defined (ILI9225_DRIVER) // Needs gaps between commands + data bytes, so use slower transfer functions

      // Warning: these all end in 8-bit SPI mode!
      #define tft_Write_8(C)      spi.transfer(C);

      #define tft_Write_16(C)     spi.transfer16(C)

      #define tft_Write_16N(C)    spi.transfer16(C)

      #define tft_Write_16S(C)    spi.transfer16((C)<<8 | (C)>>8)

      #define tft_Write_32(C)     spi.transfer16((C)>>16); spi.transfer16(C)

      #define tft_Write_32C(C,D)  spi.transfer16(C); spi.transfer16(D)

      #define tft_Write_32D(C)    spi.transfer16(C); spi.transfer16(C)

    #else

      // This swaps to 8-bit mode, then back to 16-bit mode
      #define tft_Write_8(C)      while (spi_get_hw(SPI_X)->sr & SPI_SSPSR_BSY_BITS) {}; \
                                  hw_write_masked(&spi_get_hw(SPI_X)->cr0, (8 - 1) << SPI_SSPCR0_DSS_LSB, SPI_SSPCR0_DSS_BITS); \
                                  spi_get_hw(SPI_X)->dr = (uint32_t)(C); \
                                  while (spi_get_hw(SPI_X)->sr & SPI_SSPSR_BSY_BITS) {}; \
                                  hw_write_masked(&spi_get_hw(SPI_X)->cr0, (16 - 1) << SPI_SSPCR0_DSS_LSB, SPI_SSPCR0_DSS_BITS)

      // Note: the following macros do not wait for the end of transmission

      #define tft_Write_16(C)     while (!spi_is_writable(SPI_X)){}; spi_get_hw(SPI_X)->dr = (uint32_t)(C)

      #define tft_Write_16N(C)    while (!spi_is_writable(SPI_X)){}; spi_get_hw(SPI_X)->dr = (uint32_t)(C)

      #define tft_Write_16S(C)    while (!spi_is_writable(SPI_X)){}; spi_get_hw(SPI_X)->dr = (uint32_t)(C)<<8 | (C)>>8

      #define tft_Write_32(C)     spi_get_hw(SPI_X)->dr = (uint32_t)((C)>>16); spi_get_hw(SPI_X)->dr = (uint32_t)(C)

      #define tft_Write_32C(C,D)  spi_get_hw(SPI_X)->dr = (uint32_t)(C); spi_get_hw(SPI_X)->dr = (uint32_t)(D)

      #define tft_Write_32D(C)    spi_get_hw(SPI_X)->dr = (uint32_t)(C); spi_get_hw(SPI_X)->dr = (uint32_t)(C)

    #endif // RPI_DISPLAY_TYPE
  #endif

#else // Parallel 8-bit or PIO SPI

  // Wait for the PIO to stall (SM pull request finds no data in TX FIFO)
  // This is used to detect when the SM is idle and hence ready for a jump instruction
  #define WAIT_FOR_STALL  tft_pio->fdebug = pull_stall_mask; while (!(tft_pio->fdebug & pull_stall_mask))

  // Wait until at least "S" locations free
  #define WAIT_FOR_FIFO_FREE(S) while (((tft_pio->flevel >> (pio_sm * 8)) & 0x000F) > (8-S)){}

  // Wait until at least 5 locations free
  #define WAIT_FOR_FIFO_5_FREE while ((tft_pio->flevel) & (0x000c << (pio_sm * 8))){}

  // Wait until at least 1 location free
  #define WAIT_FOR_FIFO_1_FREE while ((tft_pio->flevel) & (0x0008 << (pio_sm * 8))){}

  // Wait for FIFO to empty (use before swapping to 8 bits)
  #define WAIT_FOR_FIFO_EMPTY  while(!(tft_pio->fstat & (1u << (PIO_FSTAT_TXEMPTY_LSB + pio_sm))))

  // The write register of the TX FIFO.
  #define TX_FIFO  tft_pio->txf[pio_sm]

  // Temporary - to be deleted
  #define GPIO_DIR_MASK 0

  #if  defined (SPI_18BIT_DRIVER)  || defined (SSD1963_DRIVER) // 18-bit colour (3 bytes)
      // This writes 8 bits, then switches back to 16-bit mode automatically
      // Have already waited for pio stalled (last data write complete) when DC switched to command mode
      // The wait for stall allows DC to be changed immediately afterwards
      #define tft_Write_8(C)      tft_pio->sm[pio_sm].instr = pio_instr_jmp8; \
                                  TX_FIFO = (C); \
                                  WAIT_FOR_STALL

      // Used to send last byte for 32-bit macros below since PIO sends 24 bits
      #define tft_Write_8L(C)     WAIT_FOR_STALL; \
                                  tft_pio->sm[pio_sm].instr = pio_instr_jmp8; \
                                  TX_FIFO = (C)

      // Note: the following macros do not wait for the end of transmission

      #define tft_Write_16(C)     WAIT_FOR_FIFO_FREE(1); TX_FIFO = ((((uint32_t)(C) & 0xF800)<<8) | (((C) & 0x07E0)<<5) | (((C) & 0x001F)<<3))

      #define tft_Write_16N(C)    WAIT_FOR_FIFO_FREE(1); TX_FIFO = ((((uint32_t)(C) & 0xF800)<<8) | (((C) & 0x07E0)<<5) | (((C) & 0x001F)<<3))

      #define tft_Write_16S(C)    WAIT_FOR_FIFO_FREE(1); TX_FIFO = ((((uint32_t)(C) & 0xF8) << 16) | (((C) & 0xE000)>>3) | (((C) & 0x07)<<13) | (((C) & 0x1F00)>>5))

      #define tft_Write_32(C)     WAIT_FOR_FIFO_FREE(2); TX_FIFO = ((C)>>8); WAIT_FOR_STALL; tft_Write_8(C)

      #define tft_Write_32C(C,D)  WAIT_FOR_FIFO_FREE(2); TX_FIFO = (((C)<<8) | ((D)>>8)); tft_Write_8L(D)

      #define tft_Write_32D(C)    WAIT_FOR_FIFO_FREE(2); TX_FIFO = (((C)<<8) | ((C)>>8)); tft_Write_8L(C)

  #else // PIO interface, SPI or parallel
    // This writes 8 bits, then switches back to 16-bit mode automatically
    // Have already waited for pio stalled (last data write complete) when DC switched to command mode
    // The wait for stall allows DC to be changed immediately afterwards
    #if defined (TFT_PARALLEL_8_BIT) || defined (RP2040_PIO_SPI)
      #define tft_Write_8(C)      tft_pio->sm[pio_sm].instr = pio_instr_jmp8; \
                                  TX_FIFO = (C); \
                                  WAIT_FOR_STALL
    #else // For 16-bit parallel 16 bits are always sent
      #define tft_Write_8(C)      TX_FIFO = (C); \
                                  WAIT_FOR_STALL
    #endif

      // Note: the following macros do not wait for the end of transmission

      #define tft_Write_16(C)     WAIT_FOR_FIFO_FREE(1); TX_FIFO = (C)

      #define tft_Write_16N(C)    WAIT_FOR_FIFO_FREE(1); TX_FIFO = (C)

      #define tft_Write_16S(C)    WAIT_FOR_FIFO_FREE(1); TX_FIFO = ((C)<<8) | ((C)>>8)

      #define tft_Write_32(C)     WAIT_FOR_FIFO_FREE(2); TX_FIFO = ((C)>>16); TX_FIFO = (C)

      #define tft_Write_32C(C,D)  WAIT_FOR_FIFO_FREE(2); TX_FIFO = (C); TX_FIFO = (D)

      #define tft_Write_32D(C)    WAIT_FOR_FIFO_FREE(2); TX_FIFO = (C); TX_FIFO = (C)
  #endif
#endif

#ifndef tft_Write_16N
  #define tft_Write_16N tft_Write_16
#endif

////////////////////////////////////////////////////////////////////////////////////////
// Macros to read from display using SPI or software SPI
////////////////////////////////////////////////////////////////////////////////////////
#if !defined (RP2040_PIO_INTERFACE)// SPI
  #if defined (TFT_SDA_READ)
    // Use a bit banged function call for STM32 and bi-directional SDA pin
    #define TFT_eSPI_ENABLE_8_BIT_READ // Enable tft_Read_8(void);
    #define SCLK_L digitalWrite(TFT_SCLK, LOW)
    #define SCLK_H digitalWrite(TFT_SCLK, LOW)
  #else
    // Use a SPI read transfer
    #define tft_Read_8() spi.transfer(0)
  #endif
#endif
////////////////////////////////////////////////////////////////////////////////////////
// Temporary to keep the "Arduino Mbed OS RP2040 Boards" support package happy
////////////////////////////////////////////////////////////////////////////////////////
#if defined(ARDUINO_ARCH_RP2040)

  #define ltoa itoa

#endif

#endif // Header end
