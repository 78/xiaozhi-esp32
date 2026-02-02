/*
  This sketch demonstrates the use of the horizontal and vertical gradient
  rectangle fill functions.

  Example for library:
  https://github.com/Bodmer/TFT_eSPI

  Created by Bodmer 27/1/22
*/

#include <TFT_eSPI.h>       // Include the graphics library
TFT_eSPI tft = TFT_eSPI();  // Create object "tft"

// -------------------------------------------------------------------------
// Setup
// -------------------------------------------------------------------------
void setup(void) {
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_DARKGREY);
  tft.setTextFont(2);
}

// -------------------------------------------------------------------------
// Main loop
// -------------------------------------------------------------------------
void loop()
{
  tft.fillRectHGradient(0, 0, 160, 50, TFT_MAGENTA, TFT_BLUE);
  tft.setCursor(10,10);
  tft.print("Horizontal gradient");

  tft.fillRectVGradient(0, 60, 160, 50, TFT_ORANGE, TFT_RED);
  tft.setCursor(10,70);
  tft.print("Vertical gradient");

  while(1) delay(100); // Wait here
}
