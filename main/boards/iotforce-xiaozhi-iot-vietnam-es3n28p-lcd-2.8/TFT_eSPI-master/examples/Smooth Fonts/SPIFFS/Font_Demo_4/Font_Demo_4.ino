/*
  There are four different methods of plotting anti-aliased fonts to the screen.

  This sketch uses method 4, printing "String" or character array types only to screen,
  via a Sprite. The Sprite must NOT have been created already. The printToSprite()
  function automatically creates a sprite of a minimal size to contain the String,
  then plots to screen at the "tft" cursor position. Printing via a sprite draws the
  text faster on the screen. This method minimises flicker but uses RAM for the Sprite,
  the Sprite is automatically deleted after plotting to the TFT.

  Number and float types must be converted to strings to use printToSprite() e.g.:
  spr.printToSprite( (String) number );
  spr.printToSprite( (String) (number * 55 / 1.23) );  // Put calculations within brackets

  The key advantage of this method is that you do not need to calculate the size of sprite
  needed to contain the text, the library does that for you. The library also fills the
  the sprite with text background colour for you.

  printToSprite() has a second purpose, if the sprite has been created already the String
  will be printed into the Sprite at the "sprite" cursor position, which is
  different to the "tft" cursor position. In this case the Sprite is not deleted and
  you must use pushSprite() to plot on the screen. This method is not used in this sketch.
  because in general it is better to use drawString() in an already created sprite.
  printToSprite() will NOT move the tft cursor.

*/
//  The fonts used are in the sketch data folder, press Ctrl+K to view.

//  Upload the fonts and icons to SPIFFS (must set at least 1M for SPIFFS) using the
//  "Tools"  "ESP8266 (or ESP32) Sketch Data Upload" menu option in the IDE.
//  To add this option follow instructions here for the ESP8266:
//  https://github.com/esp8266/arduino-esp8266fs-plugin
//  or for the ESP32:
//  https://github.com/me-no-dev/arduino-esp32fs-plugin

//  Close the IDE and open again to see the new menu option.

// A processing sketch to create new fonts can be found in the Tools folder of TFT_eSPI
// https://github.com/Bodmer/TFT_eSPI/tree/master/Tools/Create_Smooth_Font/Create_font

// This sketch uses font files created from the Noto family of fonts:
// https://www.google.com/get/noto/

#define AA_FONT_SMALL "NotoSansBold15"
#define AA_FONT_LARGE "NotoSansBold36"

// Font files are stored in SPIFFS, so load the library
#include <FS.h>

#include <SPI.h>
#include <TFT_eSPI.h>       // Hardware-specific library

TFT_eSPI    tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft); // Sprite class needs to be invoked

void setup(void) {

  Serial.begin(250000);

  tft.begin();

  tft.setRotation(1);

  spr.setColorDepth(16); // 16-bit colour needed to show anti-aliased fonts

  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS initialisation failed!");
    while (1) yield(); // Stay here twiddling thumbs waiting
  }
  Serial.println("\r\nSPIFFS available!");

  // ESP32 will crash if any of the fonts are missing
  bool font_missing = false;
  if (SPIFFS.exists("/NotoSansBold15.vlw")    == false) font_missing = true;
  if (SPIFFS.exists("/NotoSansBold36.vlw")    == false) font_missing = true;

  if (font_missing)
  {
    Serial.println("\r\nFont missing in SPIFFS, did you upload it?");
    while(1) yield();
  }
  else Serial.println("\r\nFonts found OK.");
}

void loop() {

  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_WHITE, TFT_BLACK); // Set the font colour and the background colour

  tft.setTextDatum(TC_DATUM); // Top Centre datum

  int xpos = tft.width() / 2; // Half the screen width
  int ypos = 50;


  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
  // Small font
  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

  spr.loadFont(AA_FONT_SMALL); // Must load the font first into the sprite class
  
  spr.setTextColor(TFT_YELLOW, TFT_BLACK); // Set the sprite font colour and the background colour

  tft.setCursor(xpos - 50, ypos);          // Set the tft cursor position, yes tft position!
  spr.printToSprite("Small 15pt font");    // Prints to tft cursor position, tft cursor NOT moved

  ypos += spr.fontHeight();   // Get the font height and move ypos down

  spr.unloadFont(); // Remove the font from sprite class to recover memory used

  delay(4000);

  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
  // Large font
  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

  tft.fillScreen(TFT_BLACK);

  spr.loadFont(AA_FONT_LARGE); // Load another different font

  spr.setTextColor(TFT_WHITE, TFT_BLUE); // Set the font colour and the background colour

  tft.setCursor(xpos - 90, ypos);  // Set the tft cursor position
  spr.printToSprite("36pt font");  // Text is rendered via a minimally sized sprite

  ypos += spr.fontHeight();  // Get the font height and move ypos down

  // Draw changing numbers - no flicker using this plot method!
  for (int i = 0; i <= 200; i++) {
    tft.setCursor(10, 10);
    // Number is converted to String type by (String) (number)
    spr.printToSprite(" " + (String) (i / 100.0) + " "); // Space padding helps over-write old numbers
    delay (20);
  }

  spr.unloadFont(); // Remove the font to recover memory used

  delay(8000);
}
