/*
  Sketch to demonstrate using the print class with smooth fonts
  that are saved onto an SD Card accessed by the SD library.

  For ESP32 only, GPIO 5 must be used for SD chip select.
  This method of storing the fonts is NOT compatible with the ESP8266.

  Sketch is written for a 240 x 320 display

  Load the font file onto the root directory of the SD Card.  The font files
  used by this sketch can be found in the Data folder, press Ctrl+K to see it.

  The library supports 16-bit Unicode characters:
  https://en.wikipedia.org/wiki/Unicode_font

  The characters supported are in the in the Basic Multilingual Plane:
  https://en.wikipedia.org/wiki/Plane_(Unicode)#Basic_Multilingual_Plane

  Make sure all the display driver and pin connections are correct by
  editing the User_Setup.h file in the TFT_eSPI library folder.

  #########################################################################
  ###### DON'T FORGET TO UPDATE THE User_Setup.h FILE IN THE LIBRARY ######
  #########################################################################
*/

// Font file is stored on SD card
#include <SD.h>

// Graphics and font library
#include <TFT_eSPI.h>
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI();  // Invoke library

// -------------------------------------------------------------------------
// Setup
// -------------------------------------------------------------------------
void setup(void) {
  Serial.begin(115200); // Used for messages

  // Initialise the SD library before the TFT so the chip select is defined
  if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);

  // Initialise the TFT after the SD card!
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  listDir(SD, "/", 0);

  Serial.println("SD and TFT initialisation done.");
}

// -------------------------------------------------------------------------
// Main loop
// -------------------------------------------------------------------------
void loop() {
  // Wrap test at right and bottom of screen
  tft.setTextWrap(true, true);

  // Name of font file (library adds leading / and .vlw)
  String fileName = "Final-Frontier-28";

  // Font and background colour, background colour is used for anti-alias blending
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  // Load the font
  tft.loadFont(fileName, SD);       // Use font stored on SD

  // Display all characters of the font
  tft.showFont(2000);

 uint32_t dt = millis();

 int count = 100;

 while (count--)
 {
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
 }

 Serial.println(millis()-dt);
 
  // Unload the font to recover used RAM
  tft.unloadFont();

  delay(10000);
}

void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    Serial.printf("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        Serial.println("Failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                listDir(fs, file.name(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}
