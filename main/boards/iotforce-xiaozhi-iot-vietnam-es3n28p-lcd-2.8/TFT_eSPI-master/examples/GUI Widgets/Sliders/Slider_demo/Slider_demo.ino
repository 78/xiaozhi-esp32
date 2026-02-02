// Slider widget demo, requires display with touch screen

// Requires widget library here:
// https://github.com/Bodmer/TFT_eWidget

#include "FS.h"

#include "Free_Fonts.h" // Include the header file attached to this sketch

#include <TFT_eSPI.h>
#include <TFT_eWidget.h>           // Widget library

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite knob = TFT_eSprite(&tft); // Sprite for the slide knob

#define CALIBRATION_FILE "/TouchCalData1"
#define REPEAT_CAL false

SliderWidget s1 = SliderWidget(&tft, &knob);    // Slider 1 widget
SliderWidget s2 = SliderWidget(&tft, &knob);    // Slider 2 widget


void setup() {
  Serial.begin(115200);
  tft.begin();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  tft.setFreeFont(FF18);

  // Calibrate the touch screen and retrieve the scaling factors
  if (REPEAT_CAL) {
    touch_calibrate();
    tft.fillScreen(TFT_BLACK);
  }

  // Create a parameter set for the slider
  slider_t param;

  // Slider slot parameters
  param.slotWidth = 9;           // Note: ends of slot will be rounded and anti-aliased
  param.slotLength = 200;        // Length includes rounded ends
  param.slotColor = TFT_BLUE;    // Slot colour
  param.slotBgColor = TFT_BLACK; // Slot background colour for anti-aliasing
  param.orientation = H_SLIDER;  // sets it "true" for horizontal

  // Slider control knob parameters (smooth rounded rectangle)
  param.knobWidth = 15;          // Always along x axis
  param.knobHeight = 25;         // Always along y axis
  param.knobRadius = 5;          // Corner radius
  param.knobColor = TFT_WHITE;   // Anti-aliased with slot backgound colour
  param.knobLineColor = TFT_RED; // Colour of marker line (set to same as knobColor for no line)

  // Slider range and movement speed
  param.sliderLT = 0;            // Left side for horizontal, top for vertical slider
  param.sliderRB = 100;          // Right side for horizontal, bottom for vertical slider
  param.startPosition = 50;      // Start position for control knob
  param.sliderDelay = 0;         // Microseconds per pixel movement delay (0 = no delay)

  // Create slider using parameters and plot at 0,0
  s1.drawSlider(0, 0, param);

  // Show bounding box (1 pixel outside slider working area)
  int16_t x, y;    // x and y can be negative
  uint16_t w, h;   // Width and height
  s1.getBoundingRect(&x, &y, &w, &h);     // Update x,y,w,h with bounding box
  tft.drawRect(x, y, w, h, TFT_DARKGREY); // Draw rectangle outline
/*
  // Alternative discrete fns to create/modify same slider - but fn sequence is important...
  s1.createSlider(9, 200, TFT_BLUE, TFT_BLACK, H_SLIDER);
  s1.createKnob(15, 25, 5, TFT_WHITE, TFT_RED);
  s1.setSliderScale(0, 100);
  s1.drawSlider(0, 0);
*/
  delay(1000);
  s1.setSliderPosition(50);
  delay(1000);
  s1.setSliderPosition(100);

  // Update any parameters that are different for slider 2
  param.slotWidth = 4;
  param.orientation = V_SLIDER; // sets it "false" for vertical

  param.knobWidth = 19;
  param.knobHeight = 19;
  param.knobRadius = 19/2; // Half w and h so creates a circle

  param.sliderLT = 200;     // Top for vertical slider
  param.sliderRB = 0;       // Bottom for vertical slider
  param.sliderDelay = 2000; // 2ms per pixel movement delay (movement is blocking until complete)

  s2.drawSlider(0, 50, param);

  s2.getBoundingRect(&x, &y, &w, &h);
  tft.drawRect(x, y, w, h, TFT_DARKGREY);
/*
  // Alternative discrete fns to create/modify same slider - but fn sequence is important...
  s2.createSlider(4, 200, TFT_BLUE, TFT_BLACK, V_SLIDER);
  s2.createKnob(19, 19, 9, TFT_WHITE, TFT_RED);
  s2.setSliderScale(200, 0, 2000);
  s2.drawSlider(0, 50);
*/
  // Move slider under software control
  delay(1000);
  s2.setSliderPosition(50);
  delay(1000);
  s2.setSliderPosition(100);

}

void loop() {
  static uint32_t scanTime = millis();
  uint16_t t_x = 9999, t_y = 9999; // To store the touch coordinates

  // Scan for touch every 50ms
  if (millis() - scanTime >= 20) {
    // Pressed will be set true if there is a valid touch on the screen
    if( tft.getTouch(&t_x, &t_y, 250) ) {
      if (s1.checkTouch(t_x, t_y)) {
        Serial.print("Slider 1 = "); Serial.println(s1.getSliderPosition());
      }
      if (s2.checkTouch(t_x, t_y)) {
        Serial.print("Slider 2 = "); Serial.println(s2.getSliderPosition());
      }
    }
    scanTime = millis();
  }

  //s1.moveTo(random(101));
  //delay(250);
  //s2.moveTo(random(101));
  //delay(250);
}





void touch_calibrate()
{
  uint16_t calData[5];
  uint8_t calDataOK = 0;

  // check file system exists
  if (!LittleFS.begin()) {
    Serial.println("formatting file system");
    LittleFS.format();
    LittleFS.begin();
  }

  // check if calibration file exists and size is correct
  if (LittleFS.exists(CALIBRATION_FILE)) {
    if (REPEAT_CAL)
    {
      // Delete if we want to re-calibrate
      LittleFS.remove(CALIBRATION_FILE);
    }
    else
    {
      File f = LittleFS.open(CALIBRATION_FILE, "r");
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
    File f = LittleFS.open(CALIBRATION_FILE, "w");
    if (f) {
      f.write((const unsigned char *)calData, 14);
      f.close();
    }
  }
}
