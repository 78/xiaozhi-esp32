/*====================================================================================

  This example draws a jpeg image in a Sprite then plot a rotated copy of the Sprite
  to the TFT.

  The jpeg used in in the sketch Data folder (press Ctrl+K to see folder)

  The jpeg must be uploaded to the ESP8266 or ESP32 SPIFFS by using the Tools menu
  sketch data upload option of the Arduino IDE. If you do not have that option it can
  be added. Close the Serial Monitor window before uploading to avoid an error message!

  To add the upload option for the ESP8266 see:
  http://www.esp8266.com/viewtopic.php?f=32&t=10081
  https://github.com/esp8266/arduino-esp8266fs-plugin/releases

  To add the upload option for the ESP32 see:
  https://github.com/me-no-dev/arduino-esp32fs-plugin

  Created by Bodmer 6/1/19 as an example to the TFT_eSPI library:
  https://github.com/Bodmer/TFT_eSPI

  Extension functions in the TFT_eFEX library are used to list SPIFFS files and render
  the jpeg to the TFT and to the Sprite:
  https://github.com/Bodmer/TFT_eFEX

  To render the Jpeg image the JPEGDecoder library is needed, this can be obtained
  with the IDE library manager, or downloaded from here:
  https://github.com/Bodmer/JPEGDecoder

  ==================================================================================*/

//====================================================================================
//                                  Libraries
//====================================================================================
// Call up the SPIFFS FLASH filing system, this is part of the ESP Core
#define FS_NO_GLOBALS
#include <FS.h>

#ifdef ESP32
#include "SPIFFS.h" // Needed for ESP32 only
#endif

// https://github.com/Bodmer/TFT_eSPI
#include <TFT_eSPI.h>                 // Hardware-specific library
TFT_eSPI tft = TFT_eSPI();            // Invoke custom library
TFT_eSprite spr = TFT_eSprite(&tft);  // Create Sprite object "spr" with pointer to "tft" object

// https://github.com/Bodmer/TFT_eFEX
#include <TFT_eFEX.h>                 // Include the function extension library
TFT_eFEX fex = TFT_eFEX(&tft);        // Create TFT_eFX object "fex" with pointer to "tft" object


//====================================================================================
//                                    Setup
//====================================================================================
void setup()
{
  Serial.begin(250000); // Used for messages

  tft.begin();
  tft.setRotation(0);  // 0 & 2 Portrait. 1 & 3 landscape
  tft.fillScreen(TFT_BLACK);

  // Create a sprite to hold the jpeg (or part of it)
  spr.createSprite(80, 64);

  // Initialise SPIFFS
  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS initialisation failed!");
    while (1) yield(); // Stay here twiddling thumbs waiting
  }
  Serial.println("\r\nInitialisation done.\r\n");

  // Lists the files so you can see what is in the SPIFFS
  fex.listSPIFFS();

  // Note the / before the SPIFFS file name must be present, this means the file is in
  // the root directory of the SPIFFS, e.g. "/tiger.jpg" for a file called "tiger.jpg"

  // Send jpeg info to serial port
  fex.jpegInfo("/Eye_80x64.jpg");

  // Draw jpeg image in Sprite spr at 0,0
  fex.drawJpeg("/Eye_80x64.jpg", 0 , 0, &spr);
}

//====================================================================================
//                                    Loop
//====================================================================================
void loop()
{

  tft.fillScreen(random(0xFFFF));


  // Set the TFT pivot point to the centre of the screen
  tft.setPivot(tft.width() / 2, tft.height() / 2);

  // Set Sprite pivot point to centre of Sprite
  spr.setPivot(spr.width() / 2, spr.height() / 2);

  // Push Sprite to the TFT at 0,0 (not rotated)
  spr.pushSprite(0, 0);

  delay(1000);

  // Push copies of Sprite rotated through increasing angles 0-360 degrees
  // with 45 degree increments
  for (int16_t angle = 0; angle <= 360; angle += 45) {
    spr.pushRotated(angle);
    delay(500);
  }

  delay(2000);

  // Move Sprite pivot to a point above the image at 40,-60
  // (Note: Top left corner is Sprite coordinate 0,0)
  // The TFT pivot point has already been set to middle of screen.
  /*                    .Pivot point at 40,-60
                        ^
                        |
                       -60
                  < 40 >|
                  ______V______
                 |             |
                 |   Sprite    |
                 |_____________|
  */
  spr.setPivot(40, -60);

  // Push Sprite to screen rotated about the new pivot points
  // negative angle rotates Sprite anticlockwise
  for (int16_t angle = 330; angle >= 0; angle -= 30) {
    spr.pushRotated(angle);
    yield(); // Stop watchdog triggering
  }

  delay(5000);
}
//====================================================================================
