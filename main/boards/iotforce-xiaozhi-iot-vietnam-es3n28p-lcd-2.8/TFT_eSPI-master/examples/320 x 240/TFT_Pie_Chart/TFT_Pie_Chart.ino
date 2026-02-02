// This sketch includes a function to draw circle segments
// for pie charts in 1 degree increments

#include <TFT_eSPI.h> // Hardware-specific library
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI(); // Invoke custom library

#define DEG2RAD 0.0174532925

byte inc = 0;
unsigned int col = 0;


void setup(void)
{
  tft.begin();

  tft.setRotation(1);

  tft.fillScreen(TFT_BLACK);
}

void loop() {

  // Draw 4 pie chart segments
  fillSegment(160, 120, 0, 60, 100, TFT_RED);
  fillSegment(160, 120, 60, 30, 100, TFT_GREEN);
  fillSegment(160, 120, 60 + 30, 120, 100, TFT_BLUE);
  fillSegment(160, 120, 60 + 30 + 120, 150, 100, TFT_YELLOW);

  delay(4000);

  // Erase old chart with 360 degree black plot
  fillSegment(160, 120, 0, 360, 100, TFT_BLACK);
}


// #########################################################################
// Draw circle segments
// #########################################################################

// x,y == coords of centre of circle
// start_angle = 0 - 359
// sub_angle   = 0 - 360 = subtended angle
// r = radius
// colour = 16-bit colour value

int fillSegment(int x, int y, int start_angle, int sub_angle, int r, unsigned int colour)
{
  // Calculate first pair of coordinates for segment start
  float sx = cos((start_angle - 90) * DEG2RAD);
  float sy = sin((start_angle - 90) * DEG2RAD);
  uint16_t x1 = sx * r + x;
  uint16_t y1 = sy * r + y;

  // Draw colour blocks every inc degrees
  for (int i = start_angle; i < start_angle + sub_angle; i++) {

    // Calculate pair of coordinates for segment end
    int x2 = cos((i + 1 - 90) * DEG2RAD) * r + x;
    int y2 = sin((i + 1 - 90) * DEG2RAD) * r + y;

    tft.fillTriangle(x1, y1, x2, y2, x, y, colour);

    // Copy segment end to segment start for next segment
    x1 = x2;
    y1 = y2;
  }
}


// #########################################################################
// Return the 16-bit colour with brightness 0-100%
// #########################################################################
unsigned int brightness(unsigned int colour, int brightness)
{
  byte red   = colour >> 11;
  byte green = (colour & 0x7E0) >> 5;
  byte blue  = colour & 0x1F;

  blue =  (blue * brightness)/100;
  green = (green * brightness)/100;
  red =   (red * brightness)/100;

  return (red << 11) + (green << 5) + blue;
}

