
        //////////////////////////////////////////////////////
        // TFT_eSPI driver functions for ESP8266 processors //
        //////////////////////////////////////////////////////

// Select the SPI port to use
// ESP8266 default (FLASH port also available via overlap mode)
  SPIClass& spi = SPI;

// Buffer for SPI transmit byte padding and byte order manipulation
uint8_t   spiBuffer[8] = {0,0,0,0,0,0,0,0};

////////////////////////////////////////////////////////////////////////////////////////
#if defined (TFT_SDA_READ) && !defined (TFT_PARALLEL_8_BIT)
////////////////////////////////////////////////////////////////////////////////////////

/***************************************************************************************
** Function name:           tft_Read_8
** Description:             ESP8266 software SPI to read bidirectional SDA line
***************************************************************************************/
uint8_t TFT_eSPI::tft_Read_8(void)
{
  uint8_t  ret = 0;
  uint32_t reg = 0;

  for (uint8_t i = 0; i < 8; i++) {  // read results
    ret <<= 1;
    SCLK_L;
    if (digitalRead(TFT_MOSI)) ret |= 1;
    SCLK_H;
  }

  return ret;
}

/***************************************************************************************
** Function name:           beginSDA
** Description:             Detach SPI from pin to permit software SPI
***************************************************************************************/
void TFT_eSPI::begin_SDA_Read(void)
{
  #ifdef TFT_SPI_OVERLAP
    // Reads in overlap mode not supported
  #else
    spi.end();
  #endif
}

/***************************************************************************************
** Function name:           endSDA
** Description:             Attach SPI pins after software SPI
***************************************************************************************/
void TFT_eSPI::end_SDA_Read(void)
{
  #ifdef TFT_SPI_OVERLAP
    spi.pins(6, 7, 8, 0);
  #else
    spi.begin();
  #endif
}
////////////////////////////////////////////////////////////////////////////////////////
#endif // #if defined (TFT_SDA_READ)
////////////////////////////////////////////////////////////////////////////////////////


/***************************************************************************************
** Function name:           read byte  - supports class functions
** Description:             Parallel bus only - dummy function - not used
***************************************************************************************/
uint8_t TFT_eSPI::readByte(void)
{
  uint8_t b = 0xAA;
  return b;
}

////////////////////////////////////////////////////////////////////////////////////////
#if defined (RPI_WRITE_STROBE)
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
void TFT_eSPI::pushPixels(const void* data_in, uint32_t len){

  uint8_t *data = (uint8_t*)data_in;
  while ( len >=64 ) {spi.writePattern(data, 64, 1); data += 64; len -= 64; }
  if (len) spi.writePattern(data, len, 1);
}

/***************************************************************************************
** Function name:           pushSwapBytePixels - for ESP32 or ESP8266 RPi TFT
** Description:             Write a sequence of pixels with swapped bytes
***************************************************************************************/
void TFT_eSPI::pushSwapBytePixels(const void* data_in, uint32_t len){
  uint16_t *data = (uint16_t*)data_in;
  while ( len-- ) {tft_Write_16(*data); data++;}
}

////////////////////////////////////////////////////////////////////////////////////////
#elif defined (SPI_18BIT_DRIVER) // SPI 18-bit colour
////////////////////////////////////////////////////////////////////////////////////////

/***************************************************************************************
** Function name:           pushBlock - for ESP8266 and 3 byte RGB display
** Description:             Write a block of pixels of the same colour
***************************************************************************************/
void TFT_eSPI::pushBlock(uint16_t color, uint32_t len)
{
  // Split out the colours
  uint8_t r = (color & 0xF800)>>8;
  uint8_t g = (color & 0x07E0)>>3;
  uint8_t b = (color & 0x001F)<<3;
  // Concatenate 4 pixels into three 32-bit blocks
  uint32_t r0 = r<<24 | b<<16 | g<<8 | r;
  uint32_t r1 = g<<24 | r<<16 | b<<8 | g;
  uint32_t r2 = b<<24 | g<<16 | r<<8 | b;

  SPI1W0 = r0;
  SPI1W1 = r1;
  SPI1W2 = r2;

  if (len > 4)
  {
    SPI1W3 = r0;
    SPI1W4 = r1;
    SPI1W5 = r2;
  }
  if (len > 8)
  {
    SPI1W6 = r0;
    SPI1W7 = r1;
    SPI1W8 = r2;
  }
  if (len > 12)
  {
    SPI1W9  = r0;
    SPI1W10 = r1;
    SPI1W11 = r2;
    SPI1W12 = r0;
    SPI1W13 = r1;
    SPI1W14 = r2;
    SPI1W15 = r0;
  }

  if (len > 20)
  {
    SPI1U1 = (503 << SPILMOSI);
    while(len>20)
    {
      while(SPI1CMD & SPIBUSY) {}
      SPI1CMD |= SPIBUSY;
      len -= 21;
    }
    while(SPI1CMD & SPIBUSY) {}
  }

  if (len)
  {
    len = (len * 24) - 1;
    SPI1U1 = (len << SPILMOSI);
    SPI1CMD |= SPIBUSY;
    while(SPI1CMD & SPIBUSY) {}
  }

}

/***************************************************************************************
** Function name:           pushPixels - for ESP8266 and 3 byte RGB display
** Description:             Write a sequence of pixels
***************************************************************************************/
void TFT_eSPI::pushPixels(const void* data_in, uint32_t len){

  uint16_t *data = (uint16_t*)data_in;

  // Send groups of 4 concatenated pixels
  if (len > 3) {
    SPI1U1 = ((4 * 24 - 1) << SPILMOSI);
    while (len > 3) {

      uint8_t  r[4];
      uint8_t  g[4];
      uint8_t  b[4];

      if (!_swapBytes) {
        // Split out the colours
        for (uint16_t i = 0; i < 4; i++) {
          uint16_t col = *data++;
          r[i] = (col & 0xF8);
          g[i] = (col & 0xE000)>>11 | (col & 0x07)<<5;
          b[i] = (col & 0x1F00)>>5;
        }
      }
      else {
        for (uint16_t i = 0; i < 4; i++) {
          uint16_t col = *data++;
          r[i] = (col & 0xF800)>>8;
          g[i] = (col & 0x07E0)>>3;
          b[i] = (col & 0x001F)<<3;
        }
      }
      uint32_t r0 = r[1]<<24 | b[0]<<16 | g[0]<<8 | r[0];
      uint32_t r1 = g[2]<<24 | r[2]<<16 | b[1]<<8 | g[1];
      uint32_t r2 = b[3]<<24 | g[3]<<16 | r[3]<<8 | b[2];

      while(SPI1CMD & SPIBUSY) {}
      SPI1W0 = r0;
      SPI1W1 = r1;
      SPI1W2 = r2;

      SPI1CMD |= SPIBUSY;
      len -= 4;
    }
    while(SPI1CMD & SPIBUSY) {}
  }

  // ILI9488 write macro is not endianess dependant, hence !_swapBytes
  if (!_swapBytes) while ( len-- ) { tft_Write_16S(*data); data++;}
  else while ( len-- ) {tft_Write_16(*data); data++;}
}

/***************************************************************************************
** Function name:           pushSwapBytePixels - for ESP8266 and 3 byte RGB display
** Description:             Write a sequence of pixels with swapped bytes
***************************************************************************************/
void TFT_eSPI::pushSwapBytePixels(const void* data_in, uint32_t len){

  uint16_t *data = (uint16_t*)data_in;
  // ILI9488 write macro is not endianess dependant, so swap byte macro not used here
  while ( len-- ) {tft_Write_16(*data); data++;}
}

////////////////////////////////////////////////////////////////////////////////////////
#else
////////////////////////////////////////////////////////////////////////////////////////

/***************************************************************************************
** Function name:           pushBlock - for ESP8266
** Description:             Write a block of pixels of the same colour
***************************************************************************************/
//Clear screen test 76.8ms theoretical. 81.5ms TFT_eSPI, 967ms Adafruit_ILI9341
//Performance 26.15Mbps@26.66MHz, 39.04Mbps@40MHz, 75.4Mbps@80MHz SPI clock
//Efficiency:
//       TFT_eSPI       98.06%              97.59%          94.24%
//       Adafruit_GFX   19.62%              14.31%           7.94%
//
void TFT_eSPI::pushBlock(uint16_t color, uint32_t len)
{
/*
while (len>1) { tft_Write_32(color<<16 | color); len-=2;}
if (len) tft_Write_16(color);
return;
//*/
  uint16_t color16 = (color >> 8) | (color << 8);
  uint32_t color32 = color16 | color16 << 16;
/*
  while(len--) {
    SPI1U1 = ((16-1) << SPILMOSI) | ((16-1) << SPILMISO);
    SPI1W0 = color16;
    SPI1CMD |= SPIBUSY;
    while(SPI1CMD & SPIBUSY) {}
  }
  return;
//*/

  SPI1W0 = color32;
  SPI1W1 = color32;
  SPI1W2 = color32;
  SPI1W3 = color32;
  if (len > 8)
  {
    SPI1W4 = color32;
    SPI1W5 = color32;
    SPI1W6 = color32;
    SPI1W7 = color32;
  }
  if (len > 16)
  {
    SPI1W8 = color32;
    SPI1W9 = color32;
    SPI1W10 = color32;
    SPI1W11 = color32;
  }
  if (len > 24)
  {
    SPI1W12 = color32;
    SPI1W13 = color32;
    SPI1W14 = color32;
    SPI1W15 = color32;
  }
  if (len > 31)
  {
    SPI1U1 = (511 << SPILMOSI);
    while(len>31)
    {
#if (defined (SPI_FREQUENCY) && (SPI_FREQUENCY == 80000000))
      if(SPI1CMD & SPIBUSY) // added to sync with flag change
#endif
      while(SPI1CMD & SPIBUSY) {}
      SPI1CMD |= SPIBUSY;
      len -= 32;
    }
    while(SPI1CMD & SPIBUSY) {}
  }

  if (len)
  {
    len = (len << 4) - 1;
    SPI1U1 = (len << SPILMOSI);
    SPI1CMD |= SPIBUSY;
    while(SPI1CMD & SPIBUSY) {}
  }

}

/***************************************************************************************
** Function name:           pushPixels - for ESP8266
** Description:             Write a sequence of pixels
***************************************************************************************/
void TFT_eSPI::pushPixels(const void* data_in, uint32_t len){

  if(_swapBytes) {
    pushSwapBytePixels(data_in, len);
    return;
  }

  uint16_t *data = (uint16_t*) data_in;

  uint32_t color[8];

  SPI1U1 = (255 << SPILMOSI) | (255 << SPILMISO);


  while(len>15)
  {
    memcpy(color,data,32);
    data+=16;

    len -= 16;

    // ESP8266 wait time here at 40MHz SPI is ~5.45us
    while(SPI1CMD & SPIBUSY) {}
    SPI1W0 = color[0];
    SPI1W1 = color[1];
    SPI1W2 = color[2];
    SPI1W3 = color[3];
    SPI1W4 = color[4];
    SPI1W5 = color[5];
    SPI1W6 = color[6];
    SPI1W7 = color[7];
    SPI1CMD |= SPIBUSY;
  }

  if(len)
  {
    uint32_t bits = (len*16-1); // bits left to shift - 1

    memcpy(color,data,len<<1);

    while(SPI1CMD & SPIBUSY) {}
    SPI1U1 = (bits << SPILMOSI) | (bits << SPILMISO);
    SPI1W0 = color[0];
    SPI1W1 = color[1];
    SPI1W2 = color[2];
    SPI1W3 = color[3];
    SPI1W4 = color[4];
    SPI1W5 = color[5];
    SPI1W6 = color[6];
    SPI1W7 = color[7];
    SPI1CMD |= SPIBUSY;
  }

  while(SPI1CMD & SPIBUSY) {}

}

/***************************************************************************************
** Function name:           pushSwapBytePixels - for ESP8266
** Description:             Write a sequence of pixels with swapped bytes
***************************************************************************************/
void TFT_eSPI::pushSwapBytePixels(const void* data_in, uint32_t len){

  uint8_t* data = (uint8_t*)data_in;
  //uint16_t* data = (uint16_t*)data_in;

  uint32_t color[8];

  SPI1U1 = (255 << SPILMOSI) | (255 << SPILMISO);

  while(len>15)
  {
    uint32_t i = 0;
    while(i<8) { color[i++] = DAT8TO32(data); data+=4; }

    len -= 16;

    // ESP8266 wait time here at 40MHz SPI is ~5.45us
    while(SPI1CMD & SPIBUSY) {}
    SPI1W0 = color[0];
    SPI1W1 = color[1];
    SPI1W2 = color[2];
    SPI1W3 = color[3];
    SPI1W4 = color[4];
    SPI1W5 = color[5];
    SPI1W6 = color[6];
    SPI1W7 = color[7];
    SPI1CMD |= SPIBUSY;
  }

  if(len)
  {
    uint32_t i = 0;
    uint32_t bits = (len*16-1); // bits left to shift - 1
    len = (len+1)>>1;
    while(len--) { color[i++] = DAT8TO32(data); data+=4; }

    while(SPI1CMD & SPIBUSY) {}
    SPI1U1 = (bits << SPILMOSI) | (bits << SPILMISO);
    SPI1W0 = color[0];
    SPI1W1 = color[1];
    SPI1W2 = color[2];
    SPI1W3 = color[3];
    SPI1W4 = color[4];
    SPI1W5 = color[5];
    SPI1W6 = color[6];
    SPI1W7 = color[7];
    SPI1CMD |= SPIBUSY;
  }

  while(SPI1CMD & SPIBUSY) {}

}

////////////////////////////////////////////////////////////////////////////////////////
#endif
////////////////////////////////////////////////////////////////////////////////////////
