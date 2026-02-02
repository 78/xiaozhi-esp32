/*
  Display "flicker free" scrolling text and updating number

  Example for library:
  https://github.com/Bodmer/TFT_eSPI

  The sketch has been tested on a 320x240 ILI9341 based TFT, it
  could be adapted for other screen sizes.

  A Sprite is notionally an invisible graphics screen that is
  kept in the processors RAM. Graphics can be drawn into the
  Sprite just as it can be drawn directly to the screen. Once
  the Sprite is completed it can be plotted onto the screen in
  any position. If there is sufficient RAM then the Sprite can
  be the same size as the screen and used as a frame buffer.

  The Sprite occupies (2 * width * height) bytes.

  On a ESP8266 Sprite sizes up to 128 x 160 can be accommodated,
  this size requires 128*160*2 bytes (40kBytes) of RAM, this must be
  available or the processor will crash. You need to make the sprite
  small enough to fit, with RAM spare for any "local variables" that
  may be needed by your sketch and libraries.

  Created by Bodmer 15/11/17

  #########################################################################
  ###### DON'T FORGET TO UPDATE THE User_Setup.h FILE IN THE LIBRARY ######
  #########################################################################
*/

// Size of sprite image for the scrolling text, this requires ~14 Kbytes of RAM
#define IWIDTH  240
#define IHEIGHT 30

// Pause in milliseconds to set scroll speed
#define WAIT 0

#include <TFT_eSPI.h>                 // Include the graphics library (this includes the sprite functions)

TFT_eSPI    tft = TFT_eSPI();         // Create object "tft"

TFT_eSprite img = TFT_eSprite(&tft);  // Create Sprite object "img" with pointer to "tft" object
//                                    // the pointer is used by pushSprite() to push it onto the TFT

// -------------------------------------------------------------------------
// Setup
// -------------------------------------------------------------------------
void setup(void) {
  tft.init();
  tft.setRotation(0);

  tft.fillScreen(TFT_BLUE);
}

// -------------------------------------------------------------------------
// Main loop
// -------------------------------------------------------------------------
void loop() {

  while (1)
  {
    // Create the sprite and clear background to black
    img.createSprite(IWIDTH, IHEIGHT);
    //img.fillSprite(TFT_BLACK); // Optional here as we fill the sprite later anyway

    for (int pos = IWIDTH; pos > 0; pos--)
    {
      build_banner("Hello World", pos);
      img.pushSprite(0, 0);

      build_banner("TFT_eSPI sprite" , pos);
      img.pushSprite(0, 50);

      delay(WAIT);
    }

    // Delete sprite to free up the memory
    img.deleteSprite();

    // Create a sprite of a different size
    numberBox(random(100), 60, 100);

  }
}

// #########################################################################
// Build the scrolling sprite image from scratch, draw text at x = xpos
// #########################################################################

void build_banner(String msg, int xpos)
{
  int h = IHEIGHT;

  // We could just use fillSprite(color) but lets be a bit more creative...

  // Fill with rainbow stripes
  while (h--) img.drawFastHLine(0, h, IWIDTH, rainbow(h * 4));

  // Draw some graphics, the text will apear to scroll over these
  img.fillRect  (IWIDTH / 2 - 20, IHEIGHT / 2 - 10, 40, 20, TFT_YELLOW);
  img.fillCircle(IWIDTH / 2, IHEIGHT / 2, 10, TFT_ORANGE);

  // Now print text on top of the graphics
  img.setTextSize(1);           // Font size scaling is x1
  img.setTextFont(4);           // Font 4 selected
  img.setTextColor(TFT_BLACK);  // Black text, no background colour
  img.setTextWrap(false);       // Turn of wrap so we can print past end of sprite

  // Need to print twice so text appears to wrap around at left and right edges
  img.setCursor(xpos, 2);  // Print text at xpos
  img.print(msg);

  img.setCursor(xpos - IWIDTH, 2); // Print text at xpos - sprite width
  img.print(msg);
}

// #########################################################################
// Create sprite, plot graphics in it, plot to screen, then delete sprite
// #########################################################################
void numberBox(int num, int x, int y)
{
  // Create a sprite 80 pixels wide, 50 high (8kbytes of RAM needed)
  img.createSprite(80, 50);

  // Fill it with black
  img.fillSprite(TFT_BLACK);

  // Draw a background of 2 filled triangles
  img.fillTriangle(  0, 0,  0, 49, 40, 25, TFT_RED);
  img.fillTriangle( 79, 0, 79, 49, 40, 25, TFT_DARKGREEN);

  // Set the font parameters
  img.setTextSize(1);           // Font size scaling is x1
  img.setFreeFont(&FreeSerifBoldItalic24pt7b);  // Select free font
  img.setTextColor(TFT_WHITE);  // White text, no background colour

  // Set text coordinate datum to middle centre
  img.setTextDatum(MC_DATUM);

  // Draw the number in middle of 80 x 50 sprite
  img.drawNumber(num, 40, 25);

  // Push sprite to TFT screen CGRAM at coordinate x,y (top left corner)
  img.pushSprite(x, y);

  // Delete sprite to free up the RAM
  img.deleteSprite();
}


// #########################################################################
// Return a 16-bit rainbow colour
// #########################################################################
unsigned int rainbow(byte value)
{
  // Value is expected to be in range 0-127
  // The value is converted to a spectrum colour from 0 = red through to 127 = blue

  byte red   = 0; // Red is the top 5 bits of a 16-bit colour value
  byte green = 0;// Green is the middle 6 bits
  byte blue  = 0; // Blue is the bottom 5 bits

  byte sector = value >> 5;
  byte amplit = value & 0x1F;

  switch (sector)
  {
    case 0:
      red   = 0x1F;
      green = amplit;
      blue  = 0;
      break;
    case 1:
      red   = 0x1F - amplit;
      green = 0x1F;
      blue  = 0;
      break;
    case 2:
      red   = 0;
      green = 0x1F;
      blue  = amplit;
      break;
    case 3:
      red   = 0;
      green = 0x1F - amplit;
      blue  = 0x1F;
      break;
  }

  return red << 11 | green << 6 | blue;
}


