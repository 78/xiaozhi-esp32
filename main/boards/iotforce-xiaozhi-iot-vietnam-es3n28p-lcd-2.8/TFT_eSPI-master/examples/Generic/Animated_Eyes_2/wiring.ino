/*
This is the example wiring used for the sketch testing.

You must not define the TFT_CS pin in the TFT_eSPI library if you are
using two independant displays. Instead the chip selects (CS) must be
defined in the "config.h" tab of this sketch. The sketch can then select
the dispay to send graphics to.

If you are only using one display, then TFT_CS can be defined in the
TFT_eSPI library.

The "Setup47_ST7735.h" file was used for the two TFT test using the wiring
as shown below:

Function    ESP32 pin       TFT 1       TFT 2
 MOSI         23     ->     SDA   ->    SDA     // The TFT pin may be named DIN
 MISO         19                                // Not connected
 SCLK         18     ->     CLK   ->    CLK     // The TFT pin may be named SCK
 TFT_DC        2     ->     DC    ->    DC      // The TFT pin may be named AO
 TFT_RST       4     ->     RST   ->    RST
 CS 1         22     ->     CS                  // Connected to TFT 1 only 
 CS 2         21                  ->    CS      // Connected to TFT 2 only 
 +5V/VIN             ->     VCC   ->    VCC
 0V                  ->     GND   ->    GND
 +5V/VIN             ->     LED   ->    LED     // Some displays do not have a backlight BL/LED pin     

The displays used for testing were 128x128 ST7735 displays, the TFT_eSPI library setup file may need
to be changed as these displays come in many configuration variants.


 */
