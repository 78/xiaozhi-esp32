/*

  Sketch to show how a Sprite can use a four-bit image with
  a palette to change the appearance of an image while rendering
  it only once.

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
  this size requires 40kBytes of RAM for a 16-bit color depth.
  
  When 8-bit color depth sprites are created they occupy
  (width * height) bytes in RAM, so larger sprites can be
  created, or the RAM required is halved.

*/


// Set delay after plotting the sprite
#define DELAY 30

// Width and height of sprite
#define WIDTH  164
#define HEIGHT 164

#include "sample_images.h"

TFT_eSPI    tft = TFT_eSPI();         // Declare object "tft"

TFT_eSprite spr = TFT_eSprite(&tft);  // Declare Sprite object "spr" with pointer to "tft" object

byte red = 31; // Red is the top 5 bits of a 16-bit colour value
byte green = 0;// Green is the middle 6 bits
byte blue = 0; // Blue is the bottom 5 bits
byte state = 0;

int rloop = 0;
int incr = 1;

uint16_t cmap[16];

void setup()
{
  Serial.begin(9600);
  Serial.println();

  delay(50);

  // Initialise the TFT registers
  tft.init();
  
  spr.setColorDepth(4);

  // Create a sprite of defined size
  spr.createSprite(WIDTH, HEIGHT);

  // Clear the TFT screen to black
  tft.fillScreen(TFT_BLACK);
  
  // push the image - only need to do this once.
  spr.pushImage(2, 2, 160, 160, (uint16_t *)stars);

  for (int i = 0; i < 16; i++)
    cmap[i] = rainbow();
}

void loop(void)
{
  // create a palette with the defined colors and push it.  
  spr.createPalette(cmap, 16);
  spr.pushSprite(tft.width() / 2 - WIDTH / 2, tft.height() / 2 - HEIGHT / 2);
  
  // update the colors
  for (int i = 0; i < 15; i++) {
    cmap[i] = cmap[i + 1];
  }
  if (incr == 2) {
    (void)rainbow();  // skip alternate steps to go faster
  }
  cmap[15] = rainbow();
  rloop += incr;
  if (rloop > 0xc0) {
    incr = incr == 2 ? 1 : 2;
    rloop = 0;
    
  }
  delay(DELAY);

}

// #########################################################################
// Return a 16-bit rainbow colour
// #########################################################################
unsigned int rainbow()
{
    switch (state) {
      case 0:
        green ++;
        if (green == 64) {
          green = 63;
          state = 1;
        }
        break;
      case 1:
        red--;
        if (red == 255) {
          red = 0;
          state = 2;
        }
        break;
      case 2:
        blue ++;
        if (blue == 32) {
          blue = 31;
          state = 3;
        }
        break;
      case 3:
        green --;
        if (green == 255) {
          green = 0;
          state = 4;
        }
        break;
      case 4:
        red ++;
        if (red == 32) {
          red = 31;
          state = 5;
        }
        break;
      case 5:
        blue --;
        if (blue == 255) {
          blue = 0;
          state = 0;
        }
        break;
    }
    return red << 11 | green << 5 | blue;
}

