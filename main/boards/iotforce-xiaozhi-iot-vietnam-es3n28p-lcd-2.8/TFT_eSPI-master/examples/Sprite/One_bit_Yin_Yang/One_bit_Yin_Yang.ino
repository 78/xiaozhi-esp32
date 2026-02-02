// This sketch draws a rotating Yin and Yang symbol. It illustrates
// the drawing and rendering of simple animated graphics using
// a 1 bit per pixel (1 bpp) Sprite.

// Note:  TFT_BLACK sets the pixel value to 0
// Any other colour sets the pixel value to 1

// A square sprite of side = 2 x RADIUS will be created
// (80 * 80)/8 = 800 bytes needed for 1 bpp sprite
//              6400 bytes for 8 bpp
//             12800 bytes for 16 bpp
#define RADIUS 40      // Radius of completed symbol = 40

#define WAIT 0         // Loop delay

// 1bpp Sprites are economical on memory but slower to render
#define COLOR_DEPTH 1  // Colour depth (1, 8 or 16 bits per pixel)

// Rotation angle increment and start angle
#define ANGLE_INC 3
int angle = 0;

#include <TFT_eSPI.h>                 // Hardware-specific library

TFT_eSPI    tft = TFT_eSPI();         // Invoke library

TFT_eSprite img = TFT_eSprite(&tft);  // Sprite class


// -------------------------------------------------------------------------
void setup(void)
{
  tft.begin();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLUE);

  img.setColorDepth(COLOR_DEPTH);
  img.createSprite(RADIUS*2+1, RADIUS*2+1);
  img.fillSprite(TFT_BLACK);
}
// -------------------------------------------------------------------------
// -------------------------------------------------------------------------
void loop() {
  // Draw Yin and Yang symbol circles into Sprite
  yinyang(RADIUS, RADIUS, angle, RADIUS);

  // Set the 2 pixel palette colours that 1 and 0 represent on the display screen
  img.setBitmapColor(TFT_WHITE, TFT_BLACK); 

  // Push Sprite image to the TFT screen at x, y
  img.pushSprite(tft.width()/2 - RADIUS, 0); // Plot sprite

  angle+=3;                   //Increment angle to rotate circle positions
  if (angle > 359) angle = 0; // Limit angle range

  // Slow things down
  delay(WAIT);
}
// -------------------------------------------------------------------------


// =========================================================================
// Draw circles for Yin and Yang - rotate positions to create symbol
// =========================================================================
// x,y == coordinate center within Sprite
// start_angle = 0 - 359
// r = radius

void yinyang(int x, int y, int start_angle, int r)
{
  int x1 = 0; // getCoord() will update these
  int y1 = 0;

  getCoord(x, y, &x1, &y1, r/2, start_angle); // Get x1 ,y1
  img.fillCircle( x1,  y1, r/2, TFT_WHITE);
  img.fillCircle( x1,  y1, r/8, TFT_BLACK);

  getCoord(x, y, &x1, &y1, r/2, start_angle + 180);
  img.fillCircle( x1,  y1, r/2, TFT_BLACK);
  img.fillCircle( x1,  y1, r/8, TFT_WHITE);
  
  img.drawCircle(x, y, r, TFT_WHITE);
}

// =========================================================================
// Get coordinates of end of a vector, pivot at x,y, length r, angle a
// =========================================================================
// Coordinates are returned to caller via the xp and yp pointers
#define RAD2DEG 0.0174532925
void getCoord(int x, int y, int *xp, int *yp, int r, int a)
{
  float sx1 = cos( (a-90) * RAD2DEG );    
  float sy1 = sin( (a-90) * RAD2DEG );
  *xp =  sx1 * r + x;
  *yp =  sy1 * r + y;
}

