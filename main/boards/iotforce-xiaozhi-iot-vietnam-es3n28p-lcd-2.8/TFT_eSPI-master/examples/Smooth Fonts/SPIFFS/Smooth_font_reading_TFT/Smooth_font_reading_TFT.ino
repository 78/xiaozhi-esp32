/*
  This sketch is based on Font Demo 1. It introduces a method for rendering
  anti-aliased fonts on an arbitrary background. This is achieved by reading
  the pixel color at each point on the screen. The TFT must support reading
  the graphics RAM of the screen memory. This sketch has been tested with
  ILI9241 and ILI9481 serial and parallel screens. Other screens may or may
  not work!

  The TFT_eSPI library must be given the name of the function in the sketch
  that will return the pixel color at a position x,y on the TFT. In this
  sketch that function is called pixelColor, so this line is included:

    tft.setCallback(pixelColor);

  TFT_eSPI will call this function during the rendering of the anti-aliased
  font and use it to blend the edges of each character with the screen color.
*/
//  The fonts used are in the sketch data folder, press Ctrl+K to view.

//  Upload the fonts and icons to SPIFFS (must set at least 1M for SPIFFS) using the
//  "Tools"  "ESP8266 (or ESP32) Sketch Data Upload" menu option in the IDE.
//  To add this option follow instructions here for the ESP8266:
//  https://github.com/esp8266/arduino-esp8266fs-plugin
//  or for the ESP32:
//  https://github.com/me-no-dev/arduino-esp32fs-plugin

//  Close the IDE and open again to see the new menu option.

//  A processing sketch to create new fonts can be found in the Tools folder of TFT_eSPI
//  https://github.com/Bodmer/TFT_eSPI/tree/master/Tools/Create_Smooth_Font/Create_font

//  This sketch uses font files created from the Noto family of fonts:
//  https://www.google.com/get/noto/

#define AA_FONT_SMALL "NotoSansBold15"
#define AA_FONT_LARGE "NotoSansBold36"

// Font files are stored in SPIFFS, so load the library
#include <FS.h>

#include <SPI.h>
#include <TFT_eSPI.h>       // Hardware-specific library

TFT_eSPI tft = TFT_eSPI();

// Callback function to provide the pixel color at x,y
uint16_t pixelColor(uint16_t x, uint16_t y) { return tft.readPixel(x, y); }


void setup(void) {
  Serial.begin(115200);

  tft.begin();

  tft.setCallback(pixelColor);  // The callback is only used during font rendering
  //tft.setCallback(nullptr);   // Switch off callback (off by default)

  tft.setRotation(1);

  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS initialisation failed!");
    while (1) yield(); // Stay here twiddling thumbs waiting
  }
  Serial.println("\r\nSPIFFS available!");

  // ESP32 will crash if any of the fonts are missing, so check
  bool font_missing = false;
  if (SPIFFS.exists("/NotoSansBold15.vlw") == false) font_missing = true;
  if (SPIFFS.exists("/NotoSansBold36.vlw") == false) font_missing = true;

  if (font_missing)
  {
    Serial.println("\r\nFont missing in SPIFFS, did you upload it?");
    while (1) yield();
  }
  else Serial.println("\r\nFonts found OK.");
}


void loop() {

  rainbow_fill(); // Fill the screen with rainbow colours

  // Select a font size commensurate with screen size
  if (tft.width()>= 320)
    tft.loadFont(AA_FONT_LARGE);
  else
    tft.loadFont(AA_FONT_SMALL);

  tft.setTextColor(TFT_BLACK, TFT_WHITE); // Background color is ignored if callback is set
  tft.setCursor(0, 10); // Set cursor at top left of screen

  uint32_t t = millis();
  tft.println(" Ode to a small\n lump of green\n putty I found\n in my armpit\n one midsummer\n morning ");
  Serial.println(t = millis()-t);

  tft.unloadFont(); // Remove the font to recover memory used

  delay(2000);
}

// #########################################################################
// Fill screen with a rainbow pattern
// #########################################################################
byte red = 31;
byte green = 0;
byte blue = 0;
byte state = 0;
unsigned int colour = red << 11; // Colour order is RGB 5+6+5 bits each

void rainbow_fill()
{
  // The colours and state are not initialised so the start colour changes each time the function is called
  
  for (int i = 319; i >= 0; i--) {
    // Draw a vertical line 1 pixel wide in the selected colour
    tft.drawFastHLine(0, i, tft.width(), colour); // in this example tft.width() returns the pixel width of the display
    // This is a "state machine" that ramps up/down the colour brightnesses in sequence
    switch (state) {
      case 0:
        green ++;
        if (green == 64) {
          green = 63;
          state = 1;
        }
        break;
      case 1:
        red--;
        if (red == 255) {
          red = 0;
          state = 2;
        }
        break;
      case 2:
        blue ++;
        if (blue == 32) {
          blue = 31;
          state = 3;
        }
        break;
      case 3:
        green --;
        if (green == 255) {
          green = 0;
          state = 4;
        }
        break;
      case 4:
        red ++;
        if (red == 32) {
          red = 31;
          state = 5;
        }
        break;
      case 5:
        blue --;
        if (blue == 255) {
          blue = 0;
          state = 0;
        }
        break;
    }
    colour = red << 11 | green << 5 | blue;
  }
}
