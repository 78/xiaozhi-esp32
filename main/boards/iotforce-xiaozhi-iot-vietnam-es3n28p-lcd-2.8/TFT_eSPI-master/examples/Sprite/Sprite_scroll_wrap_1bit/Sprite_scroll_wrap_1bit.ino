// This **ONLY** works for 1 bpp Sprites due to lack of bounds checking in the
// Sprite pushImage() function for 8 and 16-bit Sprites (it is on the TO DO list)

// Wrapping scroll example by Bodmer for the TFT_eSPI library

//==========================================================================================

#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

TFT_eSprite gfx = TFT_eSprite(&tft); // Sprite object for graphics write

TFT_eSprite fb = TFT_eSprite(&tft);  // Sprite object for frame buffer

// Width and height of the Sprite
#define WIDTH  60
#define HEIGHT 60

// Define scroll increment in x and y directions
// positive numbers = right, down
// negative numbers = left, up
#define XDELTA  1
#define YDELTA  1

int16_t scroll_x = 0; // Keep track of the scrolled position, this is where the origin 
int16_t scroll_y = 0; // (top left) of the gfx Sprite will be

int16_t radius = 5; // radius of circle

bool grow = true;   // grow or shrink circle

uint16_t *gfxPtr; // Pointer to start of graphics sprite memory area

//==========================================================================================
//==========================================================================================

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.fillScreen(TFT_BLACK);

  tft.setRotation(1);

  //tft.invertDisplay(true);

  // Create a sprite for the graphics
  gfx.setColorDepth(1);
  gfxPtr = (uint16_t*) gfx.createSprite(WIDTH, HEIGHT);      // 450 bytes needed
  gfx.fillSprite(TFT_BLACK); // Note: Sprite is filled with black when created

  // Create a sprite for the frame buffer
  fb.setColorDepth(1);
  fb.createSprite(WIDTH, HEIGHT);      // 450 bytes needed
  fb.fillSprite(TFT_BLACK); // Note: Sprite is filled with black when created

  // Text colour and alignment in graphics Sprite
  gfx.setTextColor(TFT_WHITE, TFT_BLACK);
  gfx.setTextDatum(MC_DATUM);

  // Text colour and alignment in frame buffer
  fb.setTextColor(TFT_WHITE, TFT_BLACK);
  fb.setTextDatum(MC_DATUM);

  // Next 3 lines are for test only to see what we have drawn
  drawGraphics(); // draw the graphics in the gfx sprite and copy to buffer
  gfx.pushSprite(0, 0); // Plot to screen so we see what it looks like
  delay(2000);
}

//==========================================================================================
//==========================================================================================

void loop() {
uint32_t tnow = millis();
  drawGraphics(); // Not needed if scrolling graphics are static

  wrappingScroll(XDELTA, YDELTA);

  // Plot two copies without "Hello", then one in the middle with "Hello"
  //fb.setBitmapColor(TFT_WHITE, TFT_RED  );
  fb.pushSprite(0, 0);       // Plot frame buffer onto the TFT screen
  //fb.setBitmapColor(TFT_WHITE, TFT_BLUE );
  fb.pushSprite(120, 0);     // Plot frame buffer onto the TFT screen

  fb.drawString("Hello", 30, 20, 2); // Plot hello in frame buffer
  //fb.setBitmapColor(TFT_BLACK, TFT_GREEN);
  fb.pushSprite(60, 0);      // Plot frame buffer in middle of other two

  //Serial.println(millis() - tnow);
  delay(20);
}

//==========================================================================================
// Draw graphics in the master Sprite
//==========================================================================================

void drawGraphics(void)
{
  gfx.fillSprite(TFT_BLACK);           // Clear sprite each time and completely redraw

  //gfx.drawRect(0,0,60,60,TFT_WHITE); // Test to check alignment

  gfx.drawCircle( 30 , 30, radius, TFT_WHITE);
  if (grow) radius++;
  else radius--;

  if ( radius > 25 ) grow = false;
  if ( radius <  1 ) grow = true;

  gfx.drawString("World", 30, 40, 2);   // Plot hello in frame buffer
}

//==========================================================================================
// This function scrolls and wraps the graphic in a 1 bit per pixel Sprite, dy and dy
// control the scroll direction. Pixels that scroll off one side appear on the other.
// Scrolling is achieved by plotting one Sprite inside another with an offset. This has
// to be done by plotting 4 times into a second frame buffer Sprite.
//==========================================================================================
void wrappingScroll(int16_t dx, int16_t dy)
{
  // Position the quadrants so they overlap all areas of the buffer
  scroll_x += dx;
  if (scroll_x < -WIDTH) scroll_x += WIDTH;
  if (scroll_x > 0) scroll_x -= WIDTH;

  scroll_y += dy;
  if (scroll_y < -HEIGHT) scroll_y += HEIGHT;
  if (scroll_y > 0) scroll_y -= HEIGHT;

  // push the 4 quadrants of gfx. sprite into the fb. sprite
  // pushImage will do the cropping
  fb.pushImage(scroll_x, scroll_y, WIDTH, HEIGHT, gfxPtr);
  fb.pushImage(scroll_x + WIDTH, scroll_y, WIDTH, HEIGHT, gfxPtr);
  fb.pushImage(scroll_x, scroll_y + HEIGHT, WIDTH, HEIGHT, gfxPtr);
  fb.pushImage(scroll_x  + WIDTH, scroll_y + HEIGHT, WIDTH, HEIGHT, gfxPtr);
}

//==========================================================================================
