/*
  There are four different methods of plotting anti-aliased fonts to the screen.

  This sketch uses method 3, the font characters are first plotted in a Sprite, then the
  Sprite is pushed to the screen. This method is very flexible and the Sprite can be
  created, deleted, resized as needed. To render anti-aliased fonts well the Sprite
  needs to be 16-bit.  The fonts will render in 1 bit per pixel sprites but there
  will then be no anti-aliasing. Using 1 bit per pixel Sprites is however useful
  to use the extended Unicode range in fonts on mono displays like ePaper.

  A single Sprite can be re-used for plotting different values and graphics to
  different positions on the screen. This makes this method a very powerful display tool,
  for example round buttons can be created, making use of transparent colour plotting.
  
*/
//  The fonts used are in the sketch data folder, press Ctrl+K to view.

//  Upload the fonts and icons to LittleFS (must set at least 1M for LittleFS) using the
//  "Tools"  "ESP8266 LittleFS Data Upload" menu option in the IDE.
//  To add this option follow instructions here for the ESP8266:
//  https://github.com/earlephilhower/arduino-esp8266littlefs-plugin

//  Close the IDE and open again to see the new menu option.

// A processing sketch to create new fonts can be found in the Tools folder of TFT_eSPI
// https://github.com/Bodmer/TFT_eSPI/tree/master/Tools/Create_Smooth_Font/Create_font

// This sketch uses font files created from the Noto family of fonts:
// https://www.google.com/get/noto/

#define AA_FONT_SMALL "NotoSansBold15"
#define AA_FONT_LARGE "NotoSansBold36"
#define AA_FONT_MONO  "NotoSansMonoSCB20" // NotoSansMono-SemiCondensedBold 20pt

// Font files are stored in Flash FS
#include <FS.h>
#include <LittleFS.h>
#define FlashFS LittleFS

#include <SPI.h>
#include <TFT_eSPI.h>       // Hardware-specific library

TFT_eSPI    tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft); // Sprite class needs to be invoked

void setup(void) {

  Serial.begin(250000);

  tft.begin();

  tft.setRotation(1);

  spr.setColorDepth(16); // 16-bit colour needed to show antialiased fonts

  if (!LittleFS.begin()) {
    Serial.println("Flash FS initialisation failed!");
    while (1) yield(); // Stay here twiddling thumbs waiting
  }
  Serial.println("\n\Flash FS available!");

  bool font_missing = false;
  if (LittleFS.exists("/NotoSansBold15.vlw")    == false) font_missing = true;
  if (LittleFS.exists("/NotoSansBold36.vlw")    == false) font_missing = true;
  if (LittleFS.exists("/NotoSansMonoSCB20.vlw") == false) font_missing = true;

  if (font_missing)
  {
    Serial.println("\nFont missing in Flash FS, did you upload it?");
    while(1) yield();
  }
  else Serial.println("\nFonts found OK.");
}

void loop() {

  tft.fillScreen(TFT_DARKGREY);

  int xpos = tft.width() / 2; // Half the screen width
  int ypos = 50;


  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
  // Small font
  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

  spr.loadFont(AA_FONT_SMALL, LittleFS); // Must load the font first into the sprite class

  spr.createSprite(100, 50);   // Create a sprite 100 pixels wide and 50 high

  spr.fillSprite(TFT_BLUE);

  spr.drawRect(0, 0, 100, 50, TFT_WHITE); // Draw sprite border outline (so we see extent)

  spr.setTextColor(TFT_YELLOW, TFT_DARKGREY); // Set the sprite font colour and the background colour

  spr.setTextDatum(MC_DATUM); // Middle Centre datum
  
  spr.drawString("15pt font", 50, 25 ); // Coords of middle of 100 x 50 Sprite

  spr.pushSprite(10, 10); // Push to TFT screen coord 10, 10

  spr.pushSprite(10, 70, TFT_BLUE); // Push to TFT screen, TFT_BLUE is transparent
 
  spr.unloadFont(); // Remove the font from sprite class to recover memory used

  delay(4000);

  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
  // Large font
  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

  tft.fillScreen(TFT_BLACK);

  // Beware: Sprites are a different "class" to TFT, so different fonts can be loaded
  // in the tft and sprite instances, so load the font in the class instance you use!
  // In this example this means the spr. instance.

  spr.loadFont(AA_FONT_LARGE, LittleFS); // Load another different font into the sprite instance

  // 100 x 50 sprite was created above and still exists...

  spr.fillSprite(TFT_GREEN);

  spr.setTextColor(TFT_BLACK, TFT_GREEN); // Set the font colour and the background colour

  spr.setTextDatum(MC_DATUM); // Middle Centre datum

  spr.drawString("Fits", 50, 25); // Make sure text fits in the Sprite!
  spr.pushSprite(10, 10);         // Push to TFT screen coord 10, 10

  spr.fillSprite(TFT_RED);
  spr.setTextColor(TFT_WHITE, TFT_RED); // Set the font colour and the background colour

  spr.drawString("Too big", 50, 25); // Text is too big to all fit in the Sprite!
  spr.pushSprite(10, 70);            // Push to TFT screen coord 10, 70

  // Draw changing numbers - no flicker using this plot method!

  // >>>> Note: it is best to use drawNumber() and drawFloat() for numeric values <<<<
  // >>>> this reduces digit position movement when the value changes             <<<<
  // >>>> drawNumber() and drawFloat() functions behave like drawString() and are <<<<
  // >>>> supported by setTextDatum() and setTextPadding()                        <<<<

  spr.setTextDatum(TC_DATUM); // Top Centre datum

  spr.setTextColor(TFT_WHITE, TFT_BLUE); // Set the font colour and the background colour

  for (int i = 0; i <= 200; i++) {
    spr.fillSprite(TFT_BLUE);
    spr.drawFloat(i / 100.0, 2, 50, 10); // draw with 2 decimal places at 50,10 in sprite
    spr.pushSprite(10, 130); // Push to TFT screen coord 10, 130
    delay (20);
  }

  spr.unloadFont(); // Remove the font to recover memory used

  spr.deleteSprite(); // Recover memory
  
  delay(1000);

  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
  // Mono spaced font
  // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
  
  spr.loadFont(AA_FONT_MONO, LittleFS); // Mono spaced fonts have fixed inter-character gaps to
                              // aid formatting
  int bnum = 1;

  // Example of drawing buttons
  for (int j = 0; j < 4; j++)
  {
    for (int k = 0; k < 4; k++)
    {
      int x = 120 + k * 45;
      int y = 40  + j * 30;
      button(x, y, bnum++);
    }
  }

  for (int i = 0; i < 100; i++)
  {
    button(120, 160, i);
    delay(50);
  }
  
  spr.unloadFont();

  delay(8000);
}

// #########################################################################
// Draw a number in a rounded rectangle with some transparent pixels
// Load the font before calling
// #########################################################################
void button(int x, int y, int num )
{

  // Size of sprite
  #define IWIDTH  40
  #define IHEIGHT 25

  // Create a 16-bit sprite 40 pixels wide, 25 high (2000 bytes of RAM needed)
  spr.setColorDepth(16);
  spr.createSprite(IWIDTH, IHEIGHT);

  // Fill it with black (this will be the transparent colour this time)
  spr.fillSprite(TFT_BLACK);

  // Draw a background for the numbers
  spr.fillRoundRect(  0, 0,  IWIDTH, IHEIGHT, 8, TFT_RED);
  spr.drawRoundRect(  0, 0,  IWIDTH, IHEIGHT, 8, TFT_WHITE);

  // Set the font parameters

  // Set text coordinate datum to middle centre
  spr.setTextDatum(MC_DATUM);

  // Set the font colour and the background colour
  spr.setTextColor(TFT_WHITE, TFT_RED);

  // Draw the number
  spr.drawNumber(num, IWIDTH/2, 1 + IHEIGHT/2);

  // Push sprite to TFT screen CGRAM at coordinate x,y (top left corner)
  // All black pixels will not be drawn hence will show as "transparent"
  spr.pushSprite(x, y, TFT_BLACK);

  // Delete sprite to free up the RAM
  spr.deleteSprite();
}
