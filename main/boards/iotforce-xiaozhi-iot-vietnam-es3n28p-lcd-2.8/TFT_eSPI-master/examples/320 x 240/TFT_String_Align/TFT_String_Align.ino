/*
Tests string alignment

Normally strings are printed relative to the top left corner but this can be
changed with the setTextDatum() function. The library has #defines for:

TL_DATUM = Top left
TC_DATUM = Top centre
TR_DATUM = Top right
ML_DATUM = Middle left
MC_DATUM = Middle centre
MR_DATUM = Middle right
BL_DATUM = Bottom left
BC_DATUM = Bottom centre
BR_DATUM = Bottom right

 
 Needs fonts 2, 4, 6, 7 and 8

 Make sure all the display driver and pin connections are correct by
 editing the User_Setup.h file in the TFT_eSPI library folder.

 #########################################################################
 ###### DON'T FORGET TO UPDATE THE User_Setup.h FILE IN THE LIBRARY ######
 #########################################################################
 */


#include <TFT_eSPI.h> // Hardware-specific library
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI();       // Invoke custom library

unsigned long drawTime = 0;

void setup(void) {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);
}

void loop() {

  tft.fillScreen(TFT_BLACK);
  
  for(byte datum = 0; datum < 9; datum++) {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    
    tft.setTextDatum(datum);
    
    tft.drawNumber(88,160,120,8);
    tft.fillCircle(160,120,5,TFT_RED);
    
    tft.setTextDatum(MC_DATUM);
    
    tft.setTextColor(TFT_BLACK);
    tft.drawString("X",160,120,2);
    delay(1000);
    tft.fillScreen(TFT_BLACK);
  }

  tft.setTextColor(TFT_BLUE, TFT_BLACK);
  tft.drawCentreString("69",160,120,8);
  tft.fillCircle(160,120,5,TFT_YELLOW);
  
  tft.setTextDatum(MC_DATUM);
  
  tft.setTextColor(TFT_BLACK);
  tft.drawString("X",160,120,2);
  delay(1000);
  tft.fillScreen(TFT_BLACK);
  
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.drawRightString("88",160,120,8);
  tft.fillCircle(160,120,5,TFT_YELLOW);
  
  tft.setTextDatum(MC_DATUM);
  
  tft.setTextColor(TFT_BLACK);
  tft.drawString("X",160,120,2);
  delay(1000);
  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_WHITE, TFT_BLUE);

  tft.setTextDatum(MC_DATUM);

  //Test floating point drawing function
  float test = 67.125;
  tft.drawFloat(test, 4, 160, 180, 4);
  delay(1000);
  tft.fillScreen(TFT_BLACK);
  test = -0.555555;
  tft.drawFloat(test, 3, 160, 180, 4);
  delay(1000);
  tft.fillScreen(TFT_BLACK);
  test = 0.1;
  tft.drawFloat(test, 4, 160, 180, 4);
  delay(1000);
  tft.fillScreen(TFT_BLACK);
  test = 9999999;
  tft.drawFloat(test, 1, 160, 180, 4);
  delay(1000);
  
  tft.fillCircle(160,180,5,TFT_YELLOW);
  
  tft.setTextDatum(MC_DATUM);
  
  tft.setTextColor(TFT_BLACK);
  tft.drawString("X",160,180,2);
 
  delay(4000);
}








