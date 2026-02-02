/*
  There are four different methods of plotting anti-aliased fonts to the screen.

  This sketch uses method 1, using tft.print() and tft.println() calls.

  In some cases the sketch shows what can go wrong too, so read the comments!
  
  The font is rendered WITHOUT a background, but a background colour needs to be
  set so the anti-aliasing of the character is performed correctly. This is because
  characters are drawn one by one.
  
  This method is good for static text that does not change often because changing
  values may flicker. The text appears at the tft cursor coordinates.

  It is also possible to "print" text directly into a created sprite, for example using
  spr.println("Hello"); and then push the sprite to the screen. That method is not
  demonstrated in this sketch.
  
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


void setup(void) {

  Serial.begin(250000);

  tft.begin();

  tft.setRotation(0);

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

  tft.setTextColor(TFT_WHITE, TFT_BLACK); // Set the font colour AND the background colour
                                          // so the anti-aliasing works

  tft.setCursor(0, 0); // Set cursor at top left of screen


  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
  // Small font
  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

  tft.loadFont(AA_FONT_SMALL); // Must load the font first

  tft.println("Small 15pt font"); // println moves cursor down for a new line

  tft.println(); // New line

  tft.print("ABC"); // print leaves cursor at end of line

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.println("1234"); // Added to line after ABC

  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  // print stream formatting can be used,see:
  // https://www.arduino.cc/en/Serial/Print
  int ivalue = 1234;
  tft.println(ivalue);       // print as an ASCII-encoded decimal
  tft.println(ivalue, DEC);  // print as an ASCII-encoded decimal
  tft.println(ivalue, HEX);  // print as an ASCII-encoded hexadecimal
  tft.println(ivalue, OCT);  // print as an ASCII-encoded octal
  tft.println(ivalue, BIN);  // print as an ASCII-encoded binary

  tft.println(); // New line
  tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
  float fvalue = 1.23456;
  tft.println(fvalue, 0);  // no decimal places
  tft.println(fvalue, 1);  // 1 decimal place
  tft.println(fvalue, 2);  // 2 decimal places
  tft.println(fvalue, 5);  // 5 decimal places

  delay(5000);

  // Get ready for the next demo while we have this font loaded
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0); // Set cursor at top left of screen
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.println("Wrong and right ways to");
  tft.println("print changing values...");

  tft.unloadFont(); // Remove the font to recover memory used


  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
  // Large font
  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

  tft.loadFont(AA_FONT_LARGE); // Load another different font

  //tft.fillScreen(TFT_BLACK);
  
  // Draw changing numbers - does not work unless a filled rectangle is drawn over the old text
  for (int i = 0; i <= 20; i++)
  {
    tft.setCursor(50, 50);
    tft.setTextColor(TFT_GREEN, TFT_BLACK); // TFT_BLACK is used for anti-aliasing only
                                            // By default background fill is off
    tft.print("      "); // Overprinting old number with spaces DOES NOT WORK!
    tft.setCursor(50, 50);
    tft.print(i / 10.0, 1);

    // Adding a parameter "true" to the setTextColor() function fills character background
    // This extra parameter is only for smooth fonts!
    tft.setTextColor(TFT_GREEN, TFT_BLACK, true);
    tft.setCursor(50, 90);
    tft.print(i / 10.0, 1);
    
    delay (200);
  }

  delay(5000);

  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
  // Large font text wrapping
  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

  tft.fillScreen(TFT_BLACK);
  
  tft.setTextColor(TFT_YELLOW, TFT_BLACK); // Change the font colour and the background colour

  tft.setCursor(0, 0); // Set cursor at top left of screen

  tft.println("Large font!");

  tft.setTextWrap(true); // Wrap on width
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.println("Long lines wrap to the next line");

  tft.setTextWrap(false, false); // Wrap on width and height switched off
  tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
  tft.println("Unless text wrap is switched off");

  tft.unloadFont(); // Remove the font to recover memory used

  delay(8000);
}
