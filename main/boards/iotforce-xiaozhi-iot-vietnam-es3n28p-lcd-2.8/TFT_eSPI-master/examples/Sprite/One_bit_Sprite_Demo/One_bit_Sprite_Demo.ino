/*
  Sketch to show creation of a 1bpp sprite with a transparent
  background, then plot it on the TFT.

  Example for library:
  https://github.com/Bodmer/TFT_eSPI

  A Sprite is notionally an invisible graphics screen that is
  kept in the processors RAM. Graphics can be drawn into the
  Sprite just as it can be drawn directly to the screen. Once
  the Sprite is completed it can be plotted onto the screen in
  any position. If there is sufficient RAM then the Sprite can
  be the same size as the screen and used as a frame buffer.

  A 1-bit Sprite occupies (width * height)/8 bytes in RAM. So,
  for example, a 320 x 240 pixel Sprite occupies 9600 bytes.
*/
// A new setBitmapColor(fg_color, bg_color) function allows
// any 2 colours to be used for the 1 bit sprite.
// One colour can also be defined as transparent when
// rendering to the screen.


#include <TFT_eSPI.h>                 // Include the graphics library (this includes the sprite functions)

TFT_eSPI    tft = TFT_eSPI();         // Create object "tft"

TFT_eSprite img = TFT_eSprite(&tft);  // Create Sprite object "img" with pointer to "tft" object
                                      // the pointer is used by pushSprite() to push it onto the TFT

#define BITS_PER_PIXEL 1              // How many bits per pixel in Sprite

// =========================================================================
void setup(void) {
  Serial.begin(250000);

  tft.init();

  tft.setRotation(0);
}
// =========================================================================
void loop() {

  tft.fillScreen(TFT_NAVY);

  // Draw 10 sprites containing a "transparent" colour
  for (int i = 0; i < 10; i++)
  {
    int x = random(240 - 70);
    int y = random(320 - 80);
    int c = random(0x10000); // Random colour 0 - 0xFFFF
    drawStar(x, y, c);
  }

  delay(2000);

  // Now go bananas and draw 500 more
  for (int i = 0; i < 500; i++)
  {
    int x = random(240 - 70);
    int y = random(320 - 80);
    int c = random(0x10000); // Random colour
    drawStar(x, y, c);
    yield(); // Stop watchdog reset
  }

  delay(2000);
}
// =========================================================================
// Create sprite, plot graphics in it, plot to screen, then delete sprite
// =========================================================================
void drawStar(int x, int y, int star_color)
{
  // 1 bpp colour values can only be 1 or 0 (one or zero)
  uint16_t transparent = 0; // The transparent colour, can only be 1 or 0

  // Create an 1 bit (2 colour) sprite 70x80 pixels (uses 70*80/8 = 700 bytes of RAM)
  // Colour depths of 8 bits per pixel and 16 bits are also supported.
  img.setColorDepth(BITS_PER_PIXEL);         // Set colour depth first
  img.createSprite(70, 80);                  // then create the sprite

  // Fill Sprite with the colour that will be defined later as "transparent"
  // We could also fill with any colour as transparent, and later specify that
  // same colour when we push the Sprite onto the display screen.
  img.fillSprite(transparent);

  // Draw 2 triangles to create a filled in star
  img.fillTriangle(35, 0, 0, 59, 69, 59, star_color);
  img.fillTriangle(35, 79, 0, 20, 69, 20, star_color);

  // Punch a star shaped hole in the middle with a smaller "transparent" star
  img.fillTriangle(35, 7, 6, 56, 63, 56, transparent);
  img.fillTriangle(35, 73, 6, 24, 63, 24, transparent);

  // Set the 2 pixel colours that 1 and 0 represent on the display screen
  img.setBitmapColor(star_color, transparent);

  // Push sprite to TFT screen CGRAM at coordinate x,y (top left corner)
  // Specify what colour is to be treated as transparent (black in this example)
  img.pushSprite(x, y, transparent);

  // Delete Sprite to free memory, creating and deleting takes very little time.
  img.deleteSprite();
}
// =========================================================================

