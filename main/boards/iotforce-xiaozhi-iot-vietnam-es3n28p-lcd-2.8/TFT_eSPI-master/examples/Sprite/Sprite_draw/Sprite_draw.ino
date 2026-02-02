/*

  Sketch to show how a Sprite is created, how to draw pixels
  and text within the Sprite and then push the Sprite onto
  the display screen.

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

// Set delay after plotting the sprite
#define DELAY 1000

// Width and height of sprite
#define WIDTH  128
#define HEIGHT 128

#include <TFT_eSPI.h>                 // Include the graphics library (this includes the sprite functions)

TFT_eSPI    tft = TFT_eSPI();         // Declare object "tft"

TFT_eSprite spr = TFT_eSprite(&tft);  // Declare Sprite object "spr" with pointer to "tft" object

void setup()
{
  Serial.begin(250000);
  Serial.println();

  // Initialise the TFT registers
  tft.init();

  // Optionally set colour depth to 8 or 16 bits, default is 16 if not specified
  // spr.setColorDepth(8);

  // Create a sprite of defined size
  spr.createSprite(WIDTH, HEIGHT);

  // Clear the TFT screen to blue
  tft.fillScreen(TFT_BLUE);
}

void loop(void)
{
  // Fill the whole sprite with black (Sprite is in memory so not visible yet)
  spr.fillSprite(TFT_BLACK);

  // Number of pixels to draw
  uint16_t n = 100;

  // Draw 100 random colour pixels at random positions in sprite
  while (n--)
  {
    uint16_t colour = random(0x10000); // Returns colour 0 - 0xFFFF
    int16_t x = random(WIDTH);        // Random x coordinate
    int16_t y = random(HEIGHT);       // Random y coordinate
    spr.drawPixel( x, y, colour);      // Draw pixel in sprite
  }

  // Draw some lines
  spr.drawLine(1, 0, WIDTH, HEIGHT-1, TFT_GREEN);
  spr.drawLine(0, 0, WIDTH, HEIGHT, TFT_GREEN);
  spr.drawLine(0, 1, WIDTH-1, HEIGHT, TFT_GREEN);
  spr.drawLine(0, HEIGHT-1, WIDTH-1, 0, TFT_RED);
  spr.drawLine(0, HEIGHT, WIDTH, 0, TFT_RED);
  spr.drawLine(1, HEIGHT, WIDTH, 1, TFT_RED);

  // Draw some text with Middle Centre datum
  spr.setTextDatum(MC_DATUM);
  spr.drawString("Sprite", WIDTH / 2, HEIGHT / 2, 4);

  // Now push the sprite to the TFT at position 0,0 on screen
  spr.pushSprite(-40, -40);
  spr.pushSprite(tft.width() / 2 - WIDTH / 2, tft.height() / 2 - HEIGHT / 2);
  spr.pushSprite(tft.width() - WIDTH + 40, tft.height() - HEIGHT + 40);

  delay(DELAY);

  // Fill TFT screen with blue
  tft.fillScreen(TFT_BLUE);

  // Draw a blue rectangle in sprite so when we move it 1 pixel it does not leave a trail
  // on the blue screen background
  spr.drawRect(0, 0, WIDTH, HEIGHT, TFT_BLUE);

  int x = tft.width() / 2  -  WIDTH / 2;
  int y = tft.height() / 2 - HEIGHT / 2;

  uint32_t updateTime = 0;       // time for next update

  while (true)
  {
    // Random movement direction
    int dx = 1; if (random(2)) dx = -1;
    int dy = 1; if (random(2)) dy = -1;

    // Pull it back onto screen if it wanders off
    if (x < -WIDTH/2) dx = 1;
    if (x >= tft.width()-WIDTH/2) dx = -1;
    if (y < -HEIGHT/2) dy = 1;
    if (y >= tft.height()-HEIGHT/2) dy = -1;

    // Draw it 50 time, moving in random direct or staying still
    n = 50;
    int wait = random (50);
    while (n)
    {
      if (updateTime <= millis())
      {
        // Use time delay so sprite does not move fast when not all on screen
        updateTime = millis() + wait;

        // Push the sprite to the TFT screen
        spr.pushSprite(x, y);

        // Change coord for next loop
        x += dx;
        y += dy;
        n--;
        yield(); // Stop watchdog reset
      }
    }
  } // Infinite while, will not exit!
}

