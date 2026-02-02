/*  

  // Create a viewport at TFT screen coordinated X,Y of width W and height H
  tft.setViewport(X, Y, W, H); // By default the 0,0 coordinate datum is moved to top left
                               // corner of viewport
                               // Note: tft.width() and tft.height() now return viewport size!
  // The above command is identical to:
  tft.setViewport(VP_X, VP_Y, VP_W, VP_H, true); // true parameter is optional

  // To create a viewport that keeps the coordinate datum at top left of TFT, use false parameter
  tft.setViewport(VP_X, VP_Y, VP_W, VP_H, false); // Note: tft.width() and tft.height() return TFT size!

  // To get viewport x, y coordinates, width, height and datum position flag
  int32_t x = tft.getViewportX();      // Always returns viewport x coordinate relative to screen left edge
  int32_t y = tft.getViewportY(void);  // Always returns viewport y coordinate relative to screen top edge
  int32_t w = tft.getViewportWidth();  // Always returns width of viewport
  int32_t h = tft.getViewportHeight(); // Always returns height of viewport
  bool    f = tft.getViewportDatum();  // Datum of the viewport (false = TFT corner, true = viewport corner)
  // To check if all or part of an area is in the viewport
  checkViewport(x, y, w, h); // Returns "true" if all or part of area is in viewport

  // To draw a rectangular frame outside viewport of width W (when W is negative)
  tft.frameViewport(TFT_RED, -W); // Note setting the width to a large negative value will clear the screen
                                  // outside the viewport

  // To draw a rectangular frame inside viewport of width W (when W is positive)
  tft.frameViewport(TFT_RED,  W); // Note setting the width to a large positive value will clear the screen
                                  // inside the viewport

  // To reset the viewport to the normal TFT full screen
  tft.resetViewport();  // Note: Graphics will NOT be drawn to the TFT outside a viewport until
                        // this command is used! ( The exception is using the frameViewport command
                        // detailed above with a negative width.)

  // Note:
  // Using setRotation rotates the whole TFT screen it does not just
  // rotate the viewport (this is a possible future enhancement).
  // Redraw all graphics after a rotation since some TFT's do not
  // re-map the TFT graphics RAM to the screen pixels as expected.
*/