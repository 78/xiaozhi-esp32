/*
  This sketch is the same as the Font_Demo_2 example, except the fonts in this
  example are in a FLASH (program memory) array. This means that processors
  such as the STM32 series that are not supported by a SPIFFS library can use
  smooth (anti-aliased) fonts.
*/

/*
  There are four different methods of plotting anti-aliased fonts to the screen.

  This sketch uses method 2, using graphics calls plotting direct to the TFT:
    tft.drawString(string, x, y);
    tft.drawNumber(integer, x, y);
    tft.drawFloat(float, dp, x, y); // dp = number of decimal places

  setTextDatum() and setTextPadding() functions work with those draw functions.

  This method is good for static text that does not change often because changing
  values may flicker.
  
*/

// A processing sketch to create new fonts can be found in the Tools folder of TFT_eSPI
// https://github.com/Bodmer/TFT_eSPI/tree/master/Tools/Create_Smooth_Font/Create_font

// This sketch uses font files created from the Noto family of fonts:
// https://www.google.com/get/noto/

#include "NotoSansBold15.h"
#include "NotoSansBold36.h"

// The font names are arrays references, thus must NOT be in quotes ""
#define AA_FONT_SMALL NotoSansBold15
#define AA_FONT_LARGE NotoSansBold36

#include <SPI.h>
#include <TFT_eSPI.h>       // Hardware-specific library

TFT_eSPI tft = TFT_eSPI();

void setup(void) {

  Serial.begin(250000);

  tft.begin();

  tft.setRotation(1);
}

void loop() {

  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_WHITE, TFT_BLACK); // Set the font colour and the background colour

  tft.setTextDatum(TC_DATUM); // Top Centre datum

  int xpos = tft.width() / 2; // Half the screen width
  int ypos = 10;


  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
  // Small font
  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

  tft.loadFont(AA_FONT_SMALL); // Must load the font first

  tft.drawString("Small 15pt font", xpos, ypos);

  ypos += tft.fontHeight();   // Get the font height and move ypos down

  tft.setTextColor(TFT_GREEN, TFT_BLACK);

  // If the string does not fit the screen width, then the next character will wrap to a new line
  tft.drawString("Ode To A Small Lump Of Green Putty I Found In My Armpit One Midsummer Morning", xpos, ypos);

  tft.setTextColor(TFT_GREEN, TFT_BLUE); // Background colour does not match the screen background!
  tft.drawString("Anti-aliasing causes odd looking shadow effects if the text and screen background colours are not the same!", xpos, ypos + 60);

  tft.unloadFont(); // Remove the font to recover memory used

  delay(5000);

  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
  // Large font
  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

  tft.loadFont(AA_FONT_LARGE); // Load another different font

  tft.fillScreen(TFT_BLACK);

  // The "true" parameter forces background drawing for smooth fonts
  tft.setTextColor(TFT_GREEN, TFT_BLUE, true); // Change the font colour and the background colour

  tft.drawString("36pt font", xpos, ypos);

  ypos += tft.fontHeight();  // Get the font height and move ypos down

  // Set text padding to 100 pixels wide area to over-write old values on screen
  tft.setTextPadding(100);

  // Draw changing numbers - likely to flicker using this plot method!
  for (int i = 0; i <= 99; i++) {
    tft.drawFloat(i / 10.0, 1, xpos, ypos);
    delay (200);
  }

  // Turn off text padding by setting value to 0
  tft.setTextPadding(0);

  tft.unloadFont(); // Remove the font to recover memory used

  delay(5000);

  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
  // Setting the 12 datum positions works with free fonts
  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

  // Integer numbers, floats and strings can be drawn relative to a x,y datum, e.g.:
  // tft.drawNumber( 123, x, y);
  // tft.drawFloat( 1.23, dp, x, y); // Where dp is number of decimal places to show
  // tft.drawString( "Abc", x, y);

  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_DARKGREY, TFT_BLACK, false);

  // Use middle of screen as datum
  xpos = tft.width() /2;
  ypos = tft.height()/2;

  tft.loadFont(AA_FONT_SMALL);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("[Top left]", xpos, ypos);
  drawDatumMarker(xpos, ypos);
  delay(1000);

  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("[Top centre]", xpos, ypos);
  drawDatumMarker(xpos, ypos);
  delay(1000);

  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TR_DATUM);
  tft.drawString("[Top right]", xpos, ypos);
  drawDatumMarker(xpos, ypos);
  delay(1000);

  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(ML_DATUM);
  tft.drawString("[Middle left]", xpos, ypos);
  drawDatumMarker(xpos, ypos);
  delay(1000);

  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("[Middle centre]", xpos, ypos);
  drawDatumMarker(xpos, ypos);
  delay(1000);

  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MR_DATUM);
  tft.drawString("[Middle right]", xpos, ypos);
  drawDatumMarker(xpos, ypos);
  delay(1000);

  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(BL_DATUM);
  tft.drawString("[Bottom left]", xpos, ypos);
  drawDatumMarker(xpos, ypos);
  delay(1000);

  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(BC_DATUM);
  tft.drawString("[Bottom centre]", xpos, ypos);
  drawDatumMarker(xpos, ypos);
  delay(1000);

  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(BR_DATUM);
  tft.drawString("[Bottom right]", xpos, ypos);
  drawDatumMarker(xpos, ypos);
  delay(1000);

  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(L_BASELINE);
  tft.drawString("[Left baseline]", xpos, ypos);
  drawDatumMarker(xpos, ypos);
  delay(1000);

  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(C_BASELINE);
  tft.drawString("[Centre baseline]", xpos, ypos);
  drawDatumMarker(xpos, ypos);
  delay(1000);

  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(R_BASELINE);
  tft.drawString("[Right baseline]", xpos, ypos);
  drawDatumMarker(xpos, ypos);
  delay(1000);

  tft.unloadFont(); // Remove the font to recover memory used

  delay(4000);

}

// Draw a + mark centred on x,y
void drawDatumMarker(int x, int y)
{
  tft.drawLine(x - 5, y, x + 5, y, TFT_GREEN);
  tft.drawLine(x, y - 5, x, y + 5, TFT_GREEN);
}
