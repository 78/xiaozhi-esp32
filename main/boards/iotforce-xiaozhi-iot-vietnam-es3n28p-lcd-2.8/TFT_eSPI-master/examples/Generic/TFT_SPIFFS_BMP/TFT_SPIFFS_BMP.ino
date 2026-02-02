// This sketch draws BMP images pulled from SPIFFS onto the TFT. It is an
// an example from this library: https://github.com/Bodmer/TFT_eSPI

// Images in SPIFFS must be put in the root folder (top level) to be found
// Use the SPIFFS library example to verify SPIFFS works!

// The example image used to test this sketch can be found in the sketch
// Data folder, press Ctrl+K to see this folder. Use the IDE "Tools" menu
// option to upload the sketches data folder to the SPIFFS

// This sketch has been tested on the ESP32 and ESP8266

//----------------------------------------------------------------------------------------------------

//====================================================================================
//                                  Libraries
//====================================================================================
// Call up the SPIFFS FLASH filing system this is part of the ESP Core
#define FS_NO_GLOBALS
#include <FS.h>

#ifdef ESP32
  #include "SPIFFS.h"  // For ESP32 only
#endif

// Call up the TFT library
#include <TFT_eSPI.h> // Hardware-specific library for ESP8266

// Invoke TFT library
TFT_eSPI tft = TFT_eSPI();

//====================================================================================
//                                    Setup
//====================================================================================
void setup()
{
  Serial.begin(115200);

  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS initialisation failed!");
    while (1) yield(); // Stay here twiddling thumbs waiting
  }
  Serial.println("\r\nSPIFFS initialised.");

  // Now initialise the TFT
  tft.begin();
  tft.setRotation(0);  // 0 & 2 Portrait. 1 & 3 landscape
  tft.fillScreen(TFT_BLACK);
}

//====================================================================================
//                                    Loop
//====================================================================================
void loop()
{
  int x = random(tft.width()  - 128);
  int y = random(tft.height() - 160);

  drawBmp("/parrot.bmp", x, y);

  delay(1000);
}
//====================================================================================

