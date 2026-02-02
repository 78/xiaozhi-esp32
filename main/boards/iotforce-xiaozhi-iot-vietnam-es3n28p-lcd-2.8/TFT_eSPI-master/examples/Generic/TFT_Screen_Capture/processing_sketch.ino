// This is a copy of the processing sketch that can be used to capture the images
// Copy the sketch below and remove the /* and */ at the beginning and end.

// The sketch runs in Processing version 3.3 on a PC, it can be downloaded here:
// https://processing.org/download/

/*

// This is a Processing sketch, see https://processing.org/ to download the IDE

// The sketch is a client that requests TFT screenshots from an Arduino board.
// The Arduino must call a screenshot server function to respond with pixels.

// It has been created to work with the TFT_eSPI library here:
// https://github.com/Bodmer/TFT_eSPI

// The sketch must only be run when the designated serial port is available and enumerated
// otherwise the screenshot window may freeze and that process will need to be terminated
// This is a limitation of the Processing environment and not the sketch.
// If anyone knows how to determine if a serial port is available at start up the PM me
// on (Bodmer) the Arduino forum.

// The block below contains variables that the user may need to change for a particular setup
// As a minimum set the serial port and baud rate must be defined. The capture window is
// automatically resized for landscape, portrait and different TFT resolutions.

// Captured images are stored in the sketch folder, use the Processing IDE "Sketch" menu
// option "Show Sketch Folder" or press Ctrl+K

// Created by: Bodmer  5/3/17
// Updated by: Bodmer 12/3/17
// Version: 0.07

// MIT licence applies, all text above must be included in derivative works


// ###########################################################################################
// #                  These are the values to change for a particular setup                  #
//                                                                                           #
int serial_port = 0;     // Use enumerated value from list provided when sketch is run       #
//                                                                                           #
// On an Arduino Due Programming Port use a baud rate of:115200)                             #
// On an Arduino Due Native USB Port use a baud rate of any value                            #
int serial_baud_rate = 921600; //                                                            #
//                                                                                           #
// Change the image file type saved here, comment out all but one                            #
//String image_type = ".jpg"; //                                                             #
String image_type = ".png";   // Lossless compression                                        #
//String image_type = ".bmp"; //                                                             #
//String image_type = ".tif"; //                                                             #
//                                                                                           #
boolean save_border = true;   // Save the image with a border                                #
int border = 5;               // Border pixel width                                          #
boolean fade = false;         // Fade out image after saving                                 #
//                                                                                           #
int max_images = 100; // Maximum of numbered file images before over-writing files           #
//                                                                                           #
int max_allowed  = 1000; // Maximum number of save images allowed before a restart           #
//                                                                                           #
// #                   End of the values to change for a particular setup                    #
// ###########################################################################################

// These are default values, this sketch obtains the actual values from the Arduino board
int tft_width  = 480;    // default TFT width  (automatic - sent by Arduino)
int tft_height = 480;    // default TFT height (automatic - sent by Arduino)
int color_bytes = 2;     // 2 for 16-bit, 3 for three RGB bytes (automatic - sent by Arduino)

import processing.serial.*;

Serial serial;           // Create an instance called serial

int serialCount = 0;     // Count of colour bytes arriving

// Stage window graded background colours
color bgcolor1 = color(0, 100, 104);      // Arduino IDE style background color 1
color bgcolor2 = color(77, 183, 187);     // Arduino IDE style background color 2
//color bgcolor2 = color(255, 255, 255);  // White

// TFT image frame greyscale value (dark grey)
color frameColor = 42;

color buttonStopped = color(255, 0, 0);
color buttonRunning = color(128, 204, 206);
color buttonDimmed  = color(180, 0, 0);
boolean dimmed   = false;
boolean running  = true;
boolean mouseClick = false;

int[] rgb = new int[3]; // Buffer for the colour bytes
int indexRed   = 0;     // Colour byte index in the array
int indexGreen = 1;
int indexBlue  = 2;

int n = 0;

int x_offset = (500 - tft_width) /2; // Image offsets in the window
int y_offset = 20;

int xpos = 0, ypos = 0; // Current pixel position

int beginTime     = 0;
int pixelWaitTime = 1000;  // Maximum 1000ms wait for image pixels to arrive
int lastPixelTime = 0;     // Time that "image send" command was sent

int requestTime = 0;
int requestCount = 0;

int state = 0;  // State machine current state

int   progress_bar = 0; // Console progress bar dot count
int   pixel_count  = 0; // Number of pixels read for 1 screen
float percentage   = 0; // Percentage of pixels received

int  saved_image_count = 0; // Stats - number of images processed
int  bad_image_count  = 0;  // Stats - number of images that had lost pixels
String filename = "";

int drawLoopCount = 0;      // Used for the fade out

void setup() {

  size(500, 540);  // Stage size, can handle 480 pixels wide screen
  noStroke();      // No border on the next thing drawn
  noSmooth();      // No anti-aliasing to avoid adjacent pixel colour merging

  // Graded background and title
  drawWindow();

  frameRate(2000); // High frame rate so draw() loops fast

  // Print a list of the available serial ports
  println("-----------------------");
  println("Available Serial Ports:");
  println("-----------------------");
  printArray(Serial.list());
  println("-----------------------");

  print("Port currently used: [");
  print(serial_port);
  println("]");

  String portName = Serial.list()[serial_port];

  serial = new Serial(this, portName, serial_baud_rate);

  state = 99;
}

void draw() {

  if (mouseClick) buttonClicked();

  switch(state) {

  case 0: // Init varaibles, send start request
    if (running) {
      tint(0, 0, 0, 255);
      flushBuffer();
      println("");
      print("Ready: ");

      xpos = 0;
      ypos = 0;
      serialCount = 0;
      progress_bar = 0;
      pixel_count = 0;
      percentage   = 0;
      drawLoopCount = frameCount;
      lastPixelTime = millis() + 1000;

      state = 1;
    } else {
      if (millis() > beginTime) {
        beginTime = millis() + 500;
        dimmed = !dimmed;
        if (dimmed) drawButton(buttonDimmed);
        else drawButton(buttonStopped);
      }
    }
    break;

  case 1: // Console message, give server some time
    print("requesting image ");
    serial.write("S");
    delay(10);
    beginTime = millis();
    requestTime = millis() + 1000;
    requestCount = 1;
    state = 2;
    break;

  case 2: // Get size and set start time for rendering duration report
    if (millis() > requestTime) {
      requestCount++;
      print("*");
      serial.clear();
      serial.write("S");
      if (requestCount > 32) {
        requestCount = 0;
        System.err.println(" - no response!");
        state = 0;
      }
      requestTime = millis() + 1000;
    }
    if ( getSize() == true ) { // Go to next state when we have the size and bits per pixel
      getFilename();
      flushBuffer(); // Precaution in case image header size increases in later versions
      lastPixelTime = millis() + 1000;
      beginTime = millis();
      state = 3;
    }
    break;

  case 3: // Request pixels and render returned RGB values
    state = renderPixels(); // State will change when all pixels are rendered

    // Request more pixels, changing the number requested allows the average transfer rate to be controlled
    // The pixel transfer rate is dependant on four things:
    //    1. The frame rate defined in this Processing sketch in setup()
    //    2. The baud rate of the serial link (~10 bit periods per byte)
    //    3. The number of request bytes 'R' sent in the lines below
    //    4. The number of pixels sent in a burst by the server sketch (defined via NPIXELS)

    //serial.write("RRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRR"); // 32 x NPIXELS more
    serial.write("RRRRRRRRRRRRRRRR"); // 16 x NPIXELS more
    //serial.write("RRRRRRRR"); // 8 x NPIXELS more
    //serial.write("RRRR"); // 4 x NPIXELS more
    //serial.write("RR"); // 2 x NPIXELS more
    //serial.write("R"); // 1 x NPIXELS more
    if (!running) state = 4;
    break;

  case 4: // Pixel receive time-out, flush serial buffer
    flushBuffer();
    state = 6;
    break;

  case 5: // Save the image to the sketch folder (Ctrl+K to access)
    saveScreenshot();
    saved_image_count++;
    println("Saved image count = " + saved_image_count);
    if (bad_image_count > 0) System.err.println(" Bad image count = " + bad_image_count);
    drawLoopCount = frameCount; // Reset value ready for counting in step 6
    state = 6;
    break;

  case 6: // Fade the old image if enabled
    if ( fadedImage() == true ) state = 0; // Go to next state when image has faded
    break;

  case 99: // Draw image viewer window
    drawWindow();
    delay(50); // Delay here seems to be required for the IDE console to get ready
    state = 0;
    break;

  default:
    println("");
    System.err.println("Error state reached - check sketch!");
    break;
  }
}

void drawWindow()
{
  // Graded background in Arduino colours
  for (int i = 0; i < height - 25; i++) {
    float inter = map(i, 0, height - 25, 0, 1);
    color c = lerpColor(bgcolor1, bgcolor2, inter);
    stroke(c);
    line(0, i, 500, i);
  }
  fill(bgcolor2);
  rect( 0, height-25, width-1, 24);
  textAlign(CENTER);
  textSize(20);
  fill(0);
  text("Bodmer's TFT image viewer", width/2, height-6);

  if (running) drawButton(buttonRunning);
  else drawButton(buttonStopped);
}

void flushBuffer()
{
  //println("Clearing serial pipe after a time-out");
  int clearTime = millis() + 50;
  while ( millis() < clearTime ) serial.clear();
}

boolean getSize()
{
  if ( serial.available() > 6 ) {
    println();
    char code = (char)serial.read();
    if (code == 'W') {
      tft_width = serial.read()<<8 | serial.read();
    }
    code = (char)serial.read();
    if (code == 'H') {
      tft_height = serial.read()<<8 | serial.read();
    }
    code = (char)serial.read();
    if (code == 'Y') {
      int bits_per_pixel = (char)serial.read();
      if (bits_per_pixel == 24) color_bytes = 3;
      else color_bytes = 2;
    }
    code = (char)serial.read();
    if (code == '?') {
      drawWindow();

      x_offset = (500 - tft_width) /2;
      tint(0, 0, 0, 255);
      noStroke();
      fill(frameColor);
      rect((width - tft_width)/2 - border, y_offset - border, tft_width + 2 * border, tft_height + 2 * border);
      return true;
    }
  }
  return false;
}

void saveScreenshot()
{
  println();
  if (saved_image_count < max_allowed)
  {
  if (filename == "") filename = "tft_screen_" + (n++);
  filename = filename  + image_type;
  println("Saving image as \"" + filename + "\"");
  if (save_border)
  {
    PImage partialSave = get(x_offset - border, y_offset - border, tft_width + 2*border, tft_height + 2*border);
    partialSave.save(filename);
  } else {
    PImage partialSave = get(x_offset, y_offset, tft_width, tft_height);
    partialSave.save(filename);
  }

  if (n>=max_images) n = 0;
  }
  else
  {
    System.err.println(max_allowed + " saved image count exceeded, restart the sketch");
  }
}

void getFilename()
{
  int readTime = millis() + 20;
  int inByte = 0;
  filename = "";
  while ( serial.available() > 0 && millis() < readTime && inByte != '.')
  {
    inByte = serial.read();
    if (inByte == ' ') inByte = '_';
    if ( unicodeCheck(inByte) ) filename += (char)inByte;
  }

  inByte = serial.read();
       if (inByte == '@') filename += "_" + timeCode();
  else if (inByte == '#') filename += "_" + saved_image_count%100;
  else if (inByte == '%') filename += "_" + millis();
  else if (inByte != '*') filename  = "";

  inByte = serial.read();
       if (inByte == 'j') image_type =".jpg";
  else if (inByte == 'b') image_type =".bmp";
  else if (inByte == 'p') image_type =".png";
  else if (inByte == 't') image_type =".tif";
}

boolean unicodeCheck(int unicode)
{
  if (  unicode >= '0' && unicode <= '9' ) return true;
  if ( (unicode >= 'A' && unicode <= 'Z' ) || (unicode >= 'a' && unicode <= 'z')) return true;
  if (  unicode == '_' || unicode == '/' ) return true;
  return false;
}

String timeCode()
{
 String timeCode  = (int)year() + "_" + (int)month()  + "_" + (int)day() + "_";
        timeCode += (int)hour() + "_" + (int)minute() + "_" + (int)second(); 
 return timeCode;
}

int renderPixels()
{
  if ( serial.available() > 0 ) {

    // Add the latest byte from the serial port to array:
    while (serial.available()>0)
    {
      rgb[serialCount++] = serial.read();

      // If we have 3 colour bytes:
      if ( serialCount >= color_bytes ) {
        serialCount = 0;
        pixel_count++;
        if (color_bytes == 3)
        {
          stroke(rgb[indexRed], rgb[indexGreen], rgb[indexBlue], 1000);
        } else
        { // Can cater for various byte orders
          //stroke( (rgb[0] & 0x1F)<<3, (rgb[0] & 0xE0)>>3 | (rgb[1] & 0x07)<<5, (rgb[1] & 0xF8));
          //stroke( (rgb[1] & 0x1F)<<3, (rgb[1] & 0xE0)>>3 | (rgb[0] & 0x07)<<5, (rgb[0] & 0xF8));
          stroke( (rgb[0] & 0xF8), (rgb[1] & 0xE0)>>3 | (rgb[0] & 0x07)<<5, (rgb[1] & 0x1F)<<3);
          //stroke( (rgb[1] & 0xF8), (rgb[0] & 0xE0)>>3 | (rgb[1] & 0x07)<<5, (rgb[0] & 0x1F)<<3);
        }
        // We get some pixel merge aliasing if smooth() is defined, so draw pixel twice
        point(xpos + x_offset, ypos + y_offset);
        //point(xpos + x_offset, ypos + y_offset);

        lastPixelTime = millis();
        xpos++;
        if (xpos >= tft_width) {
          xpos = 0; 
          progressBar();
          ypos++;
          if (ypos>=tft_height) {
            ypos = 0;
            if ((int)percentage <100) {
              while (progress_bar++ < 64) print(" ");
              percent(100);
            }
            println("Image fetch time = " + (millis()-beginTime)/1000.0 + " s");
            return 5;
          }
        }
      }
    }
  } else
  {
    if (millis() > (lastPixelTime + pixelWaitTime))
    {
      println("");
      System.err.println(pixelWaitTime + "ms time-out for pixels exceeded...");
      if (pixel_count > 0) {
        bad_image_count++;
        System.err.print("Pixels missing = " + (tft_width * tft_height - pixel_count));
        System.err.println(", corrupted image not saved");
        System.err.println("Good image count = " + saved_image_count);
        System.err.println(" Bad image count = " + bad_image_count);
      }
      return 4;
    }
  }
  return 3;
}

void progressBar()
{
  progress_bar++;
  print(".");
  if (progress_bar >63)
  {
    progress_bar = 0;
    percentage = 0.5 + 100 * pixel_count/(0.001 + tft_width * tft_height);
    percent(percentage);
  }
}

void percent(float percentage)
{
  if (percentage > 100) percentage = 100;
  println(" [ " + (int)percentage + "% ]");
  textAlign(LEFT);
  textSize(16);
  noStroke();
  fill(bgcolor2);
  rect(10, height - 25, 70, 20);
  fill(0);
  text(" [ " + (int)percentage + "% ]", 10, height-8);
}

boolean fadedImage()
{
  int opacity = frameCount - drawLoopCount;  // So we get increasing fade
  if (fade)
  {
    tint(255, opacity);
    //image(tft_img, x_offset, y_offset);
    noStroke();
    fill(50, 50, 50, opacity);
    rect( (width - tft_width)/2, y_offset, tft_width, tft_height);
    delay(10);
  }
  if (opacity > 50)       // End fade after 50 cycles
  {
    return true;
  }
  return false;
}

void drawButton(color buttonColor)
{
  stroke(0);
  fill(buttonColor);
  rect(500 - 100, 540 - 26, 80, 24);
  textAlign(CENTER);
  textSize(20);
  fill(0);
  if (running) text(" Pause ", 500 - 60, height-7);
  else text(" Run ", 500 - 60, height-7);
}

void buttonClicked()
{
  mouseClick = false;
  if (running) {
    running = false;
    drawButton(buttonStopped);
    System.err.println("");
    System.err.println("Stopped - click 'Run' button: ");
    //noStroke();
    //fill(50);
    //rect( (width - tft_width)/2, y_offset, tft_width, tft_height);
    beginTime = millis() + 500;
    dimmed = false;
    state = 4;
  } else {
    running = true;
    drawButton(buttonRunning);
  }
}

void mousePressed() {
  if (mouseX > (500 - 100) && mouseX < (500 - 20) && mouseY > (540 - 26) && mouseY < (540 - 2)) {
    mouseClick = true;
  }
}

*/
