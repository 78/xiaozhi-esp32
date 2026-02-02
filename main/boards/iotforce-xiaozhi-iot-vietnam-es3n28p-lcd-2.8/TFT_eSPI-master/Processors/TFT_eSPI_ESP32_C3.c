        ////////////////////////////////////////////////////
        // TFT_eSPI driver functions for ESP32 processors //
        ////////////////////////////////////////////////////

// Temporarily a separate file to TFT_eSPI_ESP32.c until board package low level API stabilises

////////////////////////////////////////////////////////////////////////////////////////
// Global variables
////////////////////////////////////////////////////////////////////////////////////////

// Select the SPI port to use, ESP32 has 2 options
#if !defined (TFT_PARALLEL_8_BIT)
  #ifdef CONFIG_IDF_TARGET_ESP32
    #ifdef USE_HSPI_PORT
      SPIClass spi = SPIClass(HSPI);
    #elif defined(USE_FSPI_PORT)
      SPIClass spi = SPIClass(FSPI);
    #else // use default VSPI port
      SPIClass spi = SPIClass(VSPI);
    #endif
  #else
    #ifdef USE_HSPI_PORT
      SPIClass spi = SPIClass(HSPI);
    #elif defined(USE_FSPI_PORT)
      SPIClass spi = SPIClass(FSPI);
    #else // use FSPI port
      SPIClass& spi = SPI;
    #endif
  #endif
#endif

#ifdef ESP32_DMA
  // DMA SPA handle
  spi_device_handle_t dmaHAL;
  #ifdef CONFIG_IDF_TARGET_ESP32
    #define DMA_CHANNEL 1
    #ifdef USE_HSPI_PORT
      spi_host_device_t spi_host = HSPI_HOST;
    #elif defined(USE_FSPI_PORT)
      spi_host_device_t spi_host = SPI_HOST;
    #else // use VSPI port
      spi_host_device_t spi_host = VSPI_HOST;
    #endif
  #else
    #ifdef USE_HSPI_PORT
      #define DMA_CHANNEL 2
      spi_host_device_t spi_host = (spi_host_device_t) DMA_CHANNEL; // Draws once then freezes
    #else // use FSPI port
      #define DMA_CHANNEL 1
      spi_host_device_t spi_host = (spi_host_device_t) DMA_CHANNEL; // Draws once then freezes
    #endif
  #endif
#endif

#if !defined (TFT_PARALLEL_8_BIT)
  // Volatile for register reads:
  volatile uint32_t* _spi_cmd       = (volatile uint32_t*)(SPI_CMD_REG(SPI_PORT));
  volatile uint32_t* _spi_user      = (volatile uint32_t*)(SPI_USER_REG(SPI_PORT));
  // Register writes only:
  volatile uint32_t* _spi_mosi_dlen = (volatile uint32_t*)(SPI_MOSI_DLEN_REG(SPI_PORT));
  volatile uint32_t* _spi_w         = (volatile uint32_t*)(SPI_W0_REG(SPI_PORT));
#endif

////////////////////////////////////////////////////////////////////////////////////////
#if defined (TFT_SDA_READ) && !defined (TFT_PARALLEL_8_BIT)
////////////////////////////////////////////////////////////////////////////////////////

/***************************************************************************************
** Function name:           beginSDA - FPSI port only
** Description:             Detach MOSI and attach MISO to SDA for reads
***************************************************************************************/
void TFT_eSPI::begin_SDA_Read(void)
{
  gpio_set_direction((gpio_num_t)TFT_MOSI, GPIO_MODE_INPUT);
  pinMatrixInAttach(TFT_MOSI, FSPIQ_IN_IDX, false);
  SET_BUS_READ_MODE;
}

/***************************************************************************************
** Function name:           endSDA - FPSI port only
** Description:             Attach MOSI to SDA and detach MISO for writes
***************************************************************************************/
void TFT_eSPI::end_SDA_Read(void)
{
  gpio_set_direction((gpio_num_t)TFT_MOSI, GPIO_MODE_OUTPUT);
  pinMatrixOutAttach(TFT_MOSI, FSPID_OUT_IDX, false, false);
  SET_BUS_WRITE_MODE;
}
////////////////////////////////////////////////////////////////////////////////////////
#endif // #if defined (TFT_SDA_READ)
////////////////////////////////////////////////////////////////////////////////////////


/***************************************************************************************
** Function name:           read byte  - supports class functions
** Description:             Read a byte from ESP32 8-bit data port
***************************************************************************************/
// Parallel bus MUST be set to input before calling this function!
uint8_t TFT_eSPI::readByte(void)
{
  uint8_t b = 0xAA;

#if defined (TFT_PARALLEL_8_BIT)
  RD_L;
  uint32_t reg;           // Read all GPIO pins 0-31
  reg = gpio_input_get(); // Read three times to allow for bus access time
  reg = gpio_input_get();
  reg = gpio_input_get(); // Data should be stable now
  RD_H;

  // Check GPIO bits used and build value
  b  = (((reg>>TFT_D0)&1) << 0);
  b |= (((reg>>TFT_D1)&1) << 1);
  b |= (((reg>>TFT_D2)&1) << 2);
  b |= (((reg>>TFT_D3)&1) << 3);
  b |= (((reg>>TFT_D4)&1) << 4);
  b |= (((reg>>TFT_D5)&1) << 5);
  b |= (((reg>>TFT_D6)&1) << 6);
  b |= (((reg>>TFT_D7)&1) << 7);
#endif

  return b;
}

////////////////////////////////////////////////////////////////////////////////////////
#ifdef TFT_PARALLEL_8_BIT
////////////////////////////////////////////////////////////////////////////////////////

/***************************************************************************************
** Function name:           GPIO direction control  - supports class functions
** Description:             Set parallel bus to INPUT or OUTPUT
***************************************************************************************/
void TFT_eSPI::busDir(uint32_t mask, uint8_t mode)
{
  // Arduino generic native function
  pinMode(TFT_D0, mode);
  pinMode(TFT_D1, mode);
  pinMode(TFT_D2, mode);
  pinMode(TFT_D3, mode);
  pinMode(TFT_D4, mode);
  pinMode(TFT_D5, mode);
  pinMode(TFT_D6, mode);
  pinMode(TFT_D7, mode);
}

/***************************************************************************************
** Function name:           GPIO direction control  - supports class functions
** Description:             Set ESP32 GPIO pin to input or output (set high) ASAP
***************************************************************************************/
void TFT_eSPI::gpioMode(uint8_t gpio, uint8_t mode)
{
  pinMode(gpio, mode);
  digitalWrite(gpio, HIGH);
}
////////////////////////////////////////////////////////////////////////////////////////
#endif // #ifdef TFT_PARALLEL_8_BIT
////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////
#if defined (RPI_WRITE_STROBE) && !defined (TFT_PARALLEL_8_BIT) // Code for RPi TFT
////////////////////////////////////////////////////////////////////////////////////////

/***************************************************************************************
** Function name:           pushBlock - for ESP32 or ESP8266 RPi TFT
** Description:             Write a block of pixels of the same colour
***************************************************************************************/
void TFT_eSPI::pushBlock(uint16_t color, uint32_t len)
{
  uint8_t colorBin[] = { (uint8_t) (color >> 8), (uint8_t) color };
  if(len) spi.writePattern(&colorBin[0], 2, 1); len--;
  while(len--) {WR_L; WR_H;}
}

/***************************************************************************************
** Function name:           pushPixels - for ESP32 or ESP8266 RPi TFT
** Description:             Write a sequence of pixels
***************************************************************************************/
void TFT_eSPI::pushPixels(const void* data_in, uint32_t len)
{
  uint8_t *data = (uint8_t*)data_in;

  if(_swapBytes) {
      while ( len-- ) {tft_Write_16(*data); data++;}
      return;
  }

  while ( len >=64 ) {spi.writePattern(data, 64, 1); data += 64; len -= 64; }
  if (len) spi.writePattern(data, len, 1);
}

////////////////////////////////////////////////////////////////////////////////////////
#elif !defined (SPI_18BIT_DRIVER) && !defined (TFT_PARALLEL_8_BIT) // Most SPI displays
////////////////////////////////////////////////////////////////////////////////////////

/***************************************************************************************
** Function name:           pushBlock - for ESP32
** Description:             Write a block of pixels of the same colour
***************************************************************************************/
/*
void TFT_eSPI::pushBlock(uint16_t color, uint32_t len){

  uint32_t color32 = (color<<8 | color >>8)<<16 | (color<<8 | color >>8);
  bool empty = true;
  volatile uint32_t* spi_w = (volatile uint32_t*)_spi_w;
  if (len > 31)
  {
    *_spi_mosi_dlen =  511;
    spi_w[0]  = color32;
    spi_w[1]  = color32;
    spi_w[2]  = color32;
    spi_w[3]  = color32;
    spi_w[4]  = color32;
    spi_w[5]  = color32;
    spi_w[6]  = color32;
    spi_w[7]  = color32;
    spi_w[8]  = color32;
    spi_w[9]  = color32;
    spi_w[10] = color32;
    spi_w[11] = color32;
    spi_w[12] = color32;
    spi_w[13] = color32;
    spi_w[14] = color32;
    spi_w[15] = color32;
    while(len>31)
    {
      while ((*_spi_cmd)&SPI_USR);
      *_spi_cmd = SPI_USR;
      len -= 32;
    }
    empty = false;
  }

  if (len)
  {
    if(empty) {
      for (uint32_t i=0; i <= len; i+=2) *spi_w++ = color32;
    }
    len = (len << 4) - 1;
    while (*_spi_cmd&SPI_USR);
    *_spi_mosi_dlen = len;
    *_spi_cmd = SPI_USR;
  }
  while ((*_spi_cmd)&SPI_USR); // Move to later in code to use transmit time usefully?
}
//*/
//*
void TFT_eSPI::pushBlock(uint16_t color, uint32_t len){

  volatile uint32_t* spi_w = _spi_w;
  uint32_t color32 = (color<<8 | color >>8)<<16 | (color<<8 | color >>8);
  uint32_t i = 0;
  uint32_t rem = len & 0x1F;
  len =  len - rem;

  // Start with partial buffer pixels
  if (rem)
  {
    while (*_spi_cmd&SPI_USR);
    for (i=0; i < rem; i+=2) *spi_w++ = color32;
    *_spi_mosi_dlen = (rem << 4) - 1;
#if CONFIG_IDF_TARGET_ESP32C3
    *_spi_cmd = SPI_UPDATE;
    while (*_spi_cmd & SPI_UPDATE);
#endif
    *_spi_cmd = SPI_USR;
    if (!len) return; //{while (*_spi_cmd&SPI_USR); return; }
    i = i>>1; while(i++<16) *spi_w++ = color32;
  }

  while (*_spi_cmd&SPI_USR);
  if (!rem) while (i++<16) *spi_w++ = color32;
  *_spi_mosi_dlen =  511;

  // End with full buffer to maximise useful time for downstream code
  while(len)
  {
    while (*_spi_cmd&SPI_USR);
#if CONFIG_IDF_TARGET_ESP32C3
    *_spi_cmd = SPI_UPDATE;
    while (*_spi_cmd & SPI_UPDATE);
#endif
    *_spi_cmd = SPI_USR;
    len -= 32;
  }

  // Do not wait here
  //while (*_spi_cmd&SPI_USR);
}
//*/
/***************************************************************************************
** Function name:           pushSwapBytePixels - for ESP32
** Description:             Write a sequence of pixels with swapped bytes
***************************************************************************************/
void TFT_eSPI::pushSwapBytePixels(const void* data_in, uint32_t len){

  uint8_t* data = (uint8_t*)data_in;
  uint32_t color[16];

  if (len > 31)
  {
    WRITE_PERI_REG(SPI_MOSI_DLEN_REG(SPI_PORT), 511);
    while(len>31)
    {
      uint32_t i = 0;
      while(i<16)
      {
        color[i++] = DAT8TO32(data);
        data+=4;
      }
      while (READ_PERI_REG(SPI_CMD_REG(SPI_PORT))&SPI_USR);
      WRITE_PERI_REG(SPI_W0_REG(SPI_PORT),  color[0]);
      WRITE_PERI_REG(SPI_W1_REG(SPI_PORT),  color[1]);
      WRITE_PERI_REG(SPI_W2_REG(SPI_PORT),  color[2]);
      WRITE_PERI_REG(SPI_W3_REG(SPI_PORT),  color[3]);
      WRITE_PERI_REG(SPI_W4_REG(SPI_PORT),  color[4]);
      WRITE_PERI_REG(SPI_W5_REG(SPI_PORT),  color[5]);
      WRITE_PERI_REG(SPI_W6_REG(SPI_PORT),  color[6]);
      WRITE_PERI_REG(SPI_W7_REG(SPI_PORT),  color[7]);
      WRITE_PERI_REG(SPI_W8_REG(SPI_PORT),  color[8]);
      WRITE_PERI_REG(SPI_W9_REG(SPI_PORT),  color[9]);
      WRITE_PERI_REG(SPI_W10_REG(SPI_PORT), color[10]);
      WRITE_PERI_REG(SPI_W11_REG(SPI_PORT), color[11]);
      WRITE_PERI_REG(SPI_W12_REG(SPI_PORT), color[12]);
      WRITE_PERI_REG(SPI_W13_REG(SPI_PORT), color[13]);
      WRITE_PERI_REG(SPI_W14_REG(SPI_PORT), color[14]);
      WRITE_PERI_REG(SPI_W15_REG(SPI_PORT), color[15]);
#if CONFIG_IDF_TARGET_ESP32C3
      SET_PERI_REG_MASK(SPI_CMD_REG(SPI_PORT), SPI_UPDATE);
      while (READ_PERI_REG(SPI_CMD_REG(SPI_PORT))&SPI_UPDATE);
#endif
      SET_PERI_REG_MASK(SPI_CMD_REG(SPI_PORT), SPI_USR);
      len -= 32;
    }
  }

  if (len > 15)
  {
    uint32_t i = 0;
    while(i<8)
    {
      color[i++] = DAT8TO32(data);
      data+=4;
    }
    while (READ_PERI_REG(SPI_CMD_REG(SPI_PORT))&SPI_USR);
    WRITE_PERI_REG(SPI_MOSI_DLEN_REG(SPI_PORT), 255);
    WRITE_PERI_REG(SPI_W0_REG(SPI_PORT),  color[0]);
    WRITE_PERI_REG(SPI_W1_REG(SPI_PORT),  color[1]);
    WRITE_PERI_REG(SPI_W2_REG(SPI_PORT),  color[2]);
    WRITE_PERI_REG(SPI_W3_REG(SPI_PORT),  color[3]);
    WRITE_PERI_REG(SPI_W4_REG(SPI_PORT),  color[4]);
    WRITE_PERI_REG(SPI_W5_REG(SPI_PORT),  color[5]);
    WRITE_PERI_REG(SPI_W6_REG(SPI_PORT),  color[6]);
    WRITE_PERI_REG(SPI_W7_REG(SPI_PORT),  color[7]);
#if CONFIG_IDF_TARGET_ESP32C3
    SET_PERI_REG_MASK(SPI_CMD_REG(SPI_PORT), SPI_UPDATE);
    while (READ_PERI_REG(SPI_CMD_REG(SPI_PORT))&SPI_UPDATE);
#endif
    SET_PERI_REG_MASK(SPI_CMD_REG(SPI_PORT), SPI_USR);
    len -= 16;
  }

  if (len)
  {
    while (READ_PERI_REG(SPI_CMD_REG(SPI_PORT))&SPI_USR);
    WRITE_PERI_REG(SPI_MOSI_DLEN_REG(SPI_PORT), (len << 4) - 1);
    for (uint32_t i=0; i <= (len<<1); i+=4) {
      WRITE_PERI_REG(SPI_W0_REG(SPI_PORT)+i, DAT8TO32(data)); data+=4;
    }
#if CONFIG_IDF_TARGET_ESP32C3
    SET_PERI_REG_MASK(SPI_CMD_REG(SPI_PORT), SPI_UPDATE);
    while (READ_PERI_REG(SPI_CMD_REG(SPI_PORT))&SPI_UPDATE);
#endif
    SET_PERI_REG_MASK(SPI_CMD_REG(SPI_PORT), SPI_USR);
  }
  while (READ_PERI_REG(SPI_CMD_REG(SPI_PORT))&SPI_USR);

}

/***************************************************************************************
** Function name:           pushPixels - for ESP32
** Description:             Write a sequence of pixels
***************************************************************************************/
void TFT_eSPI::pushPixels(const void* data_in, uint32_t len){

  if(_swapBytes) {
    pushSwapBytePixels(data_in, len);
    return;
  }

  uint32_t *data = (uint32_t*)data_in;

  if (len > 31)
  {
    WRITE_PERI_REG(SPI_MOSI_DLEN_REG(SPI_PORT), 511);
    while(len>31)
    {
      while (READ_PERI_REG(SPI_CMD_REG(SPI_PORT))&SPI_USR);
      WRITE_PERI_REG(SPI_W0_REG(SPI_PORT),  *data++);
      WRITE_PERI_REG(SPI_W1_REG(SPI_PORT),  *data++);
      WRITE_PERI_REG(SPI_W2_REG(SPI_PORT),  *data++);
      WRITE_PERI_REG(SPI_W3_REG(SPI_PORT),  *data++);
      WRITE_PERI_REG(SPI_W4_REG(SPI_PORT),  *data++);
      WRITE_PERI_REG(SPI_W5_REG(SPI_PORT),  *data++);
      WRITE_PERI_REG(SPI_W6_REG(SPI_PORT),  *data++);
      WRITE_PERI_REG(SPI_W7_REG(SPI_PORT),  *data++);
      WRITE_PERI_REG(SPI_W8_REG(SPI_PORT),  *data++);
      WRITE_PERI_REG(SPI_W9_REG(SPI_PORT),  *data++);
      WRITE_PERI_REG(SPI_W10_REG(SPI_PORT), *data++);
      WRITE_PERI_REG(SPI_W11_REG(SPI_PORT), *data++);
      WRITE_PERI_REG(SPI_W12_REG(SPI_PORT), *data++);
      WRITE_PERI_REG(SPI_W13_REG(SPI_PORT), *data++);
      WRITE_PERI_REG(SPI_W14_REG(SPI_PORT), *data++);
      WRITE_PERI_REG(SPI_W15_REG(SPI_PORT), *data++);
#if CONFIG_IDF_TARGET_ESP32C3
      SET_PERI_REG_MASK(SPI_CMD_REG(SPI_PORT), SPI_UPDATE);
      while (READ_PERI_REG(SPI_CMD_REG(SPI_PORT))&SPI_UPDATE);
#endif
      SET_PERI_REG_MASK(SPI_CMD_REG(SPI_PORT), SPI_USR);
      len -= 32;
    }
  }

  if (len)
  {
    while (READ_PERI_REG(SPI_CMD_REG(SPI_PORT))&SPI_USR);
    WRITE_PERI_REG(SPI_MOSI_DLEN_REG(SPI_PORT), (len << 4) - 1);
    for (uint32_t i=0; i <= (len<<1); i+=4) WRITE_PERI_REG((SPI_W0_REG(SPI_PORT) + i), *data++);
#if CONFIG_IDF_TARGET_ESP32C3
      SET_PERI_REG_MASK(SPI_CMD_REG(SPI_PORT), SPI_UPDATE);
      while (READ_PERI_REG(SPI_CMD_REG(SPI_PORT))&SPI_UPDATE);
#endif
    SET_PERI_REG_MASK(SPI_CMD_REG(SPI_PORT), SPI_USR);
  }
  while (READ_PERI_REG(SPI_CMD_REG(SPI_PORT))&SPI_USR);
}

////////////////////////////////////////////////////////////////////////////////////////
#elif defined (SPI_18BIT_DRIVER) // SPI 18-bit colour
////////////////////////////////////////////////////////////////////////////////////////

/***************************************************************************************
** Function name:           pushBlock - for ESP32 and 3 byte RGB display
** Description:             Write a block of pixels of the same colour
***************************************************************************************/
void TFT_eSPI::pushBlock(uint16_t color, uint32_t len)
{
  // Split out the colours
  uint32_t r = (color & 0xF800)>>8;
  uint32_t g = (color & 0x07E0)<<5;
  uint32_t b = (color & 0x001F)<<19;
  // Concatenate 4 pixels into three 32-bit blocks
  uint32_t r0 = r<<24 | b | g | r;
  uint32_t r1 = r0>>8 | g<<16;
  uint32_t r2 = r1>>8 | b<<8;

  if (len > 19)
  {
    WRITE_PERI_REG(SPI_MOSI_DLEN_REG(SPI_PORT), 479);

    while(len>19)
    {
      while (READ_PERI_REG(SPI_CMD_REG(SPI_PORT))&SPI_USR);
      WRITE_PERI_REG(SPI_W0_REG(SPI_PORT), r0);
      WRITE_PERI_REG(SPI_W1_REG(SPI_PORT), r1);
      WRITE_PERI_REG(SPI_W2_REG(SPI_PORT), r2);
      WRITE_PERI_REG(SPI_W3_REG(SPI_PORT), r0);
      WRITE_PERI_REG(SPI_W4_REG(SPI_PORT), r1);
      WRITE_PERI_REG(SPI_W5_REG(SPI_PORT), r2);
      WRITE_PERI_REG(SPI_W6_REG(SPI_PORT), r0);
      WRITE_PERI_REG(SPI_W7_REG(SPI_PORT), r1);
      WRITE_PERI_REG(SPI_W8_REG(SPI_PORT), r2);
      WRITE_PERI_REG(SPI_W9_REG(SPI_PORT), r0);
      WRITE_PERI_REG(SPI_W10_REG(SPI_PORT), r1);
      WRITE_PERI_REG(SPI_W11_REG(SPI_PORT), r2);
      WRITE_PERI_REG(SPI_W12_REG(SPI_PORT), r0);
      WRITE_PERI_REG(SPI_W13_REG(SPI_PORT), r1);
      WRITE_PERI_REG(SPI_W14_REG(SPI_PORT), r2);
#if CONFIG_IDF_TARGET_ESP32C3
      SET_PERI_REG_MASK(SPI_CMD_REG(SPI_PORT), SPI_UPDATE);
      while (READ_PERI_REG(SPI_CMD_REG(SPI_PORT))&SPI_UPDATE);
#endif
      SET_PERI_REG_MASK(SPI_CMD_REG(SPI_PORT), SPI_USR);
      len -= 20;
    }
    while (READ_PERI_REG(SPI_CMD_REG(SPI_PORT))&SPI_USR);
  }

  if (len)
  {
    WRITE_PERI_REG(SPI_MOSI_DLEN_REG(SPI_PORT), (len * 24) - 1);
    WRITE_PERI_REG(SPI_W0_REG(SPI_PORT), r0);
    WRITE_PERI_REG(SPI_W1_REG(SPI_PORT), r1);
    WRITE_PERI_REG(SPI_W2_REG(SPI_PORT), r2);
    WRITE_PERI_REG(SPI_W3_REG(SPI_PORT), r0);
    WRITE_PERI_REG(SPI_W4_REG(SPI_PORT), r1);
    WRITE_PERI_REG(SPI_W5_REG(SPI_PORT), r2);
    if (len > 8 )
    {
      WRITE_PERI_REG(SPI_W6_REG(SPI_PORT), r0);
      WRITE_PERI_REG(SPI_W7_REG(SPI_PORT), r1);
      WRITE_PERI_REG(SPI_W8_REG(SPI_PORT), r2);
      WRITE_PERI_REG(SPI_W9_REG(SPI_PORT), r0);
      WRITE_PERI_REG(SPI_W10_REG(SPI_PORT), r1);
      WRITE_PERI_REG(SPI_W11_REG(SPI_PORT), r2);
      WRITE_PERI_REG(SPI_W12_REG(SPI_PORT), r0);
      WRITE_PERI_REG(SPI_W13_REG(SPI_PORT), r1);
      WRITE_PERI_REG(SPI_W14_REG(SPI_PORT), r2);
    }
#if CONFIG_IDF_TARGET_ESP32C3
    SET_PERI_REG_MASK(SPI_CMD_REG(SPI_PORT), SPI_UPDATE);
    while (READ_PERI_REG(SPI_CMD_REG(SPI_PORT))&SPI_UPDATE);
#endif
    SET_PERI_REG_MASK(SPI_CMD_REG(SPI_PORT), SPI_USR);
    while (READ_PERI_REG(SPI_CMD_REG(SPI_PORT))&SPI_USR);
  }
}

/***************************************************************************************
** Function name:           pushPixels - for ESP32 and 3 byte RGB display
** Description:             Write a sequence of pixels
***************************************************************************************/
void TFT_eSPI::pushPixels(const void* data_in, uint32_t len){

  uint16_t *data = (uint16_t*)data_in;
  // ILI9488 write macro is not endianess dependant, hence !_swapBytes
  if(!_swapBytes) { while ( len-- ) {tft_Write_16S(*data); data++;} }
  else { while ( len-- ) {tft_Write_16(*data); data++;} }
}

/***************************************************************************************
** Function name:           pushSwapBytePixels - for ESP32 and 3 byte RGB display
** Description:             Write a sequence of pixels with swapped bytes
***************************************************************************************/
void TFT_eSPI::pushSwapBytePixels(const void* data_in, uint32_t len){

  uint16_t *data = (uint16_t*)data_in;
  // ILI9488 write macro is not endianess dependant, so swap byte macro not used here
  while ( len-- ) {tft_Write_16(*data); data++;}
}

////////////////////////////////////////////////////////////////////////////////////////
#elif defined (TFT_PARALLEL_8_BIT) // Now the code for ESP32 8-bit parallel
////////////////////////////////////////////////////////////////////////////////////////

/***************************************************************************************
** Function name:           pushBlock - for ESP32 and parallel display
** Description:             Write a block of pixels of the same colour
***************************************************************************************/
void TFT_eSPI::pushBlock(uint16_t color, uint32_t len){
  if ( (color >> 8) == (color & 0x00FF) )
  { if (!len) return;
    tft_Write_16(color);
  #if defined (SSD1963_DRIVER)
    while (--len) {WR_L; WR_H; WR_L; WR_H; WR_L; WR_H;}
  #else
    #ifdef PSEUDO_16_BIT
      while (--len) {WR_L; WR_H;}
    #else
      while (--len) {WR_L; WR_H; WR_L; WR_H;}
    #endif
  #endif
  }
  else while (len--) {tft_Write_16(color);}
}

/***************************************************************************************
** Function name:           pushSwapBytePixels - for ESP32 and parallel display
** Description:             Write a sequence of pixels with swapped bytes
***************************************************************************************/
void TFT_eSPI::pushSwapBytePixels(const void* data_in, uint32_t len){

  uint16_t *data = (uint16_t*)data_in;
  while ( len-- ) {tft_Write_16(*data); data++;}
}

/***************************************************************************************
** Function name:           pushPixels - for ESP32 and parallel display
** Description:             Write a sequence of pixels
***************************************************************************************/
void TFT_eSPI::pushPixels(const void* data_in, uint32_t len){

  uint16_t *data = (uint16_t*)data_in;
  if(_swapBytes) { while ( len-- ) {tft_Write_16(*data); data++; } }
  else { while ( len-- ) {tft_Write_16S(*data); data++;} }
}

////////////////////////////////////////////////////////////////////////////////////////
#endif // End of display interface specific functions
////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////
#if defined (ESP32_DMA) && !defined (TFT_PARALLEL_8_BIT) //       DMA FUNCTIONS
////////////////////////////////////////////////////////////////////////////////////////

/***************************************************************************************
** Function name:           dmaBusy
** Description:             Check if DMA is busy
***************************************************************************************/
bool TFT_eSPI::dmaBusy(void)
{
  if (!DMA_Enabled || !spiBusyCheck) return false;

  spi_transaction_t *rtrans;
  esp_err_t ret;
  uint8_t checks = spiBusyCheck;
  for (int i = 0; i < checks; ++i)
  {
    ret = spi_device_get_trans_result(dmaHAL, &rtrans, 0);
    if (ret == ESP_OK) spiBusyCheck--;
  }

  //Serial.print("spiBusyCheck=");Serial.println(spiBusyCheck);
  if (spiBusyCheck ==0) return false;
  return true;
}


/***************************************************************************************
** Function name:           dmaWait
** Description:             Wait until DMA is over (blocking!)
***************************************************************************************/
void TFT_eSPI::dmaWait(void)
{
  if (!DMA_Enabled || !spiBusyCheck) return;
  spi_transaction_t *rtrans;
  esp_err_t ret;
  for (int i = 0; i < spiBusyCheck; ++i)
  {
    ret = spi_device_get_trans_result(dmaHAL, &rtrans, portMAX_DELAY);
    assert(ret == ESP_OK);
  }
  spiBusyCheck = 0;
}


/***************************************************************************************
** Function name:           pushPixelsDMA
** Description:             Push pixels to TFT (len must be less than 32767)
***************************************************************************************/
// This will byte swap the original image if setSwapBytes(true) was called by sketch.
void TFT_eSPI::pushPixelsDMA(uint16_t* image, uint32_t len)
{
  if ((len == 0) || (!DMA_Enabled)) return;

  dmaWait();

  if(_swapBytes) {
    for (uint32_t i = 0; i < len; i++) (image[i] = image[i] << 8 | image[i] >> 8);
  }

  esp_err_t ret;
  static spi_transaction_t trans;

  memset(&trans, 0, sizeof(spi_transaction_t));

  trans.user = (void *)1;
  trans.tx_buffer = image;  //finally send the line data
  trans.length = len * 16;        //Data length, in bits
  trans.flags = 0;                //SPI_TRANS_USE_TXDATA flag

  ret = spi_device_queue_trans(dmaHAL, &trans, portMAX_DELAY);
  assert(ret == ESP_OK);

  spiBusyCheck++;
}


/***************************************************************************************
** Function name:           pushImageDMA
** Description:             Push image to a window (w*h must be less than 65536)
***************************************************************************************/
// Fixed const data assumed, will NOT clip or swap bytes
void TFT_eSPI::pushImageDMA(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t const* image)
{
  if ((w == 0) || (h == 0) || (!DMA_Enabled)) return;

  uint32_t len = w*h;

  dmaWait();

  setAddrWindow(x, y, w, h);

  esp_err_t ret;
  static spi_transaction_t trans;

  memset(&trans, 0, sizeof(spi_transaction_t));

  trans.user = (void *)1;
  trans.tx_buffer = image;   //Data pointer
  trans.length = len * 16;   //Data length, in bits
  trans.flags = 0;           //SPI_TRANS_USE_TXDATA flag

  ret = spi_device_queue_trans(dmaHAL, &trans, portMAX_DELAY);
  assert(ret == ESP_OK);

  spiBusyCheck++;
}


/***************************************************************************************
** Function name:           pushImageDMA
** Description:             Push image to a window (w*h must be less than 65536)
***************************************************************************************/
// This will clip and also swap bytes if setSwapBytes(true) was called by sketch
void TFT_eSPI::pushImageDMA(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t* image, uint16_t* buffer)
{
  if ((x >= _vpW) || (y >= _vpH) || (!DMA_Enabled)) return;

  int32_t dx = 0;
  int32_t dy = 0;
  int32_t dw = w;
  int32_t dh = h;

  if (x < _vpX) { dx = _vpX - x; dw -= dx; x = _vpX; }
  if (y < _vpY) { dy = _vpY - y; dh -= dy; y = _vpY; }

  if ((x + dw) > _vpW ) dw = _vpW - x;
  if ((y + dh) > _vpH ) dh = _vpH - y;

  if (dw < 1 || dh < 1) return;

  uint32_t len = dw*dh;

  if (buffer == nullptr) {
    buffer = image;
    dmaWait();
  }

  // If image is clipped, copy pixels into a contiguous block
  if ( (dw != w) || (dh != h) ) {
    if(_swapBytes) {
      for (int32_t yb = 0; yb < dh; yb++) {
        for (int32_t xb = 0; xb < dw; xb++) {
          uint32_t src = xb + dx + w * (yb + dy);
          (buffer[xb + yb * dw] = image[src] << 8 | image[src] >> 8);
        }
      }
    }
    else {
      for (int32_t yb = 0; yb < dh; yb++) {
        memcpy((uint8_t*) (buffer + yb * dw), (uint8_t*) (image + dx + w * (yb + dy)), dw << 1);
      }
    }
  }
  // else, if a buffer pointer has been provided copy whole image to the buffer
  else if (buffer != image || _swapBytes) {
    if(_swapBytes) {
      for (uint32_t i = 0; i < len; i++) (buffer[i] = image[i] << 8 | image[i] >> 8);
    }
    else {
      memcpy(buffer, image, len*2);
    }
  }

  if (spiBusyCheck) dmaWait(); // In case we did not wait earlier

  setAddrWindow(x, y, dw, dh);

  esp_err_t ret;
  static spi_transaction_t trans;

  memset(&trans, 0, sizeof(spi_transaction_t));

  trans.user = (void *)1;
  trans.tx_buffer = buffer;  //finally send the line data
  trans.length = len * 16;   //Data length, in bits
  trans.flags = 0;           //SPI_TRANS_USE_TXDATA flag

  ret = spi_device_queue_trans(dmaHAL, &trans, portMAX_DELAY);
  assert(ret == ESP_OK);

  spiBusyCheck++;
}

////////////////////////////////////////////////////////////////////////////////////////
// Processor specific DMA initialisation
////////////////////////////////////////////////////////////////////////////////////////

// The DMA functions here work with SPI only (not parallel)
/***************************************************************************************
** Function name:           dc_callback
** Description:             Toggles DC line during transaction
***************************************************************************************/
extern "C" void dc_callback();

void IRAM_ATTR dc_callback(spi_transaction_t *spi_tx)
{
  if ((bool)spi_tx->user) {DC_D;}
  else {DC_C;}
}

/***************************************************************************************
** Function name:           initDMA
** Description:             Initialise the DMA engine - returns true if init OK
***************************************************************************************/
bool TFT_eSPI::initDMA(bool ctrl_cs)
{
  if (DMA_Enabled) return false;

  esp_err_t ret;
  spi_bus_config_t buscfg = {
    .mosi_io_num = TFT_MOSI,
    .miso_io_num = TFT_MISO,
    .sclk_io_num = TFT_SCLK,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .data4_io_num = -1,
    .data5_io_num = -1,
    .data6_io_num = -1,
    .data7_io_num = -1,
    .max_transfer_sz = TFT_WIDTH * TFT_HEIGHT * 2 + 8, // TFT screen size
    .flags = 0,
    .intr_flags = 0
  };

  int8_t pin = -1;
  if (ctrl_cs) pin = TFT_CS;

  spi_device_interface_config_t devcfg = {
    .command_bits = 0,
    .address_bits = 0,
    .dummy_bits = 0,
    .mode = TFT_SPI_MODE,
    .duty_cycle_pos = 0,
    .cs_ena_pretrans = 0,
    .cs_ena_posttrans = 0,
    .clock_speed_hz = SPI_FREQUENCY,
    .input_delay_ns = 0,
    .spics_io_num = pin,
    .flags = SPI_DEVICE_NO_DUMMY, //0,
    .queue_size = 1,
    .pre_cb = 0, //dc_callback, //Callback to handle D/C line
    .post_cb = 0
  };
  ret = spi_bus_initialize(spi_host, &buscfg, DMA_CHANNEL);
  ESP_ERROR_CHECK(ret);
  ret = spi_bus_add_device(spi_host, &devcfg, &dmaHAL);
  ESP_ERROR_CHECK(ret);

  DMA_Enabled = true;
  spiBusyCheck = 0;
  return true;
}

/***************************************************************************************
** Function name:           deInitDMA
** Description:             Disconnect the DMA engine from SPI
***************************************************************************************/
void TFT_eSPI::deInitDMA(void)
{
  if (!DMA_Enabled) return;
  spi_bus_remove_device(dmaHAL);
  spi_bus_free(spi_host);
  DMA_Enabled = false;
}

////////////////////////////////////////////////////////////////////////////////////////
#endif // End of DMA FUNCTIONS
////////////////////////////////////////////////////////////////////////////////////////
