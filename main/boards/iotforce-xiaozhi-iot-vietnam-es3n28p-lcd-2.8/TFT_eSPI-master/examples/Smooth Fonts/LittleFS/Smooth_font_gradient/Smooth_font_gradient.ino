/*
  This sketch is based on Font Demo 1. It introduces a method for rendering
  anti-aliased fonts on a graded background. This is achieved by telling the
  TFT_eSPI library the pixel color at each point on the screen. In this sketch
  a graded background is drawn, the color of each pixel can therefore be
  determined. The TFT does not need to support reading of the graphics memory.
  The sketch could be adapted so only part of the screen is has a color gradient.

  The TFT_eSPI library must be given the name of the function in the sketch
  that will return the pixel xolor at a position x,y on the TFT. In this
  sketch that function is called gradientColor, so this line is included:

    tft.setCallback(gradientColor);

  TFT_eSPI will call this function during the rendering of the anti-aliased
  font to blend the edges of each character with the returned color.

  If the TFT supports reading the screen RAM then the returned value can be
  tft.readPixel(x,y) and anti-aliased text can the be drawn over any screen
  image. See "Smooth_font_reading_TFT" example.
*/
//  The fonts used are in the sketch data folder, press Ctrl+K to view.

//  Upload the fonts and icons to LittleFS (must set at least 1M for LittleFS) using the
//  "Tools"  "ESP8266 LittleFS Data Upload" menu option in the IDE.
//  To add this option follow instructions here for the ESP8266:
//  https://github.com/earlephilhower/arduino-esp8266littlefs-plugin

//  Close the IDE and open again to see the new menu option.

//  A processing sketch to create new fonts can be found in the Tools folder of TFT_eSPI
//  https://github.com/Bodmer/TFT_eSPI/tree/master/Tools/Create_Smooth_Font/Create_font

//  This sketch uses font files created from the Noto family of fonts:
//  https://www.google.com/get/noto/

#define AA_FONT_SMALL "NotoSansBold15"
#define AA_FONT_LARGE "NotoSansBold36"

// Font files are stored in Flash FS
#include <FS.h>
#include <LittleFS.h>
#define FlashFS LittleFS

#include <SPI.h>
#include <TFT_eSPI.h>       // Hardware-specific library

TFT_eSPI tft = TFT_eSPI();

#define TOP_COLOR    TFT_RED
#define BOTTOM_COLOR TFT_BLACK

#define GRADIENT_HEIGHT (9 + tft.fontHeight() * 5) // Gradient over 5 lines
#define OUTSIDE_GRADIENT TFT_BLUE

uint16_t gradientColor(uint16_t x, uint16_t y)
{
  if (y > GRADIENT_HEIGHT) return OUTSIDE_GRADIENT;   // Outside gradient area
  uint8_t alpha = (255 * y) / GRADIENT_HEIGHT;        // alpha is a value in the range 0-255
  return tft.alphaBlend(alpha, BOTTOM_COLOR, TOP_COLOR);
}

void fillGradient() {
  uint16_t w = tft.width();
  for (uint16_t y = 0; y < tft.height(); ++y) {
    uint16_t color = gradientColor(0, y); // x not used here
    tft.drawFastHLine(0, y, w, color);
  }
}

void setup(void) {
  Serial.begin(115200);

  tft.begin();

  tft.setCallback(gradientColor); // Switch on color callback for anti-aliased fonts
  //tft.setCallback(nullptr);     // Switch off callback (off by default)

  tft.setRotation(1);

  if (!LittleFS.begin()) {
    Serial.println("Flash FS initialisation failed!");
    while (1) yield(); // Stay here twiddling thumbs waiting
  }
  Serial.println("\n\Flash FS available!");

  bool font_missing = false;
  if (LittleFS.exists("/NotoSansBold15.vlw") == false) font_missing = true;
  if (LittleFS.exists("/NotoSansBold36.vlw")    == false) font_missing = true;

  if (font_missing)
  {
    Serial.println("\nFont missing in Flash FS, did you upload it?");
    while (1) yield();
  }
  else Serial.println("\nFonts found OK.");
}


void loop() {

  // Select a font size commensurate with screen size
  if (tft.width()>= 320)
    tft.loadFont(AA_FONT_LARGE, LittleFS);
  else
    tft.loadFont(AA_FONT_SMALL, LittleFS);

  fillGradient(); // Put here after selecting the font so fontHeight() is already set

  tft.setTextColor(TFT_WHITE); // Background color is ignored in gradient area
  tft.setCursor(0, 10); // Set cursor at top left of screen

  uint32_t t = millis();
  tft.println(" Ode to a small\n lump of green\n putty I found\n in my armpit\n one midsummer\n morning ");
  Serial.println(t = millis()-t);

  tft.unloadFont(); // Remove the font to recover memory used

  delay(2000);
}
