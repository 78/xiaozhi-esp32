// Draw random coloured smooth (anti-aliased) rounded rectangles on the TFT

#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

void setup(void) {
  tft.init();
  tft.fillScreen(TFT_BLACK); // Background is black
}

void loop() {
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0);

  // Draw some random smooth rounded rectangles
  for (int i = 0; i < 20; i++)
  {
    int radius = random(60);
    int w = random(2 * radius, 160);
    int h = random(2 * radius, 160);
    int t = random(1, radius / 3);
    int x = random(tft.width() - w);
    int y = random(tft.height() - h);

    // Random colour is anti-aliased (blended) with background colour (black in this case)
    tft.drawSmoothRoundRect(x, y, radius, radius - t, w, h, random(0x10000), TFT_BLACK);
  }
  tft.print("Variable thickness");
  delay(2000);

  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0);

  // Draw some random minimum thickness smooth rounded rectangles
  for (int i = 0; i < 20; i++)
  {
    int radius = random(60);
    int w = random(2 * radius, 160);
    int h = random(2 * radius, 160);
    int t = 0;
    int x = random(tft.width() - w);
    int y = random(tft.height() - h);

    // Random colour is anti-aliased (blended) with background colour (black in this case)
    tft.drawSmoothRoundRect(x, y, radius, radius - t, w, h, random(0x10000), TFT_BLACK);
  }
  tft.print("Minimum thickness");
  delay(2000);
}
