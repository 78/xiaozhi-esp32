// Spiro
// Rainbow pattern generator

#include <TFT_eSPI.h> // Hardware-specific library
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI();       // Invoke custom library

#define DEG2RAD 0.0174532925 // Convert angles in degrees to radians

unsigned long runTime = 0;

float sx = 0, sy = 0;
uint16_t x0 = 0, x1 = 0, yy0 = 0, yy1 = 0;

void setup()
{
  //randomSeed(analogRead(A0));

  // Setup the LCD
  tft.init();
  tft.setRotation(3);
}

void loop()
{
  runTime = millis();

  tft.fillScreen(TFT_BLACK);
  int n = random(2, 23), r = random(20, 100), colour = 0; //rainbow();

  for (long i = 0; i < (360 * n); i++) {
    sx = cos((i / n - 90) * DEG2RAD);
    sy = sin((i / n - 90) * DEG2RAD);
    x0 = sx * (120 - r) + 159;
    yy0 = sy * (120 - r) + 119;


    sy = cos(((i % 360) - 90) * DEG2RAD);
    sx = sin(((i % 360) - 90) * DEG2RAD);
    x1 = sx * r + x0;
    yy1 = sy * r + yy0;
    tft.drawPixel(x1, yy1, rainbow(map(i%360,0,360,0,127))); //colour);
  }
  
  r = random(20, 100);//r = r / random(2,4);
  for (long i = 0; i < (360 * n); i++) {
    sx = cos((i / n - 90) * DEG2RAD);
    sy = sin((i / n - 90) * DEG2RAD);
    x0 = sx * (120 - r) + 159;
    yy0 = sy * (120 - r) + 119;


    sy = cos(((i % 360) - 90) * DEG2RAD);
    sx = sin(((i % 360) - 90) * DEG2RAD);
    x1 = sx * r + x0;
    yy1 = sy * r + yy0;
    tft.drawPixel(x1, yy1, rainbow(map(i%360,0,360,0,127))); //colour);
  }


  delay(2000);
}

unsigned int rainbow(int value)
{
  // Value is expected to be in range 0-127
  // The value is converted to a spectrum colour from 0 = blue through to red = blue
  //int value = random (128);
  byte red = 0; // Red is the top 5 bits of a 16-bit colour value
  byte green = 0;// Green is the middle 6 bits
  byte blue = 0; // Blue is the bottom 5 bits

  byte quadrant = value / 32;

  if (quadrant == 0) {
    blue = 31;
    green = 2 * (value % 32);
    red = 0;
  }
  if (quadrant == 1) {
    blue = 31 - (value % 32);
    green = 63;
    red = 0;
  }
  if (quadrant == 2) {
    blue = 0;
    green = 63;
    red = value % 32;
  }
  if (quadrant == 3) {
    blue = 0;
    green = 63 - 2 * (value % 32);
    red = 31;
  }
  return (red << 11) + (green << 5) + blue;
}


