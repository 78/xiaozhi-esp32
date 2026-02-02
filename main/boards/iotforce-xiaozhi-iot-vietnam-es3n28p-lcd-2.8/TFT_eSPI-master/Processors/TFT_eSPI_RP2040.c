        ////////////////////////////////////////////////////
        //       TFT_eSPI generic driver functions        //
        ////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////
// Global variables
////////////////////////////////////////////////////////////////////////////////////////

#if !defined (RP2040_PIO_INTERFACE) // SPI

  // Select the SPI port and board package to use
  #ifdef ARDUINO_ARCH_MBED
    // Arduino RP2040 board package
    MbedSPI spi = MbedSPI(TFT_MISO, TFT_MOSI, TFT_SCLK);
  #else
    // Community RP2040 board package by Earle Philhower
    //SPIClass& spi = SPI; // will use board package default pins
    SPIClassRP2040 spi = SPIClassRP2040(SPI_X, TFT_MISO, -1, TFT_SCLK, TFT_MOSI);
  #endif

#else // PIO interface used (8-bit parallel or SPI)

  #ifdef RP2040_PIO_SPI
    #if  defined (SPI_18BIT_DRIVER)
      // SPI PIO code for 18-bit colour transmit
      #include "pio_SPI_18bit.pio.h"
    #else
      // SPI PIO code for 16-bit colour transmit
      #include "pio_SPI.pio.h"
    #endif
  #elif defined (TFT_PARALLEL_8_BIT)
    #if defined (SSD1963_DRIVER)
      // PIO code for 8-bit parallel interface (18-bit colour)
      #include "pio_8bit_parallel_18bpp.pio.h"
    #else
      // PIO code for 8-bit parallel interface (16-bit colour)
      #include "pio_8bit_parallel.pio.h"
    #endif
  #else // must be TFT_PARALLEL_16_BIT
    // PIO code for 16-bit parallel interface (16-bit colour)
    #include "pio_16bit_parallel.pio.h"
  #endif

  // Board package specific differences
  #ifdef ARDUINO_ARCH_MBED
    // Not supported at the moment
    #error The Arduino RP2040 MBED board package is not supported when PIO is used. Use the community package by Earle Philhower.
  #endif

  // Community RP2040 board package by Earle Philhower
  PIO tft_pio = pio0;     // Code will try both pio's to find a free SM
  int8_t pio_sm = 0;  // pioinit will claim a free one
  // Updated later with the loading offset of the PIO program.
  uint32_t program_offset  = 0;

  // SM stalled mask
  uint32_t pull_stall_mask = 0;

  // SM jump instructions to change SM behaviour
  uint32_t pio_instr_jmp8  = 0;
  uint32_t pio_instr_fill  = 0;
  uint32_t pio_instr_addr  = 0;

  // SM "set" instructions to control DC control signal
  uint32_t pio_instr_set_dc = 0;
  uint32_t pio_instr_clr_dc = 0;

#endif

#ifdef RP2040_DMA
  int32_t            dma_tx_channel;
  dma_channel_config dma_tx_config;
#endif

////////////////////////////////////////////////////////////////////////////////////////
#if defined (TFT_SDA_READ) && !defined (RP2040_PIO_INTERFACE)
////////////////////////////////////////////////////////////////////////////////////////

/***************************************************************************************
** Function name:           tft_Read_8
** Description:             Bit bashed SPI to read bidirectional SDA line
***************************************************************************************/
uint8_t TFT_eSPI::tft_Read_8(void)
{
  uint8_t  ret = 0;

  /*
  for (uint8_t i = 0; i < 8; i++) {  // read results
    ret <<= 1;
    SCLK_L;
    if (digitalRead(TFT_MOSI)) ret |= 1;
    SCLK_H;
  }
  */
  
  ret = spi.transfer(0x00);

  return ret;
}

/***************************************************************************************
** Function name:           beginSDA
** Description:             Detach SPI from pin to permit software SPI
***************************************************************************************/
void TFT_eSPI::begin_SDA_Read(void)
{
  // Release configured SPI port for SDA read
  spi.end();
}

/***************************************************************************************
** Function name:           endSDA
** Description:             Attach SPI pins after software SPI
***************************************************************************************/
void TFT_eSPI::end_SDA_Read(void)
{
  // Configure SPI port ready for next TFT access
  spi.begin();
}

////////////////////////////////////////////////////////////////////////////////////////
#endif // #if defined (TFT_SDA_READ)
////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////
#if defined (RP2040_PIO_INTERFACE)
////////////////////////////////////////////////////////////////////////////////////////
#ifdef RP2040_PIO_SPI
void pioinit(uint32_t clock_freq) {

  // Find enough free space on one of the PIO's
  tft_pio = pio0;
  if (!pio_can_add_program(tft_pio, &tft_io_program)) {
    tft_pio = pio1;
    if (!pio_can_add_program(tft_pio, &tft_io_program)) {
      // Serial.println("No room for PIO program!");
      return;
    }
  }

  pio_sm = pio_claim_unused_sm(tft_pio, false);

  // Load the PIO program
  program_offset = pio_add_program(tft_pio, &tft_io_program);

  // Associate pins with the PIO
  pio_gpio_init(tft_pio, TFT_DC);
  pio_gpio_init(tft_pio, TFT_SCLK);
  pio_gpio_init(tft_pio, TFT_MOSI);

  // Configure the pins to be outputs
  pio_sm_set_consecutive_pindirs(tft_pio, pio_sm, TFT_DC, 1, true);
  pio_sm_set_consecutive_pindirs(tft_pio, pio_sm, TFT_SCLK, 1, true);
  pio_sm_set_consecutive_pindirs(tft_pio, pio_sm, TFT_MOSI, 1, true);

  // Configure the state machine
  pio_sm_config c = tft_io_program_get_default_config(program_offset);

  sm_config_set_set_pins(&c, TFT_DC, 1);
  // Define the single side-set pin
  sm_config_set_sideset_pins(&c, TFT_SCLK);
  // Define the pin used for data output
  sm_config_set_out_pins(&c, TFT_MOSI, 1);
  // Set clock divider, frequency is set up to 2% faster than specified, or next division down
  uint16_t clock_div = 0.98 + clock_get_hz(clk_sys) / (clock_freq * 2.0); // 2 cycles per bit
  sm_config_set_clkdiv(&c, clock_div);
  // Make a single 8 words FIFO from the 4 words TX and RX FIFOs
  sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
  // The OSR register shifts to the left, sm designed to send MS byte of a colour first, autopull off
  sm_config_set_out_shift(&c, false, false, 0);
  // Now load the configuration
  pio_sm_init(tft_pio, pio_sm, program_offset + tft_io_offset_start_tx, &c);

  // Start the state machine.
  pio_sm_set_enabled(tft_pio, pio_sm, true);

  // Create the pull stall bit mask
  pull_stall_mask = 1u << (PIO_FDEBUG_TXSTALL_LSB + pio_sm);

  // Create the assembler instruction for the jump to byte send routine
  pio_instr_jmp8  = pio_encode_jmp(program_offset + tft_io_offset_start_8);
  pio_instr_fill  = pio_encode_jmp(program_offset + tft_io_offset_block_fill);
  pio_instr_addr  = pio_encode_jmp(program_offset + tft_io_offset_set_addr_window);

  pio_instr_set_dc = pio_encode_set((pio_src_dest)0, 1);
  pio_instr_clr_dc = pio_encode_set((pio_src_dest)0, 0);
}
#else // 8 or 16-bit parallel
void pioinit(uint16_t clock_div, uint16_t fract_div) {

  // Find enough free space on one of the PIO's
  tft_pio = pio0;
  if (!pio_can_add_program(tft_pio, &tft_io_program)) {
    tft_pio = pio1;
    if (!pio_can_add_program(tft_pio, &tft_io_program)) {
      // Serial.println("No room for PIO program!");
      return;
    }
  }

  pio_sm = pio_claim_unused_sm(tft_pio, false);

  #if defined (TFT_PARALLEL_8_BIT)
    uint8_t bits = 8;
  #else // must be TFT_PARALLEL_16_BIT
    uint8_t bits = 16;
  #endif
  
  // Load the PIO program
  program_offset = pio_add_program(tft_pio, &tft_io_program);

  // Associate pins with the PIO
  pio_gpio_init(tft_pio, TFT_DC);
  pio_gpio_init(tft_pio, TFT_WR);

  for (int i = 0; i < bits; i++) {
    pio_gpio_init(tft_pio, TFT_D0 + i);
  }

  // Configure the pins to be outputs
  pio_sm_set_consecutive_pindirs(tft_pio, pio_sm, TFT_DC, 1, true);
  pio_sm_set_consecutive_pindirs(tft_pio, pio_sm, TFT_WR, 1, true);
  pio_sm_set_consecutive_pindirs(tft_pio, pio_sm, TFT_D0, bits, true);

  // Configure the state machine
  pio_sm_config c = tft_io_program_get_default_config(program_offset);
  // Define the set pin
  sm_config_set_set_pins(&c, TFT_DC, 1);
  // Define the single side-set pin
  sm_config_set_sideset_pins(&c, TFT_WR);
  // Define the consecutive pins that are used for data output
  sm_config_set_out_pins(&c, TFT_D0, bits);
  // Set clock divider and fractional divider
  sm_config_set_clkdiv_int_frac(&c, clock_div, fract_div);
  // Make a single 8 words FIFO from the 4 words TX and RX FIFOs
  sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
  // The OSR register shifts to the left, sm designed to send MS byte of a colour first
  sm_config_set_out_shift(&c, false, false, 0);
  // Now load the configuration
  pio_sm_init(tft_pio, pio_sm, program_offset + tft_io_offset_start_tx, &c);

  // Start the state machine.
  pio_sm_set_enabled(tft_pio, pio_sm, true);

  // Create the pull stall bit mask
  pull_stall_mask = 1u << (PIO_FDEBUG_TXSTALL_LSB + pio_sm);

  // Create the instructions for the jumps to send routines
  pio_instr_jmp8  = pio_encode_jmp(program_offset + tft_io_offset_start_8);
  pio_instr_fill  = pio_encode_jmp(program_offset + tft_io_offset_block_fill);
  pio_instr_addr  = pio_encode_jmp(program_offset + tft_io_offset_set_addr_window);

  // Create the instructions to set and clear the DC signal
  pio_instr_set_dc = pio_encode_set((pio_src_dest)0, 1);
  pio_instr_clr_dc = pio_encode_set((pio_src_dest)0, 0);
}
#endif

/***************************************************************************************
** Function name:           pushBlock - for generic processor and parallel display
** Description:             Write a block of pixels of the same colour
***************************************************************************************/
#ifdef RP2040_PIO_PUSHBLOCK
// PIO handles pixel block fill writes
void TFT_eSPI::pushBlock(uint16_t color, uint32_t len)
{
#if  defined (SPI_18BIT_DRIVER) || (defined (SSD1963_DRIVER) && defined (TFT_PARALLEL_8_BIT))
  uint32_t col = ((color & 0xF800)<<8) | ((color & 0x07E0)<<5) | ((color & 0x001F)<<3);
  if (len) {
    WAIT_FOR_STALL;
    tft_pio->sm[pio_sm].instr = pio_instr_fill;

    TX_FIFO = col;
    TX_FIFO = --len; // Decrement first as PIO sends n+1
  }
#else
  if (len) {
    WAIT_FOR_STALL;
    tft_pio->sm[pio_sm].instr = pio_instr_fill;

    TX_FIFO = color;
    TX_FIFO = --len; // Decrement first as PIO sends n+1
  }
#endif
}

#else
void TFT_eSPI::pushBlock(uint16_t color, uint32_t len){

  while (len > 4) {
    // 5 seems to be the optimum for maximum transfer rate
    WAIT_FOR_FIFO_FREE(5);
    TX_FIFO = color;
    TX_FIFO = color;
    TX_FIFO = color;
    TX_FIFO = color;
    TX_FIFO = color;

    len -= 5;
  }

  if (len) {
    // There could be a maximum of 4 words left  to send
    WAIT_FOR_FIFO_FREE(4);
    while (len--) TX_FIFO = color;
  }
}
#endif

/***************************************************************************************
** Function name:           pushPixels - for generic processor and parallel display
** Description:             Write a sequence of pixels
***************************************************************************************/
void TFT_eSPI::pushPixels(const void* data_in, uint32_t len){
#if  defined (SPI_18BIT_DRIVER) || (defined (SSD1963_DRIVER) && defined (TFT_PARALLEL_8_BIT))
  uint16_t *data = (uint16_t*)data_in;
  if (_swapBytes) {
    while ( len-- ) {
      uint32_t col = *data++;
      tft_Write_16(col);
    }
  }
  else {
    while ( len-- ) {
      uint32_t col = *data++;
      tft_Write_16S(col);
    }
  }
#else
  const uint16_t *data = (uint16_t*)data_in;

  // PIO sends MS byte first, so bytes are already swapped on transmit
  if(_swapBytes) {
    while (len > 4) {
      WAIT_FOR_FIFO_FREE(5);
      TX_FIFO = data[0];
      TX_FIFO = data[1];
      TX_FIFO = data[2];
      TX_FIFO = data[3];
      TX_FIFO = data[4];
      data += 5;
      len  -= 5;
    }

    if (len) {
      WAIT_FOR_FIFO_FREE(4);
      while(len--) TX_FIFO = *data++;
    }
  }
  else {
    while (len > 4) {
      WAIT_FOR_FIFO_FREE(5);
      TX_FIFO = data[0] << 8 | data[0] >> 8;
      TX_FIFO = data[1] << 8 | data[1] >> 8;
      TX_FIFO = data[2] << 8 | data[2] >> 8;
      TX_FIFO = data[3] << 8 | data[3] >> 8;
      TX_FIFO = data[4] << 8 | data[4] >> 8;
      data += 5;
      len  -= 5;
    }

    if (len) {
      WAIT_FOR_FIFO_FREE(4);
      while(len--) {
        TX_FIFO = *data << 8 | *data >> 8;
        data++;
      }
    }
  }
#endif
}

/***************************************************************************************
** Function name:           GPIO direction control  - supports class functions
** Description:             Set parallel bus to INPUT or OUTPUT
***************************************************************************************/
void TFT_eSPI::busDir(uint32_t mask, uint8_t mode)
{
  // Avoid warnings
  mask = mask;
  mode = mode;
/*
  // mask is unused for generic processor
  // Arduino native functions suited well to a generic driver
  pinMode(TFT_D0, mode);
  pinMode(TFT_D1, mode);
  pinMode(TFT_D2, mode);
  pinMode(TFT_D3, mode);
  pinMode(TFT_D4, mode);
  pinMode(TFT_D5, mode);
  pinMode(TFT_D6, mode);
  pinMode(TFT_D7, mode);
*/
}

/***************************************************************************************
** Function name:           GPIO direction control  - supports class functions
** Description:             Faster GPIO pin input/output switch
***************************************************************************************/
void TFT_eSPI::gpioMode(uint8_t gpio, uint8_t mode)
{
  // Avoid warnings
  gpio = gpio;
  mode = mode;

}

/***************************************************************************************
** Function name:           read byte  - supports class functions
** Description:             Read a byte - parallel bus only - not supported yet
***************************************************************************************/
uint8_t TFT_eSPI::readByte(void)
{
  uint8_t b = 0;
/*
  busDir(0, INPUT);
  digitalWrite(TFT_RD, LOW);

  b |= digitalRead(TFT_D0) << 0;
  b |= digitalRead(TFT_D1) << 1;
  b |= digitalRead(TFT_D2) << 2;
  b |= digitalRead(TFT_D3) << 3;
  b |= digitalRead(TFT_D4) << 4;
  b |= digitalRead(TFT_D5) << 5;
  b |= digitalRead(TFT_D6) << 6;
  b |= digitalRead(TFT_D7) << 7;

  digitalWrite(TFT_RD, HIGH);
  busDir(0, OUTPUT);
*/
  return b;
}

////////////////////////////////////////////////////////////////////////////////////////
#elif defined (RPI_WRITE_STROBE)  // For RPi TFT with write strobe
////////////////////////////////////////////////////////////////////////////////////////

/***************************************************************************************
** Function name:           pushBlock - for ESP32 or RP2040 RPi TFT
** Description:             Write a block of pixels of the same colour
***************************************************************************************/
void TFT_eSPI::pushBlock(uint16_t color, uint32_t len){

  if(len) { tft_Write_16(color); len--; }
  while(len--) {WR_L; WR_H;}
}

/***************************************************************************************
** Function name:           pushPixels - for ESP32 or RP2040 RPi TFT
** Description:             Write a sequence of pixels
***************************************************************************************/
void TFT_eSPI::pushPixels(const void* data_in, uint32_t len)
{
  uint16_t *data = (uint16_t*)data_in;

  if (_swapBytes) while ( len-- ) {tft_Write_16S(*data); data++;}
  else while ( len-- ) {tft_Write_16(*data); data++;}
}

////////////////////////////////////////////////////////////////////////////////////////
#elif defined (SPI_18BIT_DRIVER) // SPI 18-bit colour
////////////////////////////////////////////////////////////////////////////////////////

/***************************************************************************************
** Function name:           pushBlock - for RP2040 and 3 byte RGB display
** Description:             Write a block of pixels of the same colour
***************************************************************************************/
void TFT_eSPI::pushBlock(uint16_t color, uint32_t len)
{
  uint16_t r = (color & 0xF800)>>8;
  uint16_t g = (color & 0x07E0)>>3;
  uint16_t b = (color & 0x001F)<<3;

  // If more than 32 pixels then change to 16-bit transfers with concatenated pixels
  if (len > 32) {
    uint32_t rg = r<<8 | g;
    uint32_t br = b<<8 | r;
    uint32_t gb = g<<8 | b;
    // Must wait before changing to 16-bit
    while (spi_get_hw(SPI_X)->sr & SPI_SSPSR_BSY_BITS) {};
    hw_write_masked(&spi_get_hw(SPI_X)->cr0, (16 - 1) << SPI_SSPCR0_DSS_LSB, SPI_SSPCR0_DSS_BITS);
    while ( len > 1 ) {
      while (!spi_is_writable(SPI_X)){}; spi_get_hw(SPI_X)->dr = rg;
      while (!spi_is_writable(SPI_X)){}; spi_get_hw(SPI_X)->dr = br;
      while (!spi_is_writable(SPI_X)){}; spi_get_hw(SPI_X)->dr = gb;
      len -= 2;
    }
    // Must wait before changing back to 8-bit
    while (spi_get_hw(SPI_X)->sr & SPI_SSPSR_BSY_BITS) {};
    hw_write_masked(&spi_get_hw(SPI_X)->cr0, (8 - 1) << SPI_SSPCR0_DSS_LSB, SPI_SSPCR0_DSS_BITS);
  }

  // Mop up the remaining pixels
  while ( len-- ) {tft_Write_8N(r);tft_Write_8N(g);tft_Write_8N(b);}
}

/***************************************************************************************
** Function name:           pushPixels - for RP2040 and 3 byte RGB display
** Description:             Write a sequence of pixels
***************************************************************************************/
void TFT_eSPI::pushPixels(const void* data_in, uint32_t len){

  uint16_t *data = (uint16_t*)data_in;
  if (_swapBytes) {
    while ( len-- ) {
      uint32_t col = *data++;
      tft_Write_16(col);
    }
  }
  else {
    while ( len-- ) {
      uint32_t col = *data++;
      tft_Write_16S(col);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////
#else //                   Standard SPI 16-bit colour TFT
////////////////////////////////////////////////////////////////////////////////////////

/***************************************************************************************
** Function name:           pushBlock - for RP2040
** Description:             Write a block of pixels of the same colour
***************************************************************************************/
void TFT_eSPI::pushBlock(uint16_t color, uint32_t len){
  while(len--)
  {
    while (!spi_is_writable(SPI_X)){};
    spi_get_hw(SPI_X)->dr = (uint32_t)color;
  }
}

/***************************************************************************************
** Function name:           pushPixels - for RP2040
** Description:             Write a sequence of pixels
***************************************************************************************/
void TFT_eSPI::pushPixels(const void* data_in, uint32_t len){
  uint16_t *data = (uint16_t*)data_in;
  if (_swapBytes) {
    while(len--)
    {
      while (!spi_is_writable(SPI_X)){};
      spi_get_hw(SPI_X)->dr = (uint32_t)(*data++);
    }
  }
  else
  {
    while(len--)
    {
      uint16_t color = *data++;
      color = color >> 8 | color << 8;
      while (!spi_is_writable(SPI_X)){};
      spi_get_hw(SPI_X)->dr = (uint32_t)color;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////
#endif // End of display interface specific functions
////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////
#ifdef RP2040_DMA // DMA functions for 16-bit SPI and 8/16-bit parallel displays
////////////////////////////////////////////////////////////////////////////////////////
/*
These are created in header file:
  uint32_t           dma_tx_channel;
  dma_channel_config dma_tx_config;
*/

/***************************************************************************************
** Function name:           dmaBusy
** Description:             Check if DMA is busy
***************************************************************************************/
bool TFT_eSPI::dmaBusy(void) {
  if (!DMA_Enabled) return false;

  if (dma_channel_is_busy(dma_tx_channel)) return true;

#if !defined (RP2040_PIO_INTERFACE)
  // For SPI must also wait for FIFO to flush and reset format
  while (spi_get_hw(SPI_X)->sr & SPI_SSPSR_BSY_BITS) {};
  hw_write_masked(&spi_get_hw(SPI_X)->cr0, (16 - 1) << SPI_SSPCR0_DSS_LSB, SPI_SSPCR0_DSS_BITS);
#endif

  return false;
}

/***************************************************************************************
** Function name:           dmaWait
** Description:             Wait until DMA is over (blocking!)
***************************************************************************************/
void TFT_eSPI::dmaWait(void)
{
  while (dma_channel_is_busy(dma_tx_channel));

#if !defined (RP2040_PIO_INTERFACE)
  // For SPI must also wait for FIFO to flush and reset format
  while (spi_get_hw(SPI_X)->sr & SPI_SSPSR_BSY_BITS) {};
  hw_write_masked(&spi_get_hw(SPI_X)->cr0, (16 - 1) << SPI_SSPCR0_DSS_LSB, SPI_SSPCR0_DSS_BITS);
#endif
}

/***************************************************************************************
** Function name:           pushPixelsDMA
** Description:             Push pixels to TFT
***************************************************************************************/
void TFT_eSPI::pushPixelsDMA(uint16_t* image, uint32_t len)
{
  if ((len == 0) || (!DMA_Enabled)) return;

  dmaWait();

  channel_config_set_bswap(&dma_tx_config, !_swapBytes);

#if !defined (RP2040_PIO_INTERFACE)
  dma_channel_configure(dma_tx_channel, &dma_tx_config, &spi_get_hw(SPI_X)->dr, (uint16_t*)image, len, true);
#else
  dma_channel_configure(dma_tx_channel, &dma_tx_config, &tft_pio->txf[pio_sm], (uint16_t*)image, len, true);
#endif
}

/***************************************************************************************
** Function name:           pushImageDMA
** Description:             Push image to a window
***************************************************************************************/
// This will clip to the viewport
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
    for (int32_t yb = 0; yb < dh; yb++) {
      memmove((uint8_t*) (buffer + yb * dw), (uint8_t*) (image + dx + w * (yb + dy)), dw << 1);
    }
  }
  // else, if a buffer pointer has been provided copy whole image to the buffer
  else if (buffer != image) {
    memcpy(buffer, image, len*2);
  }

  dmaWait(); // In case we did not wait earlier

  setAddrWindow(x, y, dw, dh);

  channel_config_set_bswap(&dma_tx_config, !_swapBytes);

#if !defined (RP2040_PIO_INTERFACE)
  dma_channel_configure(dma_tx_channel, &dma_tx_config, &spi_get_hw(SPI_X)->dr, (uint16_t*)buffer, len, true);
#else
  dma_channel_configure(dma_tx_channel, &dma_tx_config, &tft_pio->txf[pio_sm], (uint16_t*)buffer, len, true);
#endif
}

/***************************************************************************************
** Function name:           initDMA
** Description:             Initialise the DMA engine - returns true if init OK
***************************************************************************************/
bool TFT_eSPI::initDMA(bool ctrl_cs)
{
  if (DMA_Enabled) return false;

  ctrl_cs = ctrl_cs; // stop unused parameter warning

  dma_tx_channel = dma_claim_unused_channel(false);
  
  if (dma_tx_channel < 0) return false;

  dma_tx_config = dma_channel_get_default_config(dma_tx_channel);

  channel_config_set_transfer_data_size(&dma_tx_config, DMA_SIZE_16);
#if !defined (RP2040_PIO_INTERFACE)
  channel_config_set_dreq(&dma_tx_config, spi_get_index(SPI_X) ? DREQ_SPI1_TX : DREQ_SPI0_TX);
#else
  channel_config_set_dreq(&dma_tx_config, pio_get_dreq(tft_pio, pio_sm, true));
#endif

  DMA_Enabled = true;
  return true;
}

/***************************************************************************************
** Function name:           deInitDMA
** Description:             Disconnect the DMA engine from SPI
***************************************************************************************/
void TFT_eSPI::deInitDMA(void)
{
  if (!DMA_Enabled) return;
  dma_channel_unclaim(dma_tx_channel);
  DMA_Enabled = false;
}

////////////////////////////////////////////////////////////////////////////////////////
#endif // End of DMA FUNCTIONS
////////////////////////////////////////////////////////////////////////////////////////
