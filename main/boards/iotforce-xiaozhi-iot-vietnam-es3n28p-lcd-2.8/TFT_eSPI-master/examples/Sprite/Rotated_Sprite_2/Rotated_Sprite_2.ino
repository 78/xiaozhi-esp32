// This example plots a rotated Sprite into another Sprite and then the resultant composited
// Sprite is pushed to the TFT screen. This example is for a 240 x 320 screen.

// The motivation for developing this capability is that animated dials can be drawn easily
// and the complex calculations involved are handled by the TFT_eSPI library. To create a dial
// with a moving needle a graphic of a meter needle is plotted at a specified angle into another
// Sprite that contains the dial face. When the needle Sprite is pushed to the dial Sprite the
// plotting ensures two pivot points for each Sprite coincide with pixel level accuracy.

// Two rotation pivot points must be set, one for the first Sprite and one for the second
// Sprite using setPivot(). These pivot points do not need to be within the Sprite boundaries.

// In this example a needle graphic is also be plotted direct to a defined TFT pivot point.

// The rotation angle is in degrees, an angle of 0 means no Sprite rotation.

// The pushRotated() function works with 1, 8 and 16-bit per pixel (bpp) Sprites.

// For 1 bpp Sprites the foreground and background colours are defined with the
// member function setBitmapColor(foregroundColor, backgroundColor).

// Created by Bodmer 6/1/19 as an example to the TFT_eSPI library:
// https://github.com/Bodmer/TFT_eSPI

#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

TFT_eSprite dial   = TFT_eSprite(&tft); // Sprite object for dial
TFT_eSprite needle = TFT_eSprite(&tft); // Sprite object for needle

uint32_t startMillis;

int16_t angle = 0;

// =======================================================================================
// Setup
// =======================================================================================

void setup()   {
  Serial.begin(250000); // Debug only

  tft.begin();
  tft.setRotation(1);

  // Clear TFT screen
  tft.fillScreen(TFT_NAVY);

  // Create the dial Sprite and dial (this temporarily hijacks the use of the needle Sprite)
  createDialScale(-120, 120, 30);   // create scale (start angle, end angle, increment angle)
  drawEmptyDial("Label", 12345);    // draw the centre of dial in the Sprite

  dial.pushSprite(110, 0);          // push a copy of the dial to the screen so we can see it

  // Create the needle Sprite
  createNeedle();                // draw the needle graphic
  needle.pushSprite(95, 7);    // push a copy of the needle to the screen so we can see it
}

// =======================================================================================
// Loop
// =======================================================================================

void loop() {

  // Push the needle sprite to the dial Sprite at different angles and then push the dial to the screen
  // Use angle increments in range 1 to 6 for smoother or faster movement
  for (int16_t angle = -120; angle <= 120; angle += 2) {
    plotDial(0, 0, angle, "RPM", angle + 120);
    delay(25);
    yield(); // Avoid a watchdog time-out
  }

  delay(1000);  // Pause

  // Update the dial Sprite with decreasing angle and plot to screen at 0,0, no delay
  for (int16_t angle = 120; angle >= -120; angle -= 2) {
    plotDial(0, 0, angle, "RPM", angle + 120);
    yield(); // Avoid a watchdog time-out
  }

  // Now show plotting of the rotated needle direct to the TFT
  tft.setPivot(45, 150); // Set the TFT pivot point that the needle will rotate around

  // The needle graphic has a black border so if the angle increment is small
  // (6 degrees or less in this case) it wipes the last needle position when
  // it is rotated and hence it clears the swept area to black
  for (int16_t angle = 0; angle <= 360; angle += 5)
  {
    needle.pushRotated(angle); // Plot direct to TFT at specified angle
    yield();                   // Avoid a watchdog time-out
  }
}

// =======================================================================================
// Create the dial sprite, the dial outer and place scale markers
// =======================================================================================

void createDialScale(int16_t start_angle, int16_t end_angle, int16_t increment)
{
  // Create the dial Sprite
  dial.setColorDepth(8);       // Size is odd (i.e. 91) so there is a centre pixel at 45,45
  dial.createSprite(91, 91);   // 8bpp requires 91 * 91 = 8281 bytes
  dial.setPivot(45, 45);       // set pivot in middle of dial Sprite

  // Draw dial outline
  dial.fillSprite(TFT_TRANSPARENT);           // Fill with transparent colour
  dial.fillCircle(45, 45, 43, TFT_DARKGREY);  // Draw dial outer

  // Hijack the use of the needle Sprite since that has not been used yet!
  needle.createSprite(3, 3);     // 3 pixels wide, 3 high
  needle.fillSprite(TFT_WHITE);  // Fill with white
  needle.setPivot(1, 43);        //  Set pivot point x to the Sprite centre and y to marker radius

  for (int16_t angle = start_angle; angle <= end_angle; angle += increment) {
    needle.pushRotated(&dial, angle); // Sprite is used to make scale markers
    yield(); // Avoid a watchdog time-out
  }

  needle.deleteSprite(); // Delete the hijacked Sprite
}


// =======================================================================================
// Add the empty dial face with label and value
// =======================================================================================

void drawEmptyDial(String label, int16_t val)
{
  // Draw black face
  dial.fillCircle(45, 45, 40, TFT_BLACK);
  dial.drawPixel(45, 45, TFT_WHITE);        // For demo only, mark pivot point with a while pixel

  dial.setTextDatum(TC_DATUM);              // Draw dial text
  dial.drawString(label, 45, 15, 2);
  dial.drawNumber(val, 45, 60, 2);
}

// =======================================================================================
// Update the dial and plot to screen with needle at defined angle
// =======================================================================================

void plotDial(int16_t x, int16_t y, int16_t angle, String label, uint16_t val)
{
  // Draw the blank dial in the Sprite, add label and number
  drawEmptyDial(label, val);

  // Push a rotated needle Sprite to the dial Sprite, with black as transparent colour
  needle.pushRotated(&dial, angle, TFT_BLACK); // dial is the destination Sprite

  // Push the resultant dial Sprite to the screen, with transparent colour
  dial.pushSprite(x, y, TFT_TRANSPARENT);
}

// =======================================================================================
// Create the needle Sprite and the image of the needle
// =======================================================================================

void createNeedle(void)
{
  needle.setColorDepth(8);
  needle.createSprite(11, 49); // create the needle Sprite 11 pixels wide by 49 high

  needle.fillSprite(TFT_BLACK);          // Fill with black

  // Define needle pivot point
  uint16_t piv_x = needle.width() / 2;   // x pivot of Sprite (middle)
  uint16_t piv_y = needle.height() - 10; // y pivot of Sprite (10 pixels from bottom)
  needle.setPivot(piv_x, piv_y);         // Set pivot point in this Sprite

  // Draw the red needle with a yellow tip
  // Keep needle tip 1 pixel inside dial circle to avoid leaving stray pixels
  needle.fillRect(piv_x - 1, 2, 3, piv_y + 8, TFT_RED);
  needle.fillRect(piv_x - 1, 2, 3, 5, TFT_YELLOW);

  // Draw needle centre boss
  needle.fillCircle(piv_x, piv_y, 5, TFT_MAROON);
  needle.drawPixel( piv_x, piv_y, TFT_WHITE);     // Mark needle pivot point with a white pixel
}

// =======================================================================================
