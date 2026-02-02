/*
  Sketch to show scrolling of the graphics in sprites.

  This sketch scrolls a 1 bit per pixel (1 bpp) Sprite.
  
  In a 1-bit Sprite any colour except TFT_BLACK turns a pixel "ON"
  TFT_BLACK turns a pixel "OFF".

  ON and OFF pixels can be set to any two colours before
  rendering to the screen with pushSprite, for example:
  tft.setBitmapColor(ON_COLOR, OFF_COLOR);

  Scrolling moves the pixels in a defined rectangle within
  the Sprite. By default the whole sprite is scrolled.
  The gap left by scrolling is filled with a defined colour.

  Example for library:
  https://github.com/Bodmer/TFT_eSPI

  A Sprite is notionally an invisible graphics screen that is
  kept in the processors RAM. Graphics can be drawn into the
  Sprite just as it can be drawn directly to the screen. Once
  the Sprite is completed it can be plotted onto the screen in
  any position. If there is sufficient RAM then the Sprite can
  be the same size as the screen and used as a frame buffer.

  A 1-bit Sprite occupies (width * height)/8 bytes in RAM.

*/

#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

TFT_eSprite graph1 = TFT_eSprite(&tft); // Sprite object graph1

TFT_eSprite stext1 = TFT_eSprite(&tft); // Sprite object stext1

TFT_eSprite stext2 = TFT_eSprite(&tft); // Sprite object stext2

int graphVal = 1;
int delta = 1;
int grid = 0;
int tcount = 0;

//==========================================================================================
void setup() {
  tft.init();
  tft.fillScreen(TFT_BLACK);

  // Create a sprite for the graph
  graph1.setColorDepth(1);
  graph1.createSprite(128, 61);
  graph1.fillSprite(TFT_BLACK); // Note: Sprite is filled with black when created

  // The scroll area is set to the full sprite size upon creation of the sprite
  // but we can change that by defining a smaller area using "setScrollRect()"if needed
  // parameters are x,y,w,h,color as in drawRect(), the color fills the gap left by scrolling
  //graph1.setScrollRect(64, 0, 64, 61, TFT_BLACK);  // Try this line to change the graph scroll area

  // Create a sprite for the scrolling numbers
  stext1.setColorDepth(1);
  stext1.createSprite(32, 64);
  stext1.fillSprite(TFT_BLACK); // Fill sprite with blue
  stext1.setScrollRect(0, 0, 32, 64, TFT_BLACK);     // here we set scroll gap fill color to blue
  stext1.setTextColor(TFT_WHITE); // White text, no background
  stext1.setTextDatum(BR_DATUM);  // Bottom right coordinate datum

  // Create a sprite for Hello World
  stext2.setColorDepth(1);
  stext2.createSprite(80, 16);
  stext2.fillSprite(TFT_BLACK);
  stext2.setScrollRect(0, 0, 40, 16, TFT_BLACK); // Scroll the "Hello" in the first 40 pixels
  stext2.setTextColor(TFT_WHITE); // White text, no background
}

//==========================================================================================
void loop() {
  // Draw point in graph1 sprite at far right edge (this will scroll left later)
  graph1.drawFastVLine(127,60-graphVal,2,TFT_WHITE); // draw 2 pixel point on graph

  // Draw number in stext1 sprite at 31,63 (bottom right datum set)
  stext1.drawNumber(graphVal, 31, 63, 2); // plot value in font 2

  // Push the sprites onto the TFT at specified coordinates
  tft.setBitmapColor(TFT_WHITE, TFT_BLUE); // Specify the colours of the ON and OFF pixels
  graph1.pushSprite(0, 0);

  tft.setBitmapColor(TFT_GREEN, TFT_BLACK);
  stext1.pushSprite(0, 64);

  tft.setBitmapColor(TFT_BLACK, TFT_YELLOW);
  stext2.pushSprite(60, 70);

  // Change the value to plot
  graphVal+=delta;

  // If the value reaches a limit, then change delta of value
  if (graphVal >= 60)     delta = -1;  // ramp down value
  else if (graphVal <= 1) delta = +1;  // ramp up value

  delay(50); // wait so things do not scroll too fast

  // Now scroll the sprites scroll(dt, dy) where:
  // dx is pixels to scroll, left = negative value, right = positive value
  // dy is pixels to scroll, up = negative value, down = positive value
  graph1.scroll(-1, 0); // scroll graph 1 pixel left, 0 up/down
  stext1.scroll(0,-16); // scroll stext 0 pixels left/right, 16 up
  stext2.scroll(1);     // scroll stext 1 pixel right, up/down default is 0

  // Draw the grid on far right edge of sprite as graph has now moved 1 pixel left
  grid++;
  if (grid >= 10)
  { // Draw a vertical line if we have scrolled 10 times (10 pixels)
    grid = 0;
    graph1.drawFastVLine(127, 0, 61, TFT_WHITE); // draw line on graph
  }
  else
  { // Otherwise draw points spaced 10 pixels for the horizontal grid lines
    for (int p = 0; p <= 60; p += 10) graph1.drawPixel(127, p, TFT_WHITE);
  }

  tcount--;
  if (tcount <=0)
  { // If we have scrolled 40 pixels the redraw text
    tcount = 40;
    stext2.drawString("Hello World", 6, 0, 2); // draw at 6,0 in sprite, font 2
  }

} // Loop back and do it all again
//==========================================================================================
