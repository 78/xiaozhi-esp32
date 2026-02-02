// Icon images are stored in tabs ^ e.g. Alert.h etc.above this line
// more than one icon can be in a header file

// Arrays containing FLASH images can be created with UTFT library tool:
// (libraries\UTFT\Tools\ImageConverter565.exe)
// Convert to .c format then copy into a new tab

/*
 This sketch demonstrates loading images from arrays stored in program (FLASH) memory.

 Works with TFT_eSPI library here:
 https://github.com/Bodmer/TFT_eSPI

 This sketch does not use/need any fonts at all...

 Code derived from ILI9341_due library example

 Make sure all the display driver and pin connections are correct by
 editing the User_Setup.h file in the TFT_eSPI library folder.

 #########################################################################
 ###### DON'T FORGET TO UPDATE THE User_Setup.h FILE IN THE LIBRARY ######
 #########################################################################
*/

#include <TFT_eSPI.h>       // Hardware-specific library

TFT_eSPI tft = TFT_eSPI();  // Invoke custom library

// Include the header files that contain the icons
#include "Alert.h"
#include "Close.h"
#include "Info.h"

long count = 0; // Loop count

void setup()
{
  Serial.begin(115200);
  tft.begin();
  tft.setRotation(1);	// landscape

  tft.fillScreen(TFT_BLACK);

  // Swap the colour byte order when rendering
  tft.setSwapBytes(true);

  // Draw the icons
  tft.pushImage(100, 100, infoWidth, infoHeight, info);
  tft.pushImage(140, 100, alertWidth, alertHeight, alert);
  tft.pushImage(180, 100, closeWidth, closeHeight, closeX);

  // Pause here to admire the icons!
  delay(2000);

}

void loop()
{
  // Loop filling and clearing screen
  tft.pushImage(random(tft.width() -  infoWidth), random(tft.height() -  infoHeight),  infoWidth,  infoHeight, info);
  tft.pushImage(random(tft.width() - alertWidth), random(tft.height() - alertHeight), alertWidth, alertHeight, alert);
  tft.pushImage(random(tft.width() - closeWidth), random(tft.height() - closeHeight), alertWidth, closeHeight, closeX);

  // Clear screen after 100 x 3 = 300 icons drawn
  if (1000 == count++) {
    count = 1;
    tft.setRotation(2 * random(2)); // Rotate randomly to clear display left>right or right>left to reduce monotony!
    tft.fillScreen(TFT_BLACK);
    tft.setRotation(1);
  }
}
