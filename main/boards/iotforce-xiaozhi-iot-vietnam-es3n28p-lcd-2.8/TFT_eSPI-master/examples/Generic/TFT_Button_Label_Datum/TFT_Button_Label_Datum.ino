/*
  The TFT_eSPI library incorporates an Adafruit_GFX compatible
  button handling class.

  This example displays a column of buttons with varying label
  alignments.

  The sketch has been tested on the ESP32 (which supports SPIFFS)

  Adjust the definitions below according to your screen size
*/

#include "FS.h"

#include <SPI.h>

#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

// This is the file name used to store the calibration data
// You can change this to create new calibration files.
// The SPIFFS file name must start with "/".
#define CALIBRATION_FILE "/TouchCalData1"

// Set REPEAT_CAL to true instead of false to run calibration
// again, otherwise it will only be done once.
// Repeat calibration if you change the screen rotation.
#define REPEAT_CAL false

// Keypad start position, key sizes and spacing
#define KEY_X 160 // Centre of key
#define KEY_Y 50
#define KEY_W 320 // Width and height
#define KEY_H 22
#define KEY_SPACING_X 0 // X and Y gap
#define KEY_SPACING_Y 1
#define KEY_TEXTSIZE 1   // Font size multiplier
#define BUTTON_X_DELTA 22
#define NUM_KEYS 6

TFT_eSPI_Button key[NUM_KEYS];

void setup() {
  
  Serial.begin(115200);
  
  tft.init();

  // Set the rotation before we calibrate
  tft.setRotation(1);

  // Check for backlight pin if not connected to VCC
  #ifndef TFT_BL
    Serial.println("No TFT backlight pin defined");
  #else
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
  #endif

  // call screen calibration
  touch_calibrate();

  // Clear screen
  tft.fillScreen(TFT_BLACK);

  tft.setFreeFont(&FreeMono9pt7b);
  
  drawButtons();
}

void loop() {
  uint16_t t_x = 0, t_y = 0; // To store the touch coordinates

  // Get current touch state and coordinates
  bool pressed = tft.getTouch(&t_x, &t_y);

  // Adjust press state of each key appropriately
  for (uint8_t b = 0; b < NUM_KEYS; b++) {
    if (pressed && key[b].contains(t_x, t_y)) 
      key[b].press(true);  // tell the button it is pressed
    else
      key[b].press(false);  // tell the button it is NOT pressed
  }

  // Check if any key has changed state
  for (uint8_t b = 0; b < NUM_KEYS; b++) {
    // If button was just pressed, redraw inverted button
    if (key[b].justPressed()) {
      Serial.println("Button " + (String)b + " pressed");
      key[b].drawButton(true, "ML_DATUM + " + (String)(b * 10) + "px");
    }

    // If button was just released, redraw normal color button
    if (key[b].justReleased()) {
      Serial.println("Button " + (String)b + " released");
      key[b].drawButton(false, "ML_DATUM + " + (String)(b * 10) + "px");
    }
  }

}

void drawButtons()
{
  // Generate buttons with different size X deltas
  for (int i = 0; i < NUM_KEYS; i++)
  {
    key[i].initButton(&tft,
                      KEY_X + 0 * (KEY_W + KEY_SPACING_X),
                      KEY_Y + i * (KEY_H + KEY_SPACING_Y), // x, y, w, h, outline, fill, text
                      KEY_W,
                      KEY_H,
                      TFT_BLACK, // Outline
                      TFT_CYAN, // Fill
                      TFT_BLACK, // Text
                      "", // 10 Byte Label
                      KEY_TEXTSIZE);

    // Adjust button label X delta according to array position
    // setLabelDatum(uint16_t x_delta, uint16_t y_delta, uint8_t datum)
    key[i].setLabelDatum(i * 10 - (KEY_W/2), 0, ML_DATUM);

    // Draw button and specify label string
    // Specifying label string here will allow more than the default 10 byte label
    key[i].drawButton(false, "ML_DATUM + " + (String)(i * 10) + "px");
  }
}

void touch_calibrate()
{
  uint16_t calData[5];
  uint8_t calDataOK = 0;

  // check file system exists
  if (!SPIFFS.begin()) {
    Serial.println("Formatting file system");
    SPIFFS.format();
    SPIFFS.begin();
  }

  // check if calibration file exists and size is correct
  if (SPIFFS.exists(CALIBRATION_FILE)) {
    if (REPEAT_CAL)
    {
      // Delete if we want to re-calibrate
      SPIFFS.remove(CALIBRATION_FILE);
    }
    else
    {
      File f = SPIFFS.open(CALIBRATION_FILE, "r");
      if (f) {
        if (f.readBytes((char *)calData, 14) == 14)
          calDataOK = 1;
        f.close();
      }
    }
  }

  if (calDataOK && !REPEAT_CAL) {
    // calibration data valid
    tft.setTouch(calData);
  } else {
    // data not valid so recalibrate
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(20, 0);
    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    tft.println("Touch corners as indicated");

    tft.setTextFont(1);
    tft.println();

    if (REPEAT_CAL) {
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.println("Set REPEAT_CAL to false to stop this running again!");
    }

    tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("Calibration complete!");

    // store data
    File f = SPIFFS.open(CALIBRATION_FILE, "w");
    if (f) {
      f.write((const unsigned char *)calData, 14);
      f.close();
    }
  }
}
