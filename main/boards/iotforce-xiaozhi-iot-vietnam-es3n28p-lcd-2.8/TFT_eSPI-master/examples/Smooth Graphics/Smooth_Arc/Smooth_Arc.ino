// Example for drawSmoothArc function.
// Draws smooth arcs with rounded or square smooth ends

#include <TFT_eSPI.h>       // Include the graphics library
TFT_eSPI tft = TFT_eSPI();  // Create object "tft"

// -------------------------------------------------------------------------
// Setup
// -------------------------------------------------------------------------
void setup(void) {
  Serial.begin(115200);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
}

// -------------------------------------------------------------------------
// Main loop
// -------------------------------------------------------------------------
void loop()
{
  static uint32_t count = 0;

  uint16_t fg_color = random(0x10000);
  uint16_t bg_color = TFT_BLACK;       // This is the background colour used for smoothing (anti-aliasing)

  uint16_t x = random(tft.width());  // Position of centre of arc
  uint16_t y = random(tft.height());

  uint8_t radius       = random(20, tft.width()/4); // Outer arc radius
  uint8_t thickness    = random(1, radius / 4);     // Thickness
  uint8_t inner_radius = radius - thickness;        // Calculate inner radius (can be 0 for circle segment)

  // 0 degrees is at 6 o'clock position
  // Arcs are drawn clockwise from start_angle to end_angle
  uint16_t start_angle = random(361); // Start angle must be in range 0 to 360
  uint16_t end_angle   = random(361); // End angle must be in range 0 to 360

  bool arc_end = random(2);           // true = round ends, false = square ends (arc_end parameter can be omitted, ends will then be square)

  tft.drawSmoothArc(x, y, radius, inner_radius, start_angle, end_angle, fg_color, bg_color, arc_end);

  count++;
  if (count < 30) delay(500); // After 15s draw as fast as possible!
}
