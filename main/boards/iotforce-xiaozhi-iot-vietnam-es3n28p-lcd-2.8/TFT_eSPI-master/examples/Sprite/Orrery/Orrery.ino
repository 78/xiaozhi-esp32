// Display an Orrery
// Works for all display sizes but 320x480 minimum size recommended
// Whole planet orbits only visible in 480 x 800 display

// Flicker free sprite example for TFT_eSPI:
// https://github.com/Bodmer/TFT_eSPI
// Sketch coded by Bodmer
// Uses astronomy engine created by Don Cross

#include <TFT_eSPI.h>                 // Hardware-specific library

TFT_eSPI    tft = TFT_eSPI();         // Invoke library

TFT_eSprite img = TFT_eSprite(&tft);  // Sprite class

#define sunX tft.width()/2
#define sunY tft.height()/2

uint16_t orb_inc;
uint16_t planet_r;

#include <stdio.h>
#include "astronomy.h"
#define TIME_TEXT_BYTES  25

astro_time_t astro_time;

uint16_t grey;

static const astro_body_t body[] = {
  BODY_SUN, BODY_MERCURY, BODY_VENUS, BODY_EARTH, BODY_MARS,
  BODY_JUPITER, BODY_SATURN, BODY_URANUS, BODY_NEPTUNE
};

static const uint16_t bodyColour[] = {
  TFT_YELLOW, TFT_DARKGREY, TFT_ORANGE, TFT_BLUE, TFT_RED,
  TFT_GOLD, TFT_BROWN, TFT_DARKCYAN, TFT_CYAN
};


// =========================================================================
// Setup
// =========================================================================
void setup() {
  Serial.begin(115200);
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  // Test with smaller display sizes
  //tft.setViewport(10,10,160,128);
  //tft.setViewport(10,10,320,240);
  //tft.setViewport(10,10,480,320);
  //tft.frameViewport(TFT_GREEN, -1);

  img.createSprite(19, 19);

  grey = tft.color565(30, 30, 30);

  astro_time = Astronomy_MakeTime(2020, 10, 16, 19, 31, 0) ;
  tft.fillCircle(sunX, sunY, 10, TFT_YELLOW);

  // i initialised to 1 so Sun is skipped
  for (int i = 1; i < sizeof(body) / sizeof(body[0]); ++i)
  {
    tft.drawCircle(sunX, sunY, i * 28, grey);
  }
}


// =========================================================================
// Loop
// =========================================================================
void loop() {
  uint32_t dt = millis();
  plot_planets();
  showTime(astro_time);

  // Add time increment (more than 0.6 days will lead to stray pixel on screen
  // due to the way previous object images are erased)
  astro_time = Astronomy_AddDays(astro_time, 0.25); // 0.25 day (6 hour) increment

  dt = millis()-dt;
  //Serial.println(dt);
  //delay(1000);
}

// =========================================================================
// Get coordinates of end of a vector, pivot at x,y, length r, angle a
// =========================================================================
// Coordinates are returned to caller via the xp and yp pointers
#define DEG2RAD 0.0174532925
void getCoord(int x, int y, int *xp, int *yp, int r, float a)
{
  float sx1 = cos( -a * DEG2RAD );
  float sy1 = sin( -a * DEG2RAD );
  *xp =  sx1 * r + x;
  *yp =  sy1 * r + y;
}

// =========================================================================
// Convert astronomical time to UTC and display
// =========================================================================
void showTime(astro_time_t time)
{
    astro_status_t status;
    char text[TIME_TEXT_BYTES];

    status = Astronomy_FormatTime(time, TIME_FORMAT_SECOND, text, sizeof(text));
    if (status != ASTRO_SUCCESS)
    {
        fprintf(stderr, "\nFATAL(PrintTime): status %d\n", status);
        exit(1);
    }
    
    tft.drawString(text, 0, 0, 2);
}

// =========================================================================
// Plot planet positions as an Orrery
// =========================================================================
int plot_planets(void)
{
  astro_angle_result_t ang;

  int i;
  int num_bodies = sizeof(body) / sizeof(body[0]);

  // i initialised to 1 so Sun is skipped
  for (i = 1; i < num_bodies; ++i)
  {
    ang = Astronomy_EclipticLongitude(body[i], astro_time);

    int x1 = 0; // getCoord() will update these
    int y1 = 0;

    getCoord(0, 0, &x1, &y1, i * 28, ang.angle); // Get x1 ,y1

    img.fillSprite(TFT_TRANSPARENT);
    img.fillCircle(9, 9, 9, TFT_BLACK);
    img.drawCircle(9 - x1, 9 - y1, i * 28, grey);
    img.fillCircle(9, 9, 5, bodyColour[i]);
    img.pushSprite(sunX + x1 - 9, sunY + y1 - 9, TFT_TRANSPARENT);

    if (body[i] == BODY_EARTH)
    {
      astro_angle_result_t mang = Astronomy_LongitudeFromSun(BODY_MOON, astro_time);

      int xm = 0;
      int ym = 0;

      getCoord(x1, y1, &xm, &ym, 15, 180 + ang.angle + mang.angle); // Get x1 ,y1

      img.fillSprite(TFT_TRANSPARENT);
      img.fillCircle(9, 9, 7, TFT_BLACK);
      img.drawCircle(9 - xm, 9 - ym, i * 28, grey);
      img.fillCircle(9, 9, 2, TFT_WHITE);
      img.pushSprite(sunX + xm - 9, sunY + ym - 9, TFT_TRANSPARENT);
    }
  }

  return 0;
}
