/*
  Sketch to demonstrate using the print class with smooth fonts,
  the Smooth fonts are stored in a FLASH program memory array.

  Sketch is written for a 240 x 320 display

  New font files in the .vlw format can be created using the Processing
  sketch in the library Tools folder. The Processing sketch can convert
  TrueType fonts in *.ttf or *.otf files.

  The library supports 16-bit Unicode characters:
  https://en.wikipedia.org/wiki/Unicode_font

  The characters supported are in the in the Basic Multilingual Plane:
  https://en.wikipedia.org/wiki/Plane_(Unicode)#Basic_Multilingual_Plane

  Make sure all the display driver and pin connections are correct by
  editing the User_Setup.h file in the TFT_eSPI library folder.
*/

//  The font is stored in an array within a sketch tab.

//  A processing sketch to create new fonts can be found in the Tools folder of TFT_eSPI
//  https://github.com/Bodmer/TFT_eSPI/tree/master/Tools/Create_Smooth_Font/Create_font

#include "Final_Frontier_28.h"

// Graphics and font library
#include <TFT_eSPI.h>
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI();  // Invoke library

// -------------------------------------------------------------------------
// Setup
// -------------------------------------------------------------------------
void setup(void) {
  Serial.begin(115200); // Used for messages

  tft.init();
  tft.setRotation(1);
}

// -------------------------------------------------------------------------
// Main loop
// -------------------------------------------------------------------------
void loop() {
  // Wrap test at right and bottom of screen
  tft.setTextWrap(true, true);

  // Font and background colour, background colour is used for anti-alias blending
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  // Load the font
  tft.loadFont(Final_Frontier_28);

  // Display all characters of the font
  tft.showFont(2000);

  // Set "cursor" at top left corner of display (0,0)
  // (cursor will move to next line automatically during printing with 'tft.println'
  //  or stay on the line is there is room for the text with tft.print)
  tft.setCursor(0, 0);

  // Set the font colour to be white with a black background, set text size multiplier to 1
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  // We can now plot text on screen using the "print" class
  tft.println("Hello World!");

  // Set the font colour to be yellow
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.println(1234.56);

  // Set the font colour to be red
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.println((uint32_t)3735928559, HEX); // Should print DEADBEEF

  // Set the font colour to be green with black background
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.println("Anti-aliased font!");
  tft.println("");

  // Test some print formatting functions
  float fnumber = 123.45;

  // Set the font colour to be blue
  tft.setTextColor(TFT_BLUE, TFT_BLACK);
  tft.print("Float = ");       tft.println(fnumber);           // Print floating point number
  tft.print("Binary = ");      tft.println((int)fnumber, BIN); // Print as integer value in binary
  tft.print("Hexadecimal = "); tft.println((int)fnumber, HEX); // Print as integer number in Hexadecimal

  // Unload the font to recover used RAM
  tft.unloadFont();

  delay(10000);
}


// -------------------------------------------------------------------------
// List files in ESP8266 or ESP32 SPIFFS memory
// -------------------------------------------------------------------------
void listFiles(void) {
  Serial.println();
  Serial.println("SPIFFS files found:");

#ifdef ESP32
  listDir(SPIFFS, "/", true);
#else
  fs::Dir dir = SPIFFS.openDir("/"); // Root directory
  String  line = "=====================================";

  Serial.println(line);
  Serial.println("  File name               Size");
  Serial.println(line);

  while (dir.next()) {
    String fileName = dir.fileName();
    Serial.print(fileName);
    int spaces = 25 - fileName.length(); // Tabulate nicely
    if (spaces < 0) spaces = 1;
    while (spaces--) Serial.print(" ");
    fs::File f = dir.openFile("r");
    Serial.print(f.size()); Serial.println(" bytes");
    yield();
  }

  Serial.println(line);
#endif
  Serial.println();
  delay(1000);
}

#ifdef ESP32
void listDir(fs::FS &fs, const char * dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\n", dirname);

  fs::File root = fs.open(dirname);
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return;
  }

  fs::File file = root.openNextFile();
  while (file) {

    if (file.isDirectory()) {
      Serial.print("DIR : ");
      String fileName = file.name();
      Serial.print(fileName);
      if (levels) {
        listDir(fs, file.name(), levels - 1);
      }
    } else {
      String fileName = file.name();
      Serial.print("  " + fileName);
      int spaces = 32 - fileName.length(); // Tabulate nicely
      if (spaces < 1) spaces = 1;
      while (spaces--) Serial.print(" ");
      String fileSize = (String) file.size();
      spaces = 8 - fileSize.length(); // Tabulate nicely
      if (spaces < 1) spaces = 1;
      while (spaces--) Serial.print(" ");
      Serial.println(fileSize + " bytes");
    }

    file = root.openNextFile();
  }
}
#endif
// -------------------------------------------------------------------------
