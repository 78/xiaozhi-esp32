/*
  Sketch to show creation of a sprite with a transparent
  background, then plot it on the TFT.

  Example for library:
  https://github.com/Bodmer/TFT_eSPI

  A Sprite is notionally an invisible graphics screen that is
  kept in the processors RAM. Graphics can be drawn into the
  Sprite just as it can be drawn directly to the screen. Once
  the Sprite is completed it can be plotted onto the screen in
  any position. If there is sufficient RAM then the Sprite can
  be the same size as the screen and used as a frame buffer.

  A 16-bit Sprite occupies (2 * width * height) bytes in RAM.

  On a ESP8266 Sprite sizes up to 126 x 160 can be accommodated,
  this size requires 40kBytes of RAM for a 16-bit colour depth.
  
  When 8-bit colour depth sprites are created they occupy
  (width * height) bytes in RAM, so larger sprites can be
  created, or the RAM required is halved.
*/

#include <TFT_eSPI.h>                 // Include the graphics library (this includes the sprite functions)

TFT_eSPI    tft = TFT_eSPI();         // Create object "tft"

TFT_eSprite img = TFT_eSprite(&tft);  // Create Sprite object "img" with pointer to "tft" object
                                      // the pointer is used by pushSprite() to push it onto the TFT

void setup(void) {
  Serial.begin(250000);

  tft.init();

  tft.setRotation(0);
}

void loop() {

  tft.fillScreen(TFT_NAVY);

  // Draw 10 sprites containing a "transparent" colour
  for (int i = 0; i < 10; i++)
  {
    int x = random(240-70);
    int y = random(320-80);
    int c = random(0x10000); // Random colour
    drawStar(x, y, c);
  }

  delay(2000);

  uint32_t dt = millis();

  // Now go bananas and draw 500 more
  for (int i = 0; i < 500; i++)
  {
    int x = random(240-70);
    int y = random(320-80);
    int c = random(0x10000); // Random colour
    drawStar(x, y, c);
    yield(); // Stop watchdog reset
  }

  // Show time in milliseconds to draw and then push 1 sprite to TFT screen
  numberBox( 10, 10, (millis()-dt)/500.0 );

  delay(2000);

}

// #########################################################################
// Create sprite, plot graphics in it, plot to screen, then delete sprite
// #########################################################################
void drawStar(int x, int y, int star_color)
{
  // Create an 8-bit sprite 70x 80 pixels (uses 5600 bytes of RAM)
  img.setColorDepth(8);
  img.createSprite(70, 80);

  // Fill Sprite with a "transparent" colour
  // TFT_TRANSPARENT is already defined for convenience
  // We could also fill with any colour as "transparent" and later specify that
  // same colour when we push the Sprite onto the screen.
  img.fillSprite(TFT_TRANSPARENT);

  // Draw 2 triangles to create a filled in star
  img.fillTriangle(35, 0, 0,59, 69,59, star_color);
  img.fillTriangle(35,79, 0,20, 69,20, star_color);

  // Punch a star shaped hole in the middle with a smaller transparent star
  img.fillTriangle(35, 7, 6,56, 63,56, TFT_TRANSPARENT);
  img.fillTriangle(35,73, 6,24, 63,24, TFT_TRANSPARENT);

  // Push sprite to TFT screen CGRAM at coordinate x,y (top left corner)
  // Specify what colour is to be treated as transparent.
  img.pushSprite(x, y, TFT_TRANSPARENT);

  // Delete it to free memory
  img.deleteSprite();
 
}

// #########################################################################
// Draw a number in a rounded rectangle with some transparent pixels
// #########################################################################
void numberBox(int x, int y, float num )
{

  // Size of sprite
  #define IWIDTH  80
  #define IHEIGHT 35

  // Create a 8-bit sprite 80 pixels wide, 35 high (2800 bytes of RAM needed)
  img.setColorDepth(8);
  img.createSprite(IWIDTH, IHEIGHT);

  // Fill it with black (this will be the transparent colour this time)
  img.fillSprite(TFT_BLACK);

  // Draw a background for the numbers
  img.fillRoundRect(  0, 0,  80, 35, 15, TFT_RED);
  img.drawRoundRect(  0, 0,  80, 35, 15, TFT_WHITE);

  // Set the font parameters
  img.setTextSize(1);           // Font size scaling is x1
  img.setTextColor(TFT_WHITE);  // White text, no background colour

  // Set text coordinate datum to middle right
  img.setTextDatum(MR_DATUM);

  // Draw the number to 3 decimal places at 70,20 in font 4
  img.drawFloat(num, 3, 70, 20, 4);

  // Push sprite to TFT screen CGRAM at coordinate x,y (top left corner)
  // All black pixels will not be drawn hence will show as "transparent"
  img.pushSprite(x, y, TFT_BLACK);

  // Delete sprite to free up the RAM
  img.deleteSprite();
}

