        ////////////////////////////////////////////////////
        // TFT_eSPI driver functions for STM32 processors //
        ////////////////////////////////////////////////////

#ifndef _TFT_eSPI_STM32H_
#define _TFT_eSPI_STM32H_

// Processor ID reported by getSetup()
#define PROCESSOR_ID 0x32F

// Include processor specific header
// None

// RPi support not tested - Fast RPi not supported

// Processor specific code used by SPI bus transaction startWrite and endWrite functions
#define SET_BUS_WRITE_MODE // Not used
#define SET_BUS_READ_MODE  // Not used

// SUPPORT_TRANSACTIONS is mandatory for STM32
#if !defined (SUPPORT_TRANSACTIONS)
  #define SUPPORT_TRANSACTIONS
#endif

////////////////////////////////////////////////////////////////////////////////////////
// Define the parallel bus interface chip pin drive code
////////////////////////////////////////////////////////////////////////////////////////
#if defined(TFT_PARALLEL_8_BIT)

  // The STM32 processors can toggle pins fast, TFTs need setup and hold times
  // for writes so here twc can be extended  with delays:
  //
  //                       0 1 2 3 4 5     Extra high periods
  // TFT_WR ¯|_ _ _ _ _ _ |¯ ¯ ¯ ¯ ¯ ¯ ¯|
  //            5 4 3 2 1 0                Extra low periods
  //        xxxx=======================xxxx
  //         |<---------- twc --------->|
  //           |<- tdst ->|<-- tdht -->|
  //
  // Data is placed bit by bit on bus during period xxxx and TFT_WR driven low
  // Period xxxx depends on D0-D7 pin allocations and bit manipulation needed
  // Data stable during period ===
  // Most TFTs can be "overclocked" and run >2x faster than data sheet figures

////////////////////////////////////////////////////////////////////////////////////////
// Write strobe timing setup
////////////////////////////////////////////////////////////////////////////////////////
  #if defined (ILI9341_DRIVER) || defined(ILI9341_2_DRIVER) || defined (ST7796_DRIVER)   || defined (ILI9486_DRIVER) // WRX twc spec is <=66ns = 15.15MHz

    // Extra write pulse low time (delay for data setup)
    #if defined (STM32F1xx)
      #define WR_TWRL_0       // Change to WR_TWRL_1 if overclocking processor
    #elif defined (STM32F2xx) || defined (STM32F4xx)
      #define WR_TWRL_0       // Tested with STM32F446 - 27.6MHz when WR_TWRH_1 defined
      //#define WR_TWRL_3     // STM32F446 - 15.6MHz when WR_TWRH_3 defined
    #elif defined (STM32F7xx)
      #define WR_TWRL_1       //Tested with STM32F767
    #else
      #define WR_TWRL_5
    #endif

    // Extra write pulse high time (data hold time, delays next write cycle start)
    #if defined (STM32F1xx)
      #define WR_TWRH_0
    #elif defined (STM32F2xx) || defined (STM32F4xx)
      #define WR_TWRH_0       // Tested with STM32F446
      //#define WR_TWRL_3
    #elif defined (STM32F7xx)
      #define WR_TWRH_1       //Tested with STM32F767
    #else
      #define WR_TWRH_5
    #endif

  #elif defined (ILI9481_DRIVER) // WRX twc spec is 100ns = 10MHz

    // Extra write pulse low time (delay for data setup)
    #if defined (STM32F1xx)
      #define WR_TWRL_0
    #elif defined (STM32F2xx) || defined (STM32F4xx)
      //#define WR_TWRL_0    // STM32F446 - ~30MHz when WR_TWRH_0 defined
      //#define WR_TWRL_1    // STM32F446 - ~25MHz when WR_TWRH_0 defined
      #define WR_TWRL_2      // STM32F446 - ~20MHz when WR_TWRH_2 defined
      //#define WR_TWRL_3    // STM32F446 - ~16MHz when WR_TWRH_3 defined
      //#define WR_TWRL_4
      //#define WR_TWRL_5    // STM32F446 - ~12MHz when WR_TWRH_5 defined
    #elif defined (STM32F7xx)
      //#define WR_TWRL_0
      //#define WR_TWRL_1
      //#define WR_TWRL_2
      #define WR_TWRL_3
    #else
      //#define WR_TWRH_0    // Fastest
      //#define WR_TWRH_1
      //#define WR_TWRH_2
      #define WR_TWRH_3      // Slowest
    #endif

    // Extra write pulse high time (data hold time, delays next write cycle start)
    #if defined (STM32F1xx)
      #define WR_TWRH_0
    #elif defined (STM32F2xx) || defined (STM32F4xx)
      //#define WR_TWRH_0
      //#define WR_TWRH_1
      #define WR_TWRH_2
      //#define WR_TWRH_3
    #elif defined (STM32F7xx)
      //#define WR_TWRH_0
      //#define WR_TWRH_1
      //#define WR_TWRH_2
      #define WR_TWRH_3
      //#define WR_TWRH_4
      //#define WR_TWRH_5
    #else
      //#define WR_TWRH_0     // Fastest
      //#define WR_TWRH_1
      //#define WR_TWRH_2
      #define WR_TWRH_3       // Slowest
    #endif

  #else // Default display slow settings
    #if defined (STM32F1xx)
      // STM32F1xx series can run at full speed (unless overclocked)
      #define WR_TWRL_0
      #define WR_TWRH_0
    #else
      // Extra write pulse low time (delay for data setup)
      //#define WR_TWRL_0
      //#define WR_TWRL_1
      //#define WR_TWRL_2
      #define WR_TWRL_3
      //#define WR_TWRL_4
      //#define WR_TWRL_5

      // Extra write pulse high time (data hold time, delays next write cycle start)
      //#define WR_TWRH_0
      //#define WR_TWRH_1
      //#define WR_TWRH_2
      //#define WR_TWRH_3
      //#define WR_TWRH_4
      #define WR_TWRH_5
    #endif
  #endif

////////////////////////////////////////////////////////////////////////////////////////
// Macros for all other SPI displays
////////////////////////////////////////////////////////////////////////////////////////
#else

  // Use SPI1 as default if not defined
  #ifndef TFT_SPI_PORT
    #define TFT_SPI_PORT 1
  #endif

  #if (TFT_SPI_PORT == 1)
    #define SPIX SPI1
  #elif (TFT_SPI_PORT == 2)
    #define SPIX SPI2
  #elif (TFT_SPI_PORT == 3)
    #define SPIX SPI3
  #elif (TFT_SPI_PORT == 4)
    #define SPIX SPI4
  #endif
  // Global define is _VARIANT_ARDUINO_STM32_, see board package stm32_def.h for specific variants
  #if defined (STM32F2xx) || defined (STM32F4xx) || defined (STM32F7xx)

    #define STM32_DMA // DMA is available with these processors

    #if (TFT_SPI_PORT == 1)
      // Initialise processor specific SPI and DMA instances - used by init()
      #define INIT_TFT_DATA_BUS spiHal.Instance = SPI1; \
                                dmaHal.Instance = DMA2_Stream3
      // The DMA hard-coding for SPI1 is in TFT_eSPI_STM32.c as follows:
      //     DMA_CHANNEL_3
      //     DMA2_Stream3_IRQn and DMA2_Stream3_IRQHandler()
    #elif (TFT_SPI_PORT == 2)
      // Initialise processor specific SPI and DMA instances - used by init()
      #define INIT_TFT_DATA_BUS spiHal.Instance = SPI2; \
                                dmaHal.Instance = DMA1_Stream4
      // The DMA hard-coding for SPI2 is in TFT_eSPI_STM32.c as follows:
      //     DMA_CHANNEL_4
      //     DMA1_Stream4_IRQn and DMA1_Stream4_IRQHandler()
    #elif (TFT_SPI_PORT == 3)
      // Initialise processor specific SPI and DMA instances - used by init()
      #define INIT_TFT_DATA_BUS spiHal.Instance = SPI3; \
                                dmaHal.Instance = DMA1_Stream5
      // The DMA hard-coding for SPI3 is in TFT_eSPI_STM32.c as follows:
      //     DMA_CHANNEL_4
      //     DMA1_Stream5_IRQn and DMA1_Stream5_IRQHandler()
    #endif

  #elif defined (STM32F1xx)
    // For Blue Pill and STM32F1xx processors with DMA support
    #define STM32_DMA // DMA is available with these processors
    #if (TFT_SPI_PORT == 1)
      #define INIT_TFT_DATA_BUS spiHal.Instance = SPI1; \
                                dmaHal.Instance = DMA1_Channel3
    #elif (TFT_SPI_PORT == 2)
      #define INIT_TFT_DATA_BUS spiHal.Instance = SPI2; \
                                dmaHal.Instance = DMA1_Channel5
    #endif
  #else
    // For STM32 processor with no implemented DMA support (yet)
    #if (TFT_SPI_PORT == 1)
      #define INIT_TFT_DATA_BUS spiHal.Instance = SPI1
    #elif (TFT_SPI_PORT == 2)
      #define INIT_TFT_DATA_BUS spiHal.Instance = SPI2
    #endif
  #endif

#endif

#ifdef STM32_DMA
  // Code to check if DMA is busy, used by SPI DMA + transaction + endWrite functions
  #define DMA_BUSY_CHECK { if (DMA_Enabled) while(dmaBusy()); }
#else
  #define DMA_BUSY_CHECK
#endif

// If smooth fonts are enabled the filing system may need to be loaded
#ifdef SMOOTH_FONT
  // Call up the filing system for the anti-aliased fonts                                    <<<==== TODO
  //#define FS_NO_GLOBALS
  //#include <FS.h>
#endif // end of parallel/SPI selection

////////////////////////////////////////////////////////////////////////////////////////
// Define the DC (TFT Data/Command or Register Select (RS))pin drive code
////////////////////////////////////////////////////////////////////////////////////////
#if !defined (TFT_DC) || (TFT_DC < 0)
  #define DC_C // No macro allocated so it generates no code
  #define DC_D // No macro allocated so it generates no code
  #undef  TFT_DC
#else
  // Convert Arduino pin reference Dn or STM pin reference PXn to port and mask
  #define DC_PORT     digitalPinToPort(TFT_DC)
  #define DC_PIN_MASK digitalPinToBitMask(TFT_DC)
  // Use bit set reset register
  #define DC_C DC_DELAY; DC_PORT->BSRR = DC_PIN_MASK<<16
  #define DC_D DC_DELAY; DC_PORT->BSRR = DC_PIN_MASK
#endif

////////////////////////////////////////////////////////////////////////////////////////
// Define the CS (TFT chip select) pin drive code
////////////////////////////////////////////////////////////////////////////////////////
#if !defined (TFT_CS) || (TFT_CS < 0)
  #define CS_L // No macro allocated so it generates no code
  #define CS_H // No macro allocated so it generates no code
  #undef  TFT_CS
#else
  // Convert Arduino pin reference Dx or STM pin reference PXn to port and mask
  #define CS_PORT      digitalPinToPort(TFT_CS)
  #define CS_PIN_MASK  digitalPinToBitMask(TFT_CS)
  // Use bit set reset register
  #define CS_L CS_PORT->BSRR = CS_PIN_MASK<<16
  #define CS_H CS_PORT->BSRR = CS_PIN_MASK
#endif

////////////////////////////////////////////////////////////////////////////////////////
// Define the RD (TFT Read) pin drive code
////////////////////////////////////////////////////////////////////////////////////////
#ifdef TFT_RD
  #if (TFT_RD >= 0)
    // Convert Arduino pin reference Dx or STM pin reference PXn to port and mask
    #define RD_PORT      digitalPinToPort(TFT_RD)
    #define RD_PIN_MASK  digitalPinToBitMask(TFT_RD)
    // Use bit set reset register
    #define RD_L RD_PORT->BSRR = RD_PIN_MASK<<16
    #define RD_H RD_PORT->BSRR = RD_PIN_MASK
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
#ifdef TFT_WR
  // Convert Arduino pin reference Dx or STM pin reference PXn to port and mask
  #define WR_PORT      digitalPinToPort(TFT_WR)
  #define WR_PIN_MASK  digitalPinToBitMask(TFT_WR)
  // Use bit set reset register
  #define WR_L WR_PORT->BSRR = WR_PIN_MASK<<16
  #define WR_H WR_PORT->BSRR = WR_PIN_MASK
#endif

////////////////////////////////////////////////////////////////////////////////////////
// Define the touch screen chip select pin drive code
////////////////////////////////////////////////////////////////////////////////////////
#if !defined (TOUCH_CS) || (TOUCH_CS < 0)
  #define T_CS_L // No macro allocated so it generates no code
  #define T_CS_H // No macro allocated so it generates no code
#else
  // Speed is not important for this signal
  #define T_CS_L digitalWrite(TOUCH_CS, LOW)
  #define T_CS_H digitalWrite(TOUCH_CS, HIGH)
#endif

////////////////////////////////////////////////////////////////////////////////////////
// Make sure TFT_MISO is defined if not used to avoid an error message
////////////////////////////////////////////////////////////////////////////////////////
#if !defined (TFT_PARALLEL_8_BIT)
  #ifndef TFT_MISO
    #define TFT_MISO -1
  #endif
#endif

////////////////////////////////////////////////////////////////////////////////////////
// Define the parallel bus interface chip pin drive code
////////////////////////////////////////////////////////////////////////////////////////
#if defined (TFT_PARALLEL_8_BIT)

  // Mask for the 8 data bits to set pin directions (not used)
  #define GPIO_DIR_MASK 0

  #define PARALLEL_INIT_TFT_DATA_BUS // None

  #define INIT_TFT_DATA_BUS // Setup built into TFT_eSPI.cpp

////////////////////////////////////////////////////////////////////////////////////////
// Define the TFT_WR drive cycle timing
////////////////////////////////////////////////////////////////////////////////////////
  // Write low extra setup time
  #if   defined WR_TWRL_0
    #define     WR_TWRL
  #elif defined WR_TWRL_1 // 1 extra low period
    #define     WR_TWRL WR_L
  #elif defined WR_TWRL_2 // 2 extra low periods
    #define     WR_TWRL WR_L; WR_L
  #elif defined WR_TWRL_3 // 3 extra low periods
    #define     WR_TWRL WR_L; WR_L; WR_L
  #elif defined WR_TWRL_4 // 4 extra low periods
    #define     WR_TWRL WR_L; WR_L; WR_L; WR_L
  #elif defined WR_TWRL_5 // 5 extra low periods
    #define     WR_TWRL WR_L; WR_L; WR_L; WR_L; WR_L
  #endif

  // Write high extra hold time
  #if   defined WR_TWRH_0
    #define     WR_TWRH WR_H
  #elif defined WR_TWRH_1 // 1 extra high period
    #define     WR_TWRH  WR_H; WR_H
  #elif defined WR_TWRH_2 // 2 extra high periods
    #define     WR_TWRH  WR_H; WR_H; WR_H
  #elif defined WR_TWRH_3 // 3 extra high periods
    #define     WR_TWRH  WR_H; WR_H; WR_H; WR_H
  #elif defined WR_TWRH_4 // 4 extra high periods
    #define     WR_TWRH  WR_H; WR_H; WR_H; WR_H; WR_H
  #elif defined WR_TWRH_5 // 5 extra high periods
    #define     WR_TWRH  WR_H; WR_H; WR_H; WR_H; WR_H; WR_H
  #endif

  #define WR_STB WR_TWRL; WR_TWRH // Rising edge write strobe

////////////////////////////////////////////////////////////////////////////////////////
// Nucleo 64: hard-coded pins
////////////////////////////////////////////////////////////////////////////////////////
  #ifdef NUCLEO_64_TFT

    // Convert Arduino pin reference Dx or STM pin reference PXn to port and mask
    #define D0_PIN_NAME  digitalPinToPinName(TFT_D0)
    #define D1_PIN_NAME  digitalPinToPinName(TFT_D1)
    #define D2_PIN_NAME  digitalPinToPinName(TFT_D2)
    #define D3_PIN_NAME  digitalPinToPinName(TFT_D3)
    #define D4_PIN_NAME  digitalPinToPinName(TFT_D4)
    #define D5_PIN_NAME  digitalPinToPinName(TFT_D5)
    #define D6_PIN_NAME  digitalPinToPinName(TFT_D6)
    #define D7_PIN_NAME  digitalPinToPinName(TFT_D7)

    // Pin port bit number 0-15 (not used for Nucleo)
    #define D0_PIN_BIT  (D0_PIN_NAME & 0xF)
    #define D1_PIN_BIT  (D1_PIN_NAME & 0xF)
    #define D2_PIN_BIT  (D2_PIN_NAME & 0xF)
    #define D3_PIN_BIT  (D3_PIN_NAME & 0xF)
    #define D4_PIN_BIT  (D4_PIN_NAME & 0xF)
    #define D5_PIN_BIT  (D5_PIN_NAME & 0xF)
    #define D6_PIN_BIT  (D6_PIN_NAME & 0xF)
    #define D7_PIN_BIT  (D7_PIN_NAME & 0xF)

    // Pin port - better than get_GPIO_Port() which seems to be slow...
    #define D0_PIN_PORT GPIOA
    #define D1_PIN_PORT GPIOC
    #define D2_PIN_PORT GPIOA
    #define D3_PIN_PORT GPIOB
    #define D4_PIN_PORT GPIOB
    #define D5_PIN_PORT GPIOB
    #define D6_PIN_PORT GPIOB
    #define D7_PIN_PORT GPIOA

    // Pin masks for set/clear
    #define D0_PIN_MASK (1UL<< 9) // Set/clear mask for PA9
    #define D1_PIN_MASK (1UL<< 7) // Set/clear mask for PC7
    #define D2_PIN_MASK (1UL<<10) // Set/clear mask for PA10
    #define D3_PIN_MASK (1UL<< 3) // Set/clear mask for PB3
    #define D4_PIN_MASK (1UL<< 5) // Set/clear mask for PB5
    #define D5_PIN_MASK (1UL<< 4) // Set/clear mask for PB4
    #define D6_PIN_MASK (1UL<<10) // Set/clear mask for PB10
    #define D7_PIN_MASK (1UL<< 8) // Set/clear mask for PA8

    // Create bit set/reset mask based on LS byte of value B
    #define  D0_BSR_MASK(B) ((D0_PIN_MASK<<16)>>(((B)<< 4)&0x10))
    #define  D1_BSR_MASK(B) ((D1_PIN_MASK<<16)>>(((B)<< 3)&0x10))
    #define  D2_BSR_MASK(B) ((D2_PIN_MASK<<16)>>(((B)<< 2)&0x10))
    #define  D3_BSR_MASK(B) ((D3_PIN_MASK<<16)>>(((B)<< 1)&0x10))
    #define  D4_BSR_MASK(B) ((D4_PIN_MASK<<16)>>(((B)<< 0)&0x10))
    #define  D5_BSR_MASK(B) ((D5_PIN_MASK<<16)>>(((B)>> 1)&0x10))
    #define  D6_BSR_MASK(B) ((D6_PIN_MASK<<16)>>(((B)>> 2)&0x10))
    #define  D7_BSR_MASK(B) ((D7_PIN_MASK<<16)>>(((B)>> 3)&0x10))
    // Create bit set/reset mask for top byte of 16-bit value B
    #define  D8_BSR_MASK(B) ((D0_PIN_MASK<<16)>>(((B)>> 4)&0x10))
    #define  D9_BSR_MASK(B) ((D1_PIN_MASK<<16)>>(((B)>> 5)&0x10))
    #define D10_BSR_MASK(B) ((D2_PIN_MASK<<16)>>(((B)>> 6)&0x10))
    #define D11_BSR_MASK(B) ((D3_PIN_MASK<<16)>>(((B)>> 7)&0x10))
    #define D12_BSR_MASK(B) ((D4_PIN_MASK<<16)>>(((B)>> 8)&0x10))
    #define D13_BSR_MASK(B) ((D5_PIN_MASK<<16)>>(((B)>> 9)&0x10))
    #define D14_BSR_MASK(B) ((D6_PIN_MASK<<16)>>(((B)>>10)&0x10))
    #define D15_BSR_MASK(B) ((D7_PIN_MASK<<16)>>(((B)>>11)&0x10))

    // Write 8 bits to TFT
    #define tft_Write_8(C)   GPIOA->BSRR = D0_BSR_MASK(C) | D2_BSR_MASK(C) | D7_BSR_MASK(C); \
                             WR_L; \
                             GPIOC->BSRR = D1_BSR_MASK(C); \
                             GPIOB->BSRR = D3_BSR_MASK(C) | D4_BSR_MASK(C) | D5_BSR_MASK(C) | D6_BSR_MASK(C); \
                             WR_STB // Need to slow down strobe

    #if defined (SSD1963_DRIVER)

      // Write 18-bit color to TFT (untested)
      #define tft_Write_16(C)  r6 = (((C) & 0xF800)>> 8); g6 = (((C) & 0x07E0)>> 3); b6 = (((C) & 0x001F)<< 3); \
                               GPIOA->BSRR = D0_BSR_MASK(r6) | D2_BSR_MASK(r6) | D7_BSR_MASK(r6); \
                               WR_L; \
                               GPIOC->BSRR = D1_BSR_MASK(r6); \
                               GPIOB->BSRR = D3_BSR_MASK(r6) | D4_BSR_MASK(r6) | D5_BSR_MASK(r6) | D6_BSR_MASK(r6); \
                               WR_STB; \
                               GPIOA->BSRR = D0_BSR_MASK(g6) | D2_BSR_MASK(g6) | D7_BSR_MASK(g6); \
                               WR_L; \
                               GPIOC->BSRR = D1_BSR_MASK(g6); \
                               GPIOB->BSRR = D3_BSR_MASK(g6) | D4_BSR_MASK(g6) | D5_BSR_MASK(g6) | D6_BSR_MASK(g6); \
                               WR_STB; \
                               GPIOA->BSRR = D0_BSR_MASK(b6) | D2_BSR_MASK(b6) | D7_BSR_MASK(b6); \
                               WR_L; \
                               GPIOC->BSRR = D1_BSR_MASK(b6); \
                               GPIOB->BSRR = D3_BSR_MASK(b6) | D4_BSR_MASK(b6) | D5_BSR_MASK(b6) | D6_BSR_MASK(b6); \
                               WR_STB // Need to slow down strobe

      // 18-bit color write with swapped bytes
      #define tft_Write_16S(C) Cswap = ((C) >>8 | (C) << 8); tft_Write_16(Cswap)

    #else
      // Write 16 bits to TFT
      #define tft_Write_16(C)  GPIOA->BSRR = D8_BSR_MASK(C) | D10_BSR_MASK(C) | D15_BSR_MASK(C); \
                               WR_L; \
                               GPIOC->BSRR = D9_BSR_MASK(C); \
                               GPIOB->BSRR = D11_BSR_MASK(C) | D12_BSR_MASK(C) | D13_BSR_MASK(C) | D14_BSR_MASK(C); \
                               WR_STB; \
                               GPIOA->BSRR = D0_BSR_MASK(C) | D2_BSR_MASK(C) | D7_BSR_MASK(C); \
                               WR_L; \
                               GPIOC->BSRR = D1_BSR_MASK(C); \
                               GPIOB->BSRR = D3_BSR_MASK(C) | D4_BSR_MASK(C) | D5_BSR_MASK(C) | D6_BSR_MASK(C); \
                               WR_STB // Need to slow down strobe

      // 16-bit write with swapped bytes
      #define tft_Write_16S(C) GPIOA->BSRR = D0_BSR_MASK(C) | D2_BSR_MASK(C) | D7_BSR_MASK(C); \
                               WR_L; \
                               GPIOC->BSRR = D1_BSR_MASK(C); \
                               GPIOB->BSRR = D3_BSR_MASK(C) | D4_BSR_MASK(C) | D5_BSR_MASK(C) | D6_BSR_MASK(C); \
                               WR_STB; \
                               GPIOA->BSRR = D8_BSR_MASK(C) | D10_BSR_MASK(C) | D15_BSR_MASK(C); \
                               WR_L; \
                               GPIOC->BSRR = D9_BSR_MASK(C); \
                               GPIOB->BSRR = D11_BSR_MASK(C) | D12_BSR_MASK(C) | D13_BSR_MASK(C) | D14_BSR_MASK(C); \
                               WR_STB
    #endif

    #define tft_Write_32(C)    tft_Write_16((uint16_t)((C)>>16)); tft_Write_16((uint16_t)(C))

    #define tft_Write_32C(C,D) tft_Write_16((uint16_t)(C)); tft_Write_16((uint16_t)(D))

    #define tft_Write_32D(C)   tft_Write_16((uint16_t)(C)); tft_Write_16((uint16_t)(C))

    // Read a data bit
    #define RD_TFT_D0 (((GPIOA->IDR)&(D0_PIN_MASK))>>( 9-0)) // Read pin PA9
    #define RD_TFT_D1 (((GPIOC->IDR)&(D1_PIN_MASK))>>( 7-1)) // Read pin PC7
    #define RD_TFT_D2 (((GPIOA->IDR)&(D2_PIN_MASK))>>(10-2)) // Read pin PA10
    #define RD_TFT_D3 (((GPIOB->IDR)&(D3_PIN_MASK))>>( 3-3)) // Read pin PB3
    #define RD_TFT_D4 (((GPIOB->IDR)&(D4_PIN_MASK))>>( 5-4)) // Read pin PB5
    #define RD_TFT_D5 (((GPIOB->IDR)&(D5_PIN_MASK))<<(-4+5)) // Read pin PB4
    #define RD_TFT_D6 (((GPIOB->IDR)&(D6_PIN_MASK))>>(10-6)) // Read pin PB10
    #define RD_TFT_D7 (((GPIOA->IDR)&(D7_PIN_MASK))>>( 8-7)) // Read pin PA8

////////////////////////////////////////////////////////////////////////////////////////
// Nucleo 144: hard-coded pins
////////////////////////////////////////////////////////////////////////////////////////
  #elif defined (NUCLEO_144_TFT)

    // Convert Arduino pin reference Dx or STM pin reference PXn to port and mask
    // (diagnostic only - not used for Nucleo)
    #define D0_PIN_NAME  digitalPinToPinName(TFT_D0)
    #define D1_PIN_NAME  digitalPinToPinName(TFT_D1)
    #define D2_PIN_NAME  digitalPinToPinName(TFT_D2)
    #define D3_PIN_NAME  digitalPinToPinName(TFT_D3)
    #define D4_PIN_NAME  digitalPinToPinName(TFT_D4)
    #define D5_PIN_NAME  digitalPinToPinName(TFT_D5)
    #define D6_PIN_NAME  digitalPinToPinName(TFT_D6)
    #define D7_PIN_NAME  digitalPinToPinName(TFT_D7)

    // Pin port bit number 0-15 (diagnostic only - not used for Nucleo)
    #define D0_PIN_BIT  (D0_PIN_NAME & 0xF)
    #define D1_PIN_BIT  (D1_PIN_NAME & 0xF)
    #define D2_PIN_BIT  (D2_PIN_NAME & 0xF)
    #define D3_PIN_BIT  (D3_PIN_NAME & 0xF)
    #define D4_PIN_BIT  (D4_PIN_NAME & 0xF)
    #define D5_PIN_BIT  (D5_PIN_NAME & 0xF)
    #define D6_PIN_BIT  (D6_PIN_NAME & 0xF)
    #define D7_PIN_BIT  (D7_PIN_NAME & 0xF)


    #if !defined (STM32H7xx)

      // Ports associated with pins - get_GPIO_Port() seems to be slow...
      #define D0_PIN_PORT GPIOF
      #define D1_PIN_PORT GPIOD
      #define D2_PIN_PORT GPIOF
      #define D3_PIN_PORT GPIOE
      #define D4_PIN_PORT GPIOF
      #define D5_PIN_PORT GPIOE
      #define D6_PIN_PORT GPIOE
      #define D7_PIN_PORT GPIOF

      // Pin masks for set/clear
      #define D0_PIN_MASK (1UL<<12) // Set/clear mask for PF12 PF3
      #define D1_PIN_MASK (1UL<<15) // Set/clear mask for PD15
      #define D2_PIN_MASK (1UL<<15) // Set/clear mask for PF15 PG14
      #define D3_PIN_MASK (1UL<<13) // Set/clear mask for PE13
      #define D4_PIN_MASK (1UL<<14) // Set/clear mask for PF14
      #define D5_PIN_MASK (1UL<<11) // Set/clear mask for PE11
      #define D6_PIN_MASK (1UL<< 9) // Set/clear mask for PE9
      #define D7_PIN_MASK (1UL<<13) // Set/clear mask for PF13 PG12

      // Create bit set/reset mask based on LS byte of value B
      #define  D0_BSR_MASK(B) ((D0_PIN_MASK<<16)>>(((B)<< 4)&0x10))
      #define  D1_BSR_MASK(B) ((D1_PIN_MASK<<16)>>(((B)<< 3)&0x10))
      #define  D2_BSR_MASK(B) ((D2_PIN_MASK<<16)>>(((B)<< 2)&0x10))
      #define  D3_BSR_MASK(B) ((D3_PIN_MASK<<16)>>(((B)<< 1)&0x10))
      #define  D4_BSR_MASK(B) ((D4_PIN_MASK<<16)>>(((B)<< 0)&0x10))
      #define  D5_BSR_MASK(B) ((D5_PIN_MASK<<16)>>(((B)>> 1)&0x10))
      #define  D6_BSR_MASK(B) ((D6_PIN_MASK<<16)>>(((B)>> 2)&0x10))
      #define  D7_BSR_MASK(B) ((D7_PIN_MASK<<16)>>(((B)>> 3)&0x10))
      // Create bit set/reset mask for top byte of 16-bit value B
      #define  D8_BSR_MASK(B) ((D0_PIN_MASK<<16)>>(((B)>> 4)&0x10))
      #define  D9_BSR_MASK(B) ((D1_PIN_MASK<<16)>>(((B)>> 5)&0x10))
      #define D10_BSR_MASK(B) ((D2_PIN_MASK<<16)>>(((B)>> 6)&0x10))
      #define D11_BSR_MASK(B) ((D3_PIN_MASK<<16)>>(((B)>> 7)&0x10))
      #define D12_BSR_MASK(B) ((D4_PIN_MASK<<16)>>(((B)>> 8)&0x10))
      #define D13_BSR_MASK(B) ((D5_PIN_MASK<<16)>>(((B)>> 9)&0x10))
      #define D14_BSR_MASK(B) ((D6_PIN_MASK<<16)>>(((B)>>10)&0x10))
      #define D15_BSR_MASK(B) ((D7_PIN_MASK<<16)>>(((B)>>11)&0x10))


      // Write 8 bits to TFT
      #define tft_Write_8(C)   GPIOF->BSRR = D0_BSR_MASK(C) | D2_BSR_MASK(C) | D4_BSR_MASK(C) | D7_BSR_MASK(C); \
                               WR_L; \
                               GPIOD->BSRR = D1_BSR_MASK(C); \
                               GPIOE->BSRR = D3_BSR_MASK(C) | D5_BSR_MASK(C) | D6_BSR_MASK(C); \
                               WR_STB

      #if defined (SSD1963_DRIVER)

        // Write 18-bit color to TFT (untested)
        #define tft_Write_16(C)  r6 = (((C) & 0xF800)>> 8); g6 = (((C) & 0x07E0)>> 3); b6 = (((C) & 0x001F)<< 3); \
                                 GPIOF->BSRR = D0_BSR_MASK(r6) | D2_BSR_MASK(r6) | D4_BSR_MASK(r6) | D7_BSR_MASK(r6); \
                                 WR_L; \
                                 GPIOD->BSRR = D1_BSR_MASK(r6); \
                                 GPIOE->BSRR = D3_BSR_MASK(r6) | D5_BSR_MASK(r6) | D6_BSR_MASK(r6); \
                                 WR_STB; \
                                 GPIOF->BSRR = D0_BSR_MASK(g6) | D2_BSR_MASK(g6) | D4_BSR_MASK(g6) | D7_BSR_MASK(g6); \
                                 WR_L; \
                                 GPIOD->BSRR = D1_BSR_MASK(g6); \
                                 GPIOE->BSRR = D3_BSR_MASK(g6) | D5_BSR_MASK(g6) | D6_BSR_MASK(g6); \
                                 WR_STB; \
                                 GPIOF->BSRR = D0_BSR_MASK(b6) | D2_BSR_MASK(b6) | D4_BSR_MASK(b6) | D7_BSR_MASK(b6); \
                                 WR_L; \
                                 GPIOD->BSRR = D1_BSR_MASK(b6); \
                                 GPIOE->BSRR = D3_BSR_MASK(b6) | D5_BSR_MASK(b6) | D6_BSR_MASK(b6); \
                                 WR_STB // Need to slow down strobe

        // 18-bit color write with swapped bytes
        #define tft_Write_16S(C) Cswap = ((C) >>8 | (C) << 8); tft_Write_16(Cswap)

      #else

        // Write 16 bits to TFT
        #define tft_Write_16(C)  GPIOF->BSRR = D8_BSR_MASK(C) | D10_BSR_MASK(C) | D12_BSR_MASK(C) | D15_BSR_MASK(C); \
                                 WR_L; \
                                 GPIOD->BSRR = D9_BSR_MASK(C); \
                                 GPIOE->BSRR = D11_BSR_MASK(C) | D13_BSR_MASK(C) | D14_BSR_MASK(C); \
                                 WR_STB;\
                                 GPIOF->BSRR = D0_BSR_MASK(C) | D2_BSR_MASK(C) | D4_BSR_MASK(C) | D7_BSR_MASK(C); \
                                 WR_L; \
                                 GPIOD->BSRR = D1_BSR_MASK(C); \
                                 GPIOE->BSRR = D3_BSR_MASK(C) | D5_BSR_MASK(C) | D6_BSR_MASK(C); \
                                 WR_STB

        // 16-bit write with swapped bytes
        #define tft_Write_16S(C) GPIOF->BSRR = D0_BSR_MASK(C) | D2_BSR_MASK(C) | D4_BSR_MASK(C) | D7_BSR_MASK(C); \
                                 WR_L; \
                                 GPIOD->BSRR = D1_BSR_MASK(C); \
                                 GPIOE->BSRR = D3_BSR_MASK(C) | D5_BSR_MASK(C) | D6_BSR_MASK(C); \
                                 WR_STB; \
                                 GPIOF->BSRR = D8_BSR_MASK(C) | D10_BSR_MASK(C) | D12_BSR_MASK(C) | D15_BSR_MASK(C); \
                                 WR_L; \
                                 GPIOD->BSRR = D9_BSR_MASK(C); \
                                 GPIOE->BSRR = D11_BSR_MASK(C) | D13_BSR_MASK(C) | D14_BSR_MASK(C); \
                                 WR_STB

      #endif

      #define tft_Write_32(C)    tft_Write_16((uint16_t)((C)>>16)); tft_Write_16((uint16_t)(C))

      #define tft_Write_32C(C,D) tft_Write_16((uint16_t)(C)); tft_Write_16((uint16_t)(D))

      #define tft_Write_32D(C)   tft_Write_16((uint16_t)(C)); tft_Write_16((uint16_t)(C))

      // Read a data bit
      #define RD_TFT_D0 (((GPIOF->IDR)&(D0_PIN_MASK))>>(12-0)) // Read pin PF12
      #define RD_TFT_D1 (((GPIOD->IDR)&(D1_PIN_MASK))>>(15-1)) // Read pin PD15
      #define RD_TFT_D2 (((GPIOF->IDR)&(D2_PIN_MASK))>>(15-2)) // Read pin PF15
      #define RD_TFT_D3 (((GPIOE->IDR)&(D3_PIN_MASK))>>(13-3)) // Read pin PE13
      #define RD_TFT_D4 (((GPIOF->IDR)&(D4_PIN_MASK))>>(14-4)) // Read pin PF14
      #define RD_TFT_D5 (((GPIOE->IDR)&(D5_PIN_MASK))>>(11-5)) // Read pin PE11
      #define RD_TFT_D6 (((GPIOE->IDR)&(D6_PIN_MASK))>>( 9-6)) // Read pin PE9
      #define RD_TFT_D7 (((GPIOF->IDR)&(D7_PIN_MASK))>>(13-7)) // Read pin PF13

    #else

      // Test setup for STM32H743 - starts to run, slow and then crashes! Board support bug?

      // Ports associated with pins - get_GPIO_Port() seems to be slow...
      #define D0_PIN_PORT GPIOF
      #define D1_PIN_PORT GPIOD
      #define D2_PIN_PORT GPIOG
      #define D3_PIN_PORT GPIOE
      #define D4_PIN_PORT GPIOE
      #define D5_PIN_PORT GPIOE
      #define D6_PIN_PORT GPIOE
      #define D7_PIN_PORT GPIOG

      // Pin masks for set/clear
      #define D0_PIN_MASK (1UL<< 3) // Set/clear mask for PF3
      #define D1_PIN_MASK (1UL<<15) // Set/clear mask for PD15
      #define D2_PIN_MASK (1UL<<14) // Set/clear mask for PG14
      #define D3_PIN_MASK (1UL<<13) // Set/clear mask for PE13
      #define D4_PIN_MASK (1UL<<14) // Set/clear mask for PE14
      #define D5_PIN_MASK (1UL<<11) // Set/clear mask for PE11
      #define D6_PIN_MASK (1UL<< 9) // Set/clear mask for PE9
      #define D7_PIN_MASK (1UL<<12) // Set/clear mask for PG12

      // Create bit set/reset mask based on LS byte of value B
      #define  D0_BSR_MASK(B) ((D0_PIN_MASK<<16)>>(((B)<< 4)&0x10))
      #define  D1_BSR_MASK(B) ((D1_PIN_MASK<<16)>>(((B)<< 3)&0x10))
      #define  D2_BSR_MASK(B) ((D2_PIN_MASK<<16)>>(((B)<< 2)&0x10))
      #define  D3_BSR_MASK(B) ((D3_PIN_MASK<<16)>>(((B)<< 1)&0x10))
      #define  D4_BSR_MASK(B) ((D4_PIN_MASK<<16)>>(((B)<< 0)&0x10))
      #define  D5_BSR_MASK(B) ((D5_PIN_MASK<<16)>>(((B)>> 1)&0x10))
      #define  D6_BSR_MASK(B) ((D6_PIN_MASK<<16)>>(((B)>> 2)&0x10))
      #define  D7_BSR_MASK(B) ((D7_PIN_MASK<<16)>>(((B)>> 3)&0x10))
      // Create bit set/reset mask for top byte of 16-bit value B
      #define  D8_BSR_MASK(B) ((D0_PIN_MASK<<16)>>(((B)>> 4)&0x10))
      #define  D9_BSR_MASK(B) ((D1_PIN_MASK<<16)>>(((B)>> 5)&0x10))
      #define D10_BSR_MASK(B) ((D2_PIN_MASK<<16)>>(((B)>> 6)&0x10))
      #define D11_BSR_MASK(B) ((D3_PIN_MASK<<16)>>(((B)>> 7)&0x10))
      #define D12_BSR_MASK(B) ((D4_PIN_MASK<<16)>>(((B)>> 8)&0x10))
      #define D13_BSR_MASK(B) ((D5_PIN_MASK<<16)>>(((B)>> 9)&0x10))
      #define D14_BSR_MASK(B) ((D6_PIN_MASK<<16)>>(((B)>>10)&0x10))
      #define D15_BSR_MASK(B) ((D7_PIN_MASK<<16)>>(((B)>>11)&0x10))


      // Write 8 bits to TFT
      #define tft_Write_8(C)   GPIOF->BSRR = D0_BSR_MASK(C); \
                               GPIOG->BSRR = D2_BSR_MASK(C) | D7_BSR_MASK(C); \
                               WR_L; \
                               GPIOD->BSRR = D1_BSR_MASK(C); \
                               GPIOE->BSRR = D3_BSR_MASK(C) | D4_BSR_MASK(C) | D5_BSR_MASK(C) | D6_BSR_MASK(C); \
                               WR_STB

      // Write 16 bits to TFT
      #define tft_Write_16(C)  GPIOF->BSRR = D8_BSR_MASK(C); \
                               GPIOG->BSRR = D10_BSR_MASK(C) | D15_BSR_MASK(C); \
                               WR_L; \
                               GPIOD->BSRR = D9_BSR_MASK(C); \
                               GPIOE->BSRR = D11_BSR_MASK(C) | D12_BSR_MASK(C) | D13_BSR_MASK(C) | D14_BSR_MASK(C); \
                               WR_STB;\
                               GPIOF->BSRR = D0_BSR_MASK(C); \
                               GPIOG->BSRR = D2_BSR_MASK(C) | D7_BSR_MASK(C); \
                               WR_L; \
                               GPIOD->BSRR = D1_BSR_MASK(C); \
                               GPIOE->BSRR = D3_BSR_MASK(C) | D4_BSR_MASK(C) | D5_BSR_MASK(C) | D6_BSR_MASK(C); \
                               WR_STB

      // 16-bit write with swapped bytes
      #define tft_Write_16S(C) GPIOF->BSRR = D0_BSR_MASK(C); \
                               GPIOG->BSRR = D2_BSR_MASK(C) | D7_BSR_MASK(C); \
                               WR_L; \
                               GPIOD->BSRR = D1_BSR_MASK(C); \
                               GPIOE->BSRR = D3_BSR_MASK(C) | D4_BSR_MASK(C) | D5_BSR_MASK(C) | D6_BSR_MASK(C); \
                               WR_STB; \
                               GPIOF->BSRR = D8_BSR_MASK(C); \
                               GPIOG->BSRR = D10_BSR_MASK(C) | D15_BSR_MASK(C); \
                               WR_L; \
                               GPIOD->BSRR = D9_BSR_MASK(C); \
                               GPIOE->BSRR = D11_BSR_MASK(C) | D12_BSR_MASK(C) | D13_BSR_MASK(C) | D14_BSR_MASK(C); \
                               WR_STB

      #define tft_Write_32(C)    tft_Write_16((uint16_t)((C)>>16)); tft_Write_16((uint16_t)(C))

      #define tft_Write_32C(C,D) tft_Write_16((uint16_t)(C)); tft_Write_16((uint16_t)(D))

      #define tft_Write_32D(C)   tft_Write_16((uint16_t)(C)); tft_Write_16((uint16_t)(C))

      // Read a data bit
      #define RD_TFT_D0 (((GPIOF->IDR)&(D0_PIN_MASK))>>( 3-0)) // Read pin PF3
      #define RD_TFT_D1 (((GPIOD->IDR)&(D1_PIN_MASK))>>(15-1)) // Read pin PD15
      #define RD_TFT_D2 (((GPIOG->IDR)&(D2_PIN_MASK))>>(14-2)) // Read pin PG14
      #define RD_TFT_D3 (((GPIOE->IDR)&(D3_PIN_MASK))>>(13-3)) // Read pin PE13
      #define RD_TFT_D4 (((GPIOE->IDR)&(D4_PIN_MASK))>>(14-4)) // Read pin PE14
      #define RD_TFT_D5 (((GPIOE->IDR)&(D5_PIN_MASK))>>(11-5)) // Read pin PE11
      #define RD_TFT_D6 (((GPIOE->IDR)&(D6_PIN_MASK))>>( 9-6)) // Read pin PE9
      #define RD_TFT_D7 (((GPIOG->IDR)&(D7_PIN_MASK))>>(12-7)) // Read pin PG12

    #endif
////////////////////////////////////////////////////////////////////////////////////////
// Support for other STM32 boards (not optimised!)
////////////////////////////////////////////////////////////////////////////////////////
  #else
    #if defined (STM_PORTA_DATA_BUS) || defined (STM_PORTB_DATA_BUS) || defined (STM_PORTC_DATA_BUS) || defined (STM_PORTD_DATA_BUS)
      #if defined (STM_PORTA_DATA_BUS)
        #define GPIOX GPIOA
      #elif defined (STM_PORTB_DATA_BUS)
        #define GPIOX GPIOB
      #elif defined (STM_PORTC_DATA_BUS)
        #define GPIOX GPIOC
      #elif defined (STM_PORTD_DATA_BUS)
        #define GPIOX GPIOD
      #endif

      // Write 8 bits to TFT
      #define tft_Write_8(C)   GPIOX->BSRR = (0x00FF0000 | (uint8_t)(C)); WR_L; WR_STB

      #if defined (SSD1963_DRIVER)

        // Write 18-bit color to TFT (untested)

        #define tft_Write_16(C)  r6 = (((C) & 0xF800)>> 8); g6 = (((C) & 0x07E0)>> 3); b6 = (((C) & 0x001F)<< 3); \
                             GPIOX->BSRR = (0x00FF0000 | (uint8_t)(r6)); WR_L; WR_STB; \
                             GPIOX->BSRR = (0x00FF0000 | (uint8_t)(g6)); WR_L; WR_STB; \
                             GPIOX->BSRR = (0x00FF0000 | (uint8_t)(b6)); WR_L; WR_STB

        // 18-bit color write with swapped bytes
        #define tft_Write_16S(C) Cswap = ((C) >>8 | (C) << 8); tft_Write_16(Cswap)

      #else

          // Write 16 bits to TFT
          #define tft_Write_16(C)  GPIOX->BSRR = (0x00FF0000 | (uint8_t)(C>>8)); WR_L; WR_STB; \
                                   GPIOX->BSRR = (0x00FF0000 | (uint8_t)(C>>0)); WR_L; WR_STB

          // 16-bit write with swapped bytes
          #define tft_Write_16S(C) GPIOX->BSRR = (0x00FF0000 | (uint8_t)(C>>0)); WR_L; WR_STB; \
                                   GPIOX->BSRR = (0x00FF0000 | (uint8_t)(C>>8)); WR_L; WR_STB
      #endif

      #define tft_Write_32(C)    tft_Write_16((uint16_t)((C)>>16)); tft_Write_16((uint16_t)(C))

      #define tft_Write_32C(C,D) tft_Write_16((uint16_t)(C)); tft_Write_16((uint16_t)(D))

      #define tft_Write_32D(C)   tft_Write_16((uint16_t)(C)); tft_Write_16((uint16_t)(C))

      // Read a data bit
      #define RD_TFT_D0 ((GPIOX->IDR) & 0x01) // Read pin TFT_D0
      #define RD_TFT_D1 ((GPIOX->IDR) & 0x02) // Read pin TFT_D1
      #define RD_TFT_D2 ((GPIOX->IDR) & 0x04) // Read pin TFT_D2
      #define RD_TFT_D3 ((GPIOX->IDR) & 0x08) // Read pin TFT_D3
      #define RD_TFT_D4 ((GPIOX->IDR) & 0x10) // Read pin TFT_D4
      #define RD_TFT_D5 ((GPIOX->IDR) & 0x20) // Read pin TFT_D5
      #define RD_TFT_D6 ((GPIOX->IDR) & 0x40) // Read pin TFT_D6
      #define RD_TFT_D7 ((GPIOX->IDR) & 0x80) // Read pin TFT_D7

    #else
      // This will work with any STM32 to parallel TFT pin mapping but will be slower

      // Convert Arduino pin reference Dx or STM pin reference PXn to port and mask
      #define D0_PIN_NAME  digitalPinToPinName(TFT_D0)
      #define D1_PIN_NAME  digitalPinToPinName(TFT_D1)
      #define D2_PIN_NAME  digitalPinToPinName(TFT_D2)
      #define D3_PIN_NAME  digitalPinToPinName(TFT_D3)
      #define D4_PIN_NAME  digitalPinToPinName(TFT_D4)
      #define D5_PIN_NAME  digitalPinToPinName(TFT_D5)
      #define D6_PIN_NAME  digitalPinToPinName(TFT_D6)
      #define D7_PIN_NAME  digitalPinToPinName(TFT_D7)

      // Pin port bit number 0-15
      #define D0_PIN_BIT  (D0_PIN_NAME & 0xF)
      #define D1_PIN_BIT  (D1_PIN_NAME & 0xF)
      #define D2_PIN_BIT  (D2_PIN_NAME & 0xF)
      #define D3_PIN_BIT  (D3_PIN_NAME & 0xF)
      #define D4_PIN_BIT  (D4_PIN_NAME & 0xF)
      #define D5_PIN_BIT  (D5_PIN_NAME & 0xF)
      #define D6_PIN_BIT  (D6_PIN_NAME & 0xF)
      #define D7_PIN_BIT  (D7_PIN_NAME & 0xF)

      // Pin port
      #define D0_PIN_PORT  digitalPinToPort(TFT_D0)
      #define D1_PIN_PORT  digitalPinToPort(TFT_D1)
      #define D2_PIN_PORT  digitalPinToPort(TFT_D2)
      #define D3_PIN_PORT  digitalPinToPort(TFT_D3)
      #define D4_PIN_PORT  digitalPinToPort(TFT_D4)
      #define D5_PIN_PORT  digitalPinToPort(TFT_D5)
      #define D6_PIN_PORT  digitalPinToPort(TFT_D6)
      #define D7_PIN_PORT  digitalPinToPort(TFT_D7)

      // Pin masks for set/clear
      #define D0_PIN_MASK  digitalPinToBitMask(TFT_D0)
      #define D1_PIN_MASK  digitalPinToBitMask(TFT_D1)
      #define D2_PIN_MASK  digitalPinToBitMask(TFT_D2)
      #define D3_PIN_MASK  digitalPinToBitMask(TFT_D3)
      #define D4_PIN_MASK  digitalPinToBitMask(TFT_D4)
      #define D5_PIN_MASK  digitalPinToBitMask(TFT_D5)
      #define D6_PIN_MASK  digitalPinToBitMask(TFT_D6)
      #define D7_PIN_MASK  digitalPinToBitMask(TFT_D7)

      // Create bit set/reset mask based on LS byte of value B
      #define  D0_BSR_MASK(B) ((D0_PIN_MASK<<16)>>(((B)<< 4)&0x10))
      #define  D1_BSR_MASK(B) ((D1_PIN_MASK<<16)>>(((B)<< 3)&0x10))
      #define  D2_BSR_MASK(B) ((D2_PIN_MASK<<16)>>(((B)<< 2)&0x10))
      #define  D3_BSR_MASK(B) ((D3_PIN_MASK<<16)>>(((B)<< 1)&0x10))
      #define  D4_BSR_MASK(B) ((D4_PIN_MASK<<16)>>(((B)<< 0)&0x10))
      #define  D5_BSR_MASK(B) ((D5_PIN_MASK<<16)>>(((B)>> 1)&0x10))
      #define  D6_BSR_MASK(B) ((D6_PIN_MASK<<16)>>(((B)>> 2)&0x10))
      #define  D7_BSR_MASK(B) ((D7_PIN_MASK<<16)>>(((B)>> 3)&0x10))
      // Create bit set/reset mask for top byte of 16-bit value B
      #define  D8_BSR_MASK(B) ((D0_PIN_MASK<<16)>>(((B)>> 4)&0x10))
      #define  D9_BSR_MASK(B) ((D1_PIN_MASK<<16)>>(((B)>> 5)&0x10))
      #define D10_BSR_MASK(B) ((D2_PIN_MASK<<16)>>(((B)>> 6)&0x10))
      #define D11_BSR_MASK(B) ((D3_PIN_MASK<<16)>>(((B)>> 7)&0x10))
      #define D12_BSR_MASK(B) ((D4_PIN_MASK<<16)>>(((B)>> 8)&0x10))
      #define D13_BSR_MASK(B) ((D5_PIN_MASK<<16)>>(((B)>> 9)&0x10))
      #define D14_BSR_MASK(B) ((D6_PIN_MASK<<16)>>(((B)>>10)&0x10))
      #define D15_BSR_MASK(B) ((D7_PIN_MASK<<16)>>(((B)>>11)&0x10))


      // Write 8 bits to TFT
      #define tft_Write_8(C)   D0_PIN_PORT->BSRR = D0_BSR_MASK(C); \
                               D1_PIN_PORT->BSRR = D1_BSR_MASK(C); \
                               D2_PIN_PORT->BSRR = D2_BSR_MASK(C); \
                               D3_PIN_PORT->BSRR = D3_BSR_MASK(C); \
                               WR_L; \
                               D4_PIN_PORT->BSRR = D4_BSR_MASK(C); \
                               D5_PIN_PORT->BSRR = D5_BSR_MASK(C); \
                               D6_PIN_PORT->BSRR = D6_BSR_MASK(C); \
                               D7_PIN_PORT->BSRR = D7_BSR_MASK(C); \
                               WR_STB

      #if defined (SSD1963_DRIVER)

        // Write 18-bit color to TFT (untested)
        #define tft_Write_16(C)  r6 = (((C) & 0xF800)>> 8); g6 = (((C) & 0x07E0)>> 3); b6 = (((C) & 0x001F)<< 3); \
                                 D0_PIN_PORT->BSRR = D8_BSR_MASK(r6);  \
                                 D1_PIN_PORT->BSRR = D9_BSR_MASK(r6);  \
                                 D2_PIN_PORT->BSRR = D10_BSR_MASK(r6); \
                                 D3_PIN_PORT->BSRR = D11_BSR_MASK(r6); \
                                 WR_L; \
                                 D4_PIN_PORT->BSRR = D12_BSR_MASK(r6); \
                                 D5_PIN_PORT->BSRR = D13_BSR_MASK(r6); \
                                 D6_PIN_PORT->BSRR = D14_BSR_MASK(r6); \
                                 D7_PIN_PORT->BSRR = D15_BSR_MASK(r6); \
                                 WR_STB;\
                                 D0_PIN_PORT->BSRR = D8_BSR_MASK(g6);  \
                                 D1_PIN_PORT->BSRR = D9_BSR_MASK(g6);  \
                                 D2_PIN_PORT->BSRR = D10_BSR_MASK(g6); \
                                 D3_PIN_PORT->BSRR = D11_BSR_MASK(g6); \
                                 WR_L; \
                                 D4_PIN_PORT->BSRR = D12_BSR_MASK(g6); \
                                 D5_PIN_PORT->BSRR = D13_BSR_MASK(g6); \
                                 D6_PIN_PORT->BSRR = D14_BSR_MASK(g6); \
                                 D7_PIN_PORT->BSRR = D15_BSR_MASK(g6); \
                                 WR_STB;\
                                 D0_PIN_PORT->BSRR = D0_BSR_MASK(b6); \
                                 D1_PIN_PORT->BSRR = D1_BSR_MASK(b6); \
                                 D2_PIN_PORT->BSRR = D2_BSR_MASK(b6); \
                                 D3_PIN_PORT->BSRR = D3_BSR_MASK(b6); \
                                 WR_L; \
                                 D4_PIN_PORT->BSRR = D4_BSR_MASK(b6); \
                                 D5_PIN_PORT->BSRR = D5_BSR_MASK(b6); \
                                 D6_PIN_PORT->BSRR = D6_BSR_MASK(b6); \
                                 D7_PIN_PORT->BSRR = D7_BSR_MASK(b6); \
                                 WR_STB

        // 18-bit color write with swapped bytes
        #define tft_Write_16S(C) Cswap = ((C) >>8 | (C) << 8); tft_Write_16(Cswap)

      #else

        // Write 16 bits to TFT
        #define tft_Write_16(C)  D0_PIN_PORT->BSRR = D8_BSR_MASK(C);  \
                                 D1_PIN_PORT->BSRR = D9_BSR_MASK(C);  \
                                 D2_PIN_PORT->BSRR = D10_BSR_MASK(C); \
                                 D3_PIN_PORT->BSRR = D11_BSR_MASK(C); \
                                 WR_L; \
                                 D4_PIN_PORT->BSRR = D12_BSR_MASK(C); \
                                 D5_PIN_PORT->BSRR = D13_BSR_MASK(C); \
                                 D6_PIN_PORT->BSRR = D14_BSR_MASK(C); \
                                 D7_PIN_PORT->BSRR = D15_BSR_MASK(C); \
                                 WR_STB;\
                                 D0_PIN_PORT->BSRR = D0_BSR_MASK(C); \
                                 D1_PIN_PORT->BSRR = D1_BSR_MASK(C); \
                                 D2_PIN_PORT->BSRR = D2_BSR_MASK(C); \
                                 D3_PIN_PORT->BSRR = D3_BSR_MASK(C); \
                                 WR_L; \
                                 D4_PIN_PORT->BSRR = D4_BSR_MASK(C); \
                                 D5_PIN_PORT->BSRR = D5_BSR_MASK(C); \
                                 D6_PIN_PORT->BSRR = D6_BSR_MASK(C); \
                                 D7_PIN_PORT->BSRR = D7_BSR_MASK(C); \
                                 WR_STB

        // 16-bit write with swapped bytes
        #define tft_Write_16S(C) D0_PIN_PORT->BSRR = D0_BSR_MASK(C); \
                                 D1_PIN_PORT->BSRR = D1_BSR_MASK(C); \
                                 D2_PIN_PORT->BSRR = D2_BSR_MASK(C); \
                                 D3_PIN_PORT->BSRR = D3_BSR_MASK(C); \
                                 WR_L; \
                                 D4_PIN_PORT->BSRR = D4_BSR_MASK(C); \
                                 D5_PIN_PORT->BSRR = D5_BSR_MASK(C); \
                                 D6_PIN_PORT->BSRR = D6_BSR_MASK(C); \
                                 D7_PIN_PORT->BSRR = D7_BSR_MASK(C); \
                                 WR_STB; \
                                 D0_PIN_PORT->BSRR = D8_BSR_MASK(C);  \
                                 D1_PIN_PORT->BSRR = D9_BSR_MASK(C);  \
                                 D2_PIN_PORT->BSRR = D10_BSR_MASK(C); \
                                 D3_PIN_PORT->BSRR = D11_BSR_MASK(C); \
                                 WR_L; \
                                 D4_PIN_PORT->BSRR = D12_BSR_MASK(C); \
                                 D5_PIN_PORT->BSRR = D13_BSR_MASK(C); \
                                 D6_PIN_PORT->BSRR = D14_BSR_MASK(C); \
                                 D7_PIN_PORT->BSRR = D15_BSR_MASK(C); \
                                 WR_STB
      #endif

      #define tft_Write_32(C)    tft_Write_16((uint16_t)((C)>>16)); tft_Write_16((uint16_t)(C))

      #define tft_Write_32C(C,D) tft_Write_16((uint16_t)(C)); tft_Write_16((uint16_t)(D))

      #define tft_Write_32D(C)   tft_Write_16((uint16_t)(C)); tft_Write_16((uint16_t)(C))

      // Read a data bit
      #define RD_TFT_D0 ((((D0_PIN_PORT->IDR) >> (D0_PIN_BIT))&1)<<0) // Read pin TFT_D0
      #define RD_TFT_D1 ((((D1_PIN_PORT->IDR) >> (D1_PIN_BIT))&1)<<1) // Read pin TFT_D1
      #define RD_TFT_D2 ((((D2_PIN_PORT->IDR) >> (D2_PIN_BIT))&1)<<2) // Read pin TFT_D2
      #define RD_TFT_D3 ((((D3_PIN_PORT->IDR) >> (D3_PIN_BIT))&1)<<3) // Read pin TFT_D3
      #define RD_TFT_D4 ((((D4_PIN_PORT->IDR) >> (D4_PIN_BIT))&1)<<4) // Read pin TFT_D4
      #define RD_TFT_D5 ((((D5_PIN_PORT->IDR) >> (D5_PIN_BIT))&1)<<5) // Read pin TFT_D5
      #define RD_TFT_D6 ((((D6_PIN_PORT->IDR) >> (D6_PIN_BIT))&1)<<6) // Read pin TFT_D6
      #define RD_TFT_D7 ((((D7_PIN_PORT->IDR) >> (D7_PIN_BIT))&1)<<7) // Read pin TFT_D7
    #endif
  #endif
////////////////////////////////////////////////////////////////////////////////////////
// Macros to write commands/pixel colour data to a SPI ILI948x TFT
////////////////////////////////////////////////////////////////////////////////////////
#elif  defined (SPI_18BIT_DRIVER) // SPI 18-bit colour

  #define SPI_TXE_CHECK  while(!__HAL_SPI_GET_FLAG(&spiHal, SPI_FLAG_TXE)){}
                         //BSY check must allow for APB clock delay by checking TXE flag first
  #define SPI_BUSY_CHECK SPI_TXE_CHECK; while( __HAL_SPI_GET_FLAG(&spiHal, SPI_FLAG_BSY)){}
  #define TX_FIFO        SPI_TXE_CHECK; *((__IO uint8_t *)&SPIX->DR)

  //#define tft_Write_8(C)   spi.transfer(C)
  #define tft_Write_8(C)   TX_FIFO = (C); SPI_BUSY_CHECK
  #define tft_Write_16(C)  TX_FIFO = ((C) & 0xF800)>>8; TX_FIFO = ((C) & 0x07E0)>>3; TX_FIFO = ((C) & 0x001F)<<3; SPI_BUSY_CHECK
  #define tft_Write_16S(C) TX_FIFO = (C) & 0xF8; TX_FIFO = ((C) & 0xE000)>>11 | ((C) & 0x07)<<5; TX_FIFO = ((C) & 0x1F00)>>5; SPI_BUSY_CHECK
  #define tft_Write_16N(C) TX_FIFO = ((C) & 0xF800)>>8; TX_FIFO = ((C) & 0x07E0)>>3; TX_FIFO = ((C) & 0x001F)<<3

  #define tft_Write_32(C) \
    TX_FIFO = (C)>>24; TX_FIFO = (C)>>16; \
    TX_FIFO = (C)>>8; TX_FIFO = (C); SPI_BUSY_CHECK

  #define tft_Write_32C(C,D) \
    TX_FIFO = (C)>>8; TX_FIFO = (C); \
    TX_FIFO = (D)>>8; TX_FIFO = (D); SPI_BUSY_CHECK

  #define tft_Write_32D(C) \
    TX_FIFO = (C)>>8; TX_FIFO = (C); \
    TX_FIFO = (C)>>8; TX_FIFO = (C); SPI_BUSY_CHECK

////////////////////////////////////////////////////////////////////////////////////////
// Macros to write commands/pixel colour data to a SPI Raspberry Pi TFT
////////////////////////////////////////////////////////////////////////////////////////
#elif  defined (RPI_DISPLAY_TYPE)

  #define SPI_TXE_CHECK  while(!__HAL_SPI_GET_FLAG(&spiHal, SPI_FLAG_TXE)){}
                         //BSY check must allow for APB clock delay by checking TXE flag first
  #define SPI_BUSY_CHECK SPI_TXE_CHECK; while( __HAL_SPI_GET_FLAG(&spiHal, SPI_FLAG_BSY)){}
  #define TX_FIFO        SPI_TXE_CHECK; *((__IO uint8_t *)&SPIX->DR)

  //#define tft_Write_8(C)   spi.transfer(C)
  #define tft_Write_8(C)   TX_FIFO = (0); TX_FIFO = (C); SPI_BUSY_CHECK
  #define tft_Write_16(C)  TX_FIFO = (C)>>8; TX_FIFO = (C); SPI_BUSY_CHECK
  #define tft_Write_16S(C) TX_FIFO = (C); TX_FIFO = (C)>>8; SPI_BUSY_CHECK
  #define tft_Write_16N(C) TX_FIFO = (C)>>8; TX_FIFO = (C)

  #define tft_Write_32(C) \
    TX_FIFO = (C)>>24; TX_FIFO = (C)>>16; \
    TX_FIFO = (C)>>8; TX_FIFO = (C); SPI_BUSY_CHECK

  #define tft_Write_32C(C,D) \
    TX_FIFO = (0); TX_FIFO = (C)>>8; TX_FIFO = (0); TX_FIFO = (C); \
    TX_FIFO = (0); TX_FIFO = (D)>>8; TX_FIFO = (0); TX_FIFO = (D); SPI_BUSY_CHECK

  #define tft_Write_32D(C) \
    TX_FIFO = (0); TX_FIFO = (C)>>8; TX_FIFO = (0); TX_FIFO = (C); \
    TX_FIFO = (0); TX_FIFO = (C)>>8; TX_FIFO = (0); TX_FIFO = (C); SPI_BUSY_CHECK

////////////////////////////////////////////////////////////////////////////////////////
// Macros for all other SPI displays
////////////////////////////////////////////////////////////////////////////////////////

#else

  //#define DC_DELAY delayMicroseconds(1) // Premature BSY clear Hardware bug?

  #define SPI_TXE_CHECK  while(!__HAL_SPI_GET_FLAG(&spiHal, SPI_FLAG_TXE)){}
                         //BSY check must allow for APB clock delay by checking TXE flag first
  #define SPI_BUSY_CHECK SPI_TXE_CHECK; while( __HAL_SPI_GET_FLAG(&spiHal, SPI_FLAG_BSY)){}
  #define TX_FIFO        SPI_TXE_CHECK; *((__IO uint8_t *)&SPIX->DR)

  #define tft_Write_8(C)   TX_FIFO = (C); SPI_BUSY_CHECK
  #define tft_Write_16(C)  TX_FIFO = (C)>>8; TX_FIFO = (C); SPI_BUSY_CHECK
  #define tft_Write_16S(C) TX_FIFO = (C); TX_FIFO = (C)>>8; SPI_BUSY_CHECK
  #define tft_Write_16N(C) TX_FIFO = (C)>>8; TX_FIFO = (C)

  #define tft_Write_32(C) \
    TX_FIFO = (C)>>24; TX_FIFO = (C)>>16; \
    tft_Write_16((uint16_t) ((C)>>0))

  #define tft_Write_32C(C,D) \
    TX_FIFO = (C)>>8; TX_FIFO = (C); \
    tft_Write_16((uint16_t) (D))

  #define tft_Write_32D(C) \
    TX_FIFO = (C)>>8; TX_FIFO = (C); \
    tft_Write_16((uint16_t) (C))

#endif

#ifndef DC_DELAY
  //#define DC_DELAY delayMicroseconds(1) // Premature BSY clear hardware bug?
  #define DC_DELAY
#endif

#ifndef tft_Write_16N
  #define tft_Write_16N tft_Write_16
#endif

////////////////////////////////////////////////////////////////////////////////////////
// Macros to read from display using SPI or software SPI
////////////////////////////////////////////////////////////////////////////////////////
#if defined (TFT_SDA_READ)
  // Macros to support a bit banged function call for STM32 and bi-directional SDA pin
  #define TFT_eSPI_ENABLE_8_BIT_READ // Enable tft_Read_8();
  #define SCLK_L digitalWrite(TFT_SCLK, LOW)
  #define SCLK_H digitalWrite(TFT_SCLK, HIGH)
#elif !defined (TFT_PARALLEL_8_BIT)
  // Use a SPI read transfer
  #define tft_Read_8() spi.transfer(0)
#endif

#endif // Header end
