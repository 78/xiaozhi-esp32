// Display grey-scale images on a Monchrome ePaper display using
// Floyd-Steinberg dithering
// https://en.wikipedia.org/wiki/Floyd%E2%80%93Steinberg_dithering

// Example created by Bodmer 31/3/18 for TFT_eSPI library:
// https://github.com/Bodmer/TFT_eSPI
// Select the ePaper setup in library's "User_Setup_Select.h" file

// This sketch supports Waveshare 2 colour ePaper displays
// https://www.waveshare.com/product/modules/oleds-lcds/e-paper.htm

// Test images are in the Data folder with sketch (press Ctrl+k)
// Upload using the Tools menu "ESP8266 Sketch Data Upload" option

///////////////////////////////////////////////////////////////////
//   For ESP8266 connect as follows:                             //
//   Display  3.3V to NodeMCU 3V3                                //
//   Display   GND to NodeMCU GND                                //
//                                                               //
//   Display   GPIO   NodeMCU pin                                //
//    BUSY       5      D1                                       //
//    RESET      4      D2                                       //
//    DC         0      D3                                       //
//    CS         2      D4                                       //
//    CLK       14      D5                                       //
//                      D6 (MISO not connected to display)       //
//    DIN       13      D7                                       //
//                                                               //
//  Note: Pin allocations for the ePaper signals are defined in  //
//  ePaper library's epdif.h file, above are the default pins    //
///////////////////////////////////////////////////////////////////

// READ THIS  READ THIS  READ THIS  READ THIS  READ THIS  READ THIS
// Install the ePaper library for your own display size and type
// from here:
// https://github.com/Bodmer/EPD_Libraries

// The following is for the Waveshare 2.7" colour ePaper display
// include <epd?in?.h>  where ?.?? is screen size in inches
#include   <epd2in7.h>                  // Screen specific library
//#include <epd2in13.h>

Epd ePaper;                             // Create an instance ePaper

#include <TFT_eSPI.h>                   // Graphics library and Sprite class

TFT_eSPI      glc = TFT_eSPI();         // Invoke the graphics library class
TFT_eSprite frame = TFT_eSprite(&glc);  // Invoke the Sprite class for the image frame buffer

#define INK    COLORED                  // Black ink
#define PAPER  UNCOLORED                // 'paper' background colour

uint16_t epd_width  = EPD_WIDTH;        // Set the initial values, these are swapped
uint16_t epd_height = EPD_HEIGHT;       // in different landscape/portrait rotations
                                        // so call frame.width() or frame.height() to get new values

#define EPD_BUFFER 1                    // Label for the black frame buffer 1

uint8_t* framePtr = NULL;               // Pointer for the black frame buffer

#include "EPD_Support.h"                // Include sketch EPD support functions last!

int8_t limit = 5;                      // Limit the number of loops before halting
//------------------------------------------------------------------------------------
// Setup
//------------------------------------------------------------------------------------
void setup() {

  Serial.begin(250000); // Used for messages

  // Initialise the ePaper library
  if (ePaper.Init(INIT_LUT) != 0) {
    Serial.print("ePaper init failed");
    while (1) yield(); // Wait here until re-boot
  }
  
  Serial.println("\r\n ePaper initialisation OK");

  // Initialise the SPIFFS filing system
  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS initialisation failed!");
    while (1) yield(); // Stay here twiddling thumbs
  }

  Serial.println(" SPIFFS initialisation OK");

  frame.setColorDepth(1); // Must set the bits per pixel to 1 for ePaper displays
                          // Set bit depth BEFORE creating Sprite, default is 16!

  // Create a frame buffer in RAM of defined size and save the pointer to it
  // RAM needed is about (EPD_WIDTH * EPD_HEIGHT)/8 , ~5000 bytes for 200 x 200 pixels
  // Note: always create the Sprite before setting the Sprite rotation
  framePtr = (uint8_t*) frame.createSprite(EPD_WIDTH, EPD_HEIGHT);

  Serial.println("\r\nInitialisation done.");

  listFiles();  // List all the files in the SPIFFS
}

//------------------------------------------------------------------------------------
// Loop
//------------------------------------------------------------------------------------
void loop() {

  frame.setRotation(random(4)); // Set the rotation to 0, 1, 2 or 3 ( 1 & 3 = landscape)

  frame.fillSprite(PAPER);

  // Draw 8-bit grey-scale bitmap using Floyd-Steinberg dithering at x,y
  //           /File name      x  y
  //drawFSBmp("/TestCard.bmp", 0, 0); // 176 x 264 pixels

  drawFSBmp("/Tiger.bmp", (frame.width()-176)/2, (frame.height()-234)/2); // 176 x 234 pixels

  updateDisplay();  // Send image to display and refresh

  delay(5000);

  frame.fillSprite(PAPER);  // Fill frame with white

  // Draw circle in frame buffer (x, y, r, color) in centre of screen
  frame.drawCircle(frame.width()/2, frame.height()/2, frame.width()/6, INK);

  // Draw diagonal lines
  frame.drawLine(0 ,                0, frame.width()-1, frame.height()-1, INK);
  frame.drawLine(0 , frame.height()-1, frame.width()-1,                0, INK);

  updateDisplay();  // Send image to display and refresh

  delay(3000);

  // Run a rotation test
  rotateTest();

  // Put screen to sleep to save power (if wanted)
  ePaper.Sleep();

  if (--limit <= 0) while(1) yield(); // Wait here

  delay(20000); // Wait here for 20s

  // Wake up ePaper display so we can talk to it
  Serial.println("Waking up!");
  ePaper.Init(INIT_LUT);

} // end of loop()


//------------------------------------------------------------------------------------
// setRotation() actually rotates the drawing coordinates, not the whole display frame
// buffer so we can use this to draw text at right angles or upside down
//------------------------------------------------------------------------------------
void rotateTest(void)
{
  //frame.fillSprite(PAPER);             // Fill buffer with white to clear old graphics

  // Draw some text in frame buffer
  frame.setTextFont(4);                  // Select font 4
  frame.setTextColor(INK);               // Set colour to ink
  frame.setTextDatum(TC_DATUM);          // Middle centre text datum

  frame.setRotation(0);                  // Set the display rotation to 0, 1, 2 or 3 ( 1 & 3 = landscape)
  epd_width  = frame.width();            // Get the values for the current rotation
  epd_height = frame.height();           // epd_height is not used in this sketch

  frame.drawString("Rotation 0",   epd_width / 2, 10);

  frame.setRotation(1);                  // Set the display rotation to 1
  epd_width  = frame.width();            // Get the values for the current rotation
  epd_height = frame.height();           // epd_height is not used in this sketch

  frame.drawString("Rotation 1",   epd_width / 2, 10);

  frame.setRotation(2);                  // Set the display rotation to 2
  epd_width  = frame.width();            // Get the values for the current rotation
  epd_height = frame.height();           // epd_height is not used in this sketch

  frame.drawString("Rotation 2",   epd_width / 2, 10);

  frame.setRotation(3);                  // Set the display rotation to 3
  epd_width  = frame.width();            // Get the values for the current rotation
  epd_height = frame.height();           // epd_height is not used in this sketch

  frame.drawString("Rotation 3",   epd_width / 2, 10);

  Serial.println("Updating display");
  updateDisplay();  // Update display
}
