// Viewport Demo

// See viewport_commands tab for details of functions available

// This example uses the viewport commands to create a "virtual TFT" within the
// normal TFT display area. This allows a sketch written for a smaller screen to
// be run in a viewport window. By default, the graphics 0,0 datum is set to the
// top left corner of the viewport, but optionally the datum can be kept at the
// corner of the TFT.

// Viewports have a number of potential uses:
// -  create a "virtual" TFT screen smaller than the actual TFT screen
//  - render GUI items (menus etc.) in a viewport, erase GUI item by redrawing whole screen,
//    this will be fast because only the viewport will be refreshed (e.g. clearing menu)
//  - limit screen refresh to a particular area, e.g. changing numbers, icons or graph plotting
//  - showing a small portion of a larger image or sprite, this allows panning and scrolling

// A viewport can have the coordinate datum (position 0,0) either at the top left corner of
// the viewport or at the normal top left corner of the TFT.
// Putting the coordinate datum at the viewport corner means that functions that draw graphics
// in a fixed position can be relocated anywhere on the screen. (see plotBox() below). This
// makes it easier to reposition groups of graphical objects (for example GUI buttons) that have
// fixed relative positions.

#include <SPI.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
}

void loop()
{
  // Normal Screen
  drawX();

  delay(2000);

  // Viewport screen
  tft.setViewport(10, 10, 140, 100);
  tft.frameViewport(TFT_NAVY, -2);
  tft.fillScreen(TFT_BLACK);
  drawX();
  tft.resetViewport();

  delay(2000);

  //Normal screen
  tft.fillScreen(TFT_BLACK);
  drawX();

  delay(2000);

  tft.fillScreen(TFT_BLACK);

  // Viewport as a clipping window (false parameter means coordinate datum stays at TFT top left)
  tft.setViewport(10, 10, tft.width()/2 - 10, tft.height() - 20, false);
  //tft.frameViewport(TFT_NAVY,  2); // Add 2 pixel border inside viewport
  //tft.frameViewport(TFT_NAVY, -2); // Add 2 pixel border outside viewport
  drawX();

  delay(2000);

  while(1)
  {
    tft.resetViewport(); // Reset viewport so width() and height() return TFT size

    uint16_t w = 40;
    uint16_t h = 40;
    uint16_t x = random(tft.width()  - w);
    uint16_t y = random(tft.height() - h);

    tft.setViewport(x, y, w, h);

    plotBox();

    delay(0);
  }
}

void drawX(void)
{
  tft.fillScreen(tft.color565(25,25,25)); // Grey

  // Draw circle
  tft.drawCircle(tft.width()/2, tft.height()/2, tft.width()/4, TFT_RED);

  // Draw diagonal lines
  tft.drawLine(0 ,                0, tft.width()-1, tft.height()-1, TFT_GREEN);
  tft.drawLine(0 , tft.height()-1, tft.width()-1,                0, TFT_BLUE);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, tft.color565(25,25,25));
  tft.drawString("Hello World!", tft.width()/2, tft.height()/2, 4); // Font 4
}

void plotBox(void)
{
  // These are always plotted at a fixed position but they can 
  // be plotted into a viewport anywhere on the screen because
  // a viewport can move the screen datum
  tft.fillScreen(TFT_BLACK); // When a viewport is set, this just fills the viewport
  tft.drawRect(0,0, 40,40, TFT_BLUE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE);
  tft.drawNumber( random(100), 20, 23, 4); // Number in font 4
}
