// Example sketch to demonstrate the drawing of X BitMap (XBM)
// format image onto the display.

// Information on the X BitMap (XBM) format can be found here:
// https://en.wikipedia.org/wiki/X_BitMap

// This example is part of the TFT_eSPI library:
// https://github.com/Bodmer/TFT_eSPI

// Created by Bodmer 23/04/18

#include "xbm.h"             // Sketch tab header for xbm images

#include <TFT_eSPI.h>        // Hardware-specific library

TFT_eSPI tft = TFT_eSPI();   // Invoke library


void setup()
{
  tft.begin();               // Initialise the display
  tft.fillScreen(TFT_BLACK); // Black screen fill
}

void loop()
{

  // Example 1
  // =========
  // Random x and y coordinates
  int x = random(tft.width()  - logoWidth);
  int y = random(tft.height() - logoHeight);

  // Draw bitmap with top left corner at x,y with foreground only color
  // Bits set to 1 plot as the defined color, bits set to 0 are not plotted
  //              x  y  xbm   xbm width  xbm height  color
  tft.drawXBitmap(x, y, logo, logoWidth, logoHeight, TFT_WHITE);

  delay(500);

  // Erase old one by drawing over with background colour
  tft.drawXBitmap(x, y, logo, logoWidth, logoHeight, TFT_BLACK);


  // Example 2
  // =========
  // New random x and y coordinates
  x = random(tft.width()  - logoWidth);
  y = random(tft.height() - logoHeight);

  // Draw bitmap with top left corner at x,y with foreground and background colors
  // Bits set to 1 plot as the defined fg color, bits set to 0 are plotted as bg color
  //              x  y  xbm   xbm width  xbm height  fg color   bg color
  tft.drawXBitmap(x, y, logo, logoWidth, logoHeight, TFT_WHITE, TFT_RED);

  delay(500);

  // Erase old one by drawing over with background colour
  tft.drawXBitmap(x, y, logo, logoWidth, logoHeight, TFT_BLACK, TFT_BLACK);

}
