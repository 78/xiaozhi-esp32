/*
 Font draw speed and flicker test, draws all numbers 0-999 in each font
 Average time in milliseconds to draw each character is shown in red
 
 A total of 2890 characters are drawn in each font then the time per
 character printed on screen
 
  #########################################################################
  ###### DON'T FORGET TO UPDATE THE User_Setup.h FILE IN THE LIBRARY ######
  #########################################################################
 */

#include <SPI.h>

#include <TFT_eSPI.h> // Hardware-specific library

TFT_eSPI tft = TFT_eSPI();       // Invoke custom library

unsigned long drawTime = 0;

void setup(void) {
  tft.begin();
  tft.setRotation(1);
}

void loop() {
  int xpos;

  //Measure time to clear screen
  //drawTime = millis();
  tft.fillScreen(TFT_BLACK);
  //drawTime = millis() - drawTime;
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  //tft.drawNumber(drawTime, 10, 100, 4);
  //delay(1000);

  drawTime = millis();

  // Print all numbers from 0 to 999 in font 1 and calculate character draw time
  for (int i = 0; i < 1000; i++) {
    yield(); tft.drawNumber(i, 100, 80, 1);
  }

  drawTime = millis() - drawTime;

  tft.setTextColor(TFT_RED, TFT_BLACK);
  xpos = 50;
  xpos += tft.drawFloat(drawTime / 2890.0, 3, xpos, 180, 4);
  tft.drawString("ms per character", xpos, 180, 4);
  if (drawTime < 100) tft.drawString("Font 1 not loaded!", 50, 210, 4);
  
  delay(4000);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
    drawTime = millis();

  // Print all numbers from 0 to 999 in font 2 and calculate character draw time
  for (int i = 0; i < 1000; i++) {
    yield(); tft.drawNumber(i, 100, 80, 2);
  }

  drawTime = millis() - drawTime;

  tft.setTextColor(TFT_RED, TFT_BLACK);
  xpos = 50;
  xpos += tft.drawFloat(drawTime / 2890.0, 3, xpos, 180, 4);
  tft.drawString("ms per character", xpos, 180, 4);
  if (drawTime < 200) tft.drawString("Font 2 not loaded!", 50, 210, 4);
  
  delay(4000);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
    drawTime = millis();

  // Print all numbers from 0 to 999 in font 4 and calculate character draw time
  for (int i = 0; i < 1000; i++) {
    yield(); tft.drawNumber(i, 100, 80, 4);
  }

  drawTime = millis() - drawTime;

  tft.setTextColor(TFT_RED, TFT_BLACK);
  xpos = 50;
  xpos += tft.drawFloat(drawTime / 2890.0, 3, xpos, 180, 4);
  tft.drawString("ms per character", xpos, 180, 4);
  if (drawTime < 200) tft.drawString("Font 4 not loaded!", 50, 210, 4);
  
  delay(4000);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
    drawTime = millis();

  // Print all numbers from 0 to 999 in font 6 and calculate character draw time
  for (int i = 0; i < 1000; i++) {
    yield(); tft.drawNumber(i, 100, 80, 6);
  }

  drawTime = millis() - drawTime;

  tft.setTextColor(TFT_RED, TFT_BLACK);
  xpos = 50;
  xpos += tft.drawFloat(drawTime / 2890.0, 3, xpos, 180, 4);
  tft.drawString("ms per character", xpos, 180, 4);
  if (drawTime < 200) tft.drawString("Font 6 not loaded!", 50, 210, 4);
  
  delay(4000);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
    drawTime = millis();

  // Print all numbers from 0 to 999 in font 7 and calculate character draw time
  for (int i = 0; i < 1000; i++) {
    yield(); tft.drawNumber(i, 100, 80, 7);
  }

  drawTime = millis() - drawTime;

  tft.setTextColor(TFT_RED, TFT_BLACK);
  xpos = 50;
  xpos += tft.drawFloat(drawTime / 2890.0, 3, xpos, 180, 4);
  tft.drawString("ms per character", xpos, 180, 4);
  if (drawTime < 200) tft.drawString("Font 7 not loaded!", 50, 210, 4);
  
  delay(4000);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
    drawTime = millis();

  // Print all numbers from 0 to 999 in font 8 and calculate character draw time
  for (int i = 0; i < 1000; i++) {
    yield(); tft.drawNumber(i, 100, 80, 8);
  }

  drawTime = millis() - drawTime;

  tft.setTextColor(TFT_RED, TFT_BLACK);
  xpos = 50;
  xpos += tft.drawFloat(drawTime / 2890.0, 3, xpos, 180, 4);
  tft.drawString("ms per character", xpos, 180, 4);
  if (drawTime < 200) tft.drawString("Font 8 not loaded!", 50, 210, 4);
  
  delay(4000);
}








