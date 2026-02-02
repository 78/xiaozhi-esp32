/*
  Sketch to show creation of a 4-bit sprite with a transparent
  background, then plot it on the TFT. The palette setup and
  palette update functions are also shown in this example.

  Example for library:
  https://github.com/Bodmer/TFT_eSPI

  A Sprite is notionally an invisible graphics screen that is
  kept in the processors RAM. Graphics can be drawn into the
  Sprite just as it can be drawn directly to the screen. Once
  the Sprite is completed it can be plotted onto the screen in
  any position. If there is sufficient RAM then the Sprite can
  be the same size as the screen and used as a frame buffer.

  A 4-bit Sprite occupies (width * height)/2 bytes in RAM.
  For example the "star" 70x80 Sprite uses 2800 bytes.
*/

// This is the default palette for 4-bit colour sprites
// which is built into the library. You can create your
// own palette (use a different array name!). The palette
// is captured and stored in RAM by the Sprite class so a
// copy does not need to be kept in the sketch.

// The default library palette is stored in FLASH and is called:
//   default_4bit_palette
// To edit colours change the default_4bit_palette array name
// at line 32, then edit line 77 to match new name.
// To setup you own palette edit the next line to //*
  /*
  static const uint16_t default_4bit_palette[] PROGMEM = {
    TFT_BLACK,    //  0  ^
    TFT_BROWN,    //  1  |
    TFT_RED,      //  2  |
    TFT_ORANGE,   //  3  |
    TFT_YELLOW,   //  4  Colours 0-9 follow the resistor colour code!
    TFT_GREEN,    //  5  |
    TFT_BLUE,     //  6  |
    TFT_PURPLE,   //  7  |
    TFT_DARKGREY, //  8  |
    TFT_WHITE,    //  9  v
    TFT_CYAN,     // 10  Blue+green mix
    TFT_MAGENTA,  // 11  Blue+red mix
    TFT_MAROON,   // 12  Darker red colour
    TFT_DARKGREEN,// 13  Darker green colour
    TFT_NAVY,     // 14  Darker blue colour
    TFT_PINK      // 15
  };
  //*/

#include <TFT_eSPI.h>                 // Include the graphics library (this includes the sprite functions)

TFT_eSPI    tft = TFT_eSPI();         // Create object "tft"

TFT_eSprite spr = TFT_eSprite(&tft);  // Create Sprite object "spr" with pointer to "tft" object
// the pointer is used by pushSprite() to push it onto the TFT

void setup(void) {
  Serial.begin(115200);

  tft.init();

  tft.setRotation(0);
}

void loop() {

  tft.fillScreen(TFT_DARKGREY);

  // Set color depth to 4 bits
  spr.setColorDepth(4);
  spr.createSprite(70, 80); // The Sprite MUST be created before setting the palette!

  // Pass the default library palette to the Sprite class
  // Edit palette array name if you use your own palette
  spr.createPalette(default_4bit_palette); // <<<<<<<<<<<<<<<<<<<<<<<<< palette array name

  // If <16 colours are defined then specify how many
  // spr.createPalette(default_4bit_palette, 12);

  // After rendering a Sprite you can change the palette to increase the range of colours
  // plotted to the screen to the full 16-bit set.

  // Change palette colour 11 to violet
  spr.setPaletteColor(11, TFT_VIOLET);

  uint16_t color15 = spr.getPaletteColor(15); // The 16-bit colour in a palette can be read back

  // Draw 50 sprites containing a "transparent" colour
  for (int i = 0; i < 50; i++)
  {
    int x = random(tft.width() - 70);
    int y = random(tft.height() - 80);
    int c = random(15); // Random colour 0-14 (4-bit index into color map).  Leave 15 for transparent.
    drawStar(x, y, c);
  }

  delay(2000);

  // Change the palette to a 16-bit grey scale colour
  for (uint8_t i = 0; i < 16; i++) {
    // (i*16+i) produces a value in range 0-255 for the RGB colours
    //                                   Red    Green    Blue
    spr.setPaletteColor(i, tft.color565(i*16+i, i*16+i, i*16+i));
  }

  // Now go bananas and draw 500 more
  for (int i = 0; i < 500; i++)
  {
    int x = random(tft.width() - 70);
    int y = random(tft.height() - 80);
    int c = random(0x10); // Random colour
    drawStar(x, y, c);
    yield(); // Stop watchdog reset
  }

  // Delete it to free memory, the Sprite copy of the palette is also deleted
  spr.deleteSprite();

  delay(2000);
}

// #########################################################################
// Plot graphics to sprite using defined color and plot to screen
// #########################################################################
void drawStar(int x, int y, int star_color)
{
  // star_color will be in the range 0-14 so that 15 is reserved for the
  // transparent colour

  // Fill Sprite with the chosen "transparent" colour
  spr.fillSprite(15); // Fill with color 15

  // Draw 2 triangles to create a filled in star
  spr.fillTriangle(35,  0, 0, 59, 69, 59, star_color);
  spr.fillTriangle(35, 79, 0, 20, 69, 20, star_color);

  // Punch a star shaped hole in the middle with a smaller transparent star
  spr.fillTriangle(35,  7, 6, 56, 63, 56, 15);
  spr.fillTriangle(35, 73, 6, 24, 63, 24, 15);

  // Push sprite to TFT screen at coordinate x,y (top left corner)
  // Specify what colour from the palette is treated as transparent.
  spr.pushSprite(x, y, 15);
}
