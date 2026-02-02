/*
Tests string alignment

Normally strings are printed relative to the top left corner but this can be
changed with the setTextDatum() function. The library has #defines for:

TL_DATUM = 0 = Top left
TC_DATUM = 1 = Top centre
TR_DATUM = 2 = Top right
ML_DATUM = 3 = Middle left
MC_DATUM = 4 = Middle centre
MR_DATUM = 5 = Middle right
BL_DATUM = 6 = Bottom left
BC_DATUM = 7 = Bottom centre
BR_DATUM = 8 = Bottom right

L_BASELINE =  9 = Left character baseline (Line the 'A' character would sit on)
C_BASELINE = 10 = Centre character baseline
R_BASELINE = 11 = Right character baseline

So you can use lines to position text like:

  tft.setTextDatum(BC_DATUM); // Set datum to bottom centre
 
 Needs fonts 2, 4, 6, 7 and 8

  #########################################################################
  ###### DON'T FORGET TO UPDATE THE User_Setup.h FILE IN THE LIBRARY ######
  ######        TO SELECT THE FONTS AND PINS YOU USE, SEE ABOVE      ######
  #########################################################################
 */


#include <SPI.h>

#include <TFT_eSPI.h> // Hardware-specific library

TFT_eSPI tft = TFT_eSPI();       // Invoke custom library

unsigned long drawTime = 0;

int x, y;  // Coordinates for drawing

void setup(void) {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);
}

void loop() {

  tft.fillScreen(TFT_BLACK);

  x = 160, y = 120;
  
  //Test all 9 datums 0-8 when plotting numbers
  for(byte datum = 0; datum < 9; datum++) {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    
    tft.setTextDatum(datum);
    
    tft.drawNumber(42, x, y, 8);
    
    drawCross(x, y, TFT_RED);
    
    delay(1000);
    tft.fillScreen(TFT_BLACK);
  }

  //Test all 9 datums 0-8 when plotting a string
  // Datum works when text size is > 1
  for(byte datum = 0; datum < 9; datum++) {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    
    tft.setTextDatum(datum);

    tft.setTextSize(2);
    
    tft.drawString("[xox]", x, y, 4);
    
    drawCross(x, y, TFT_GREEN);
    
    delay(1000);
    tft.fillScreen(TFT_BLACK);
  }

  tft.setTextSize(1);

  tft.setTextColor(TFT_BLUE, TFT_BLACK);
  // Here a special function is used that automatically centres the text
  // on the x, y coordinate
  tft.drawCentreString("69", x, y, 8);
  
  drawCross(x, y, TFT_CYAN);

  delay(1000);
  
  tft.fillScreen(TFT_BLACK);
  
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.drawRightString("88",160,120,8);
  
  drawCross(x, y, TFT_RED);
  
  delay(1000);
  
  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_WHITE, TFT_BLUE);

  //Test floating point drawing function

  tft.setTextDatum(BC_DATUM); // Set datum to bottom centre

  float test = 67.125;
  y = 180;
  tft.drawFloat(test, 4, x, y, 4);
  delay(1000);
  tft.fillScreen(TFT_BLACK);
  test = -0.555555;
  tft.drawFloat(test, 3, x, y, 4);
  delay(1000);
  tft.fillScreen(TFT_BLACK);
  test = 0.1;
  tft.drawFloat(test, 4, x, y, 4);
  delay(1000);
  tft.fillScreen(TFT_BLACK);
  test = 9999999;
  tft.drawFloat(test, 1, x, y, 4);

  drawCross(x, y, TFT_RED);
  
  delay(4000);
}

void drawCross(int x, int y, unsigned int color)
{
  tft.drawLine(x - 5, y, x + 5, y, color);
  tft.drawLine(x, y - 5, x, y + 5, color);
}





