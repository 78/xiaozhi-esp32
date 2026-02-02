 // Coded by Bodmer 10/2/18, see license in root directory.
 // This is part of the TFT_eSPI class and is associated with the Touch Screen handlers

 public:
           // Get raw x,y ADC values from touch controller
  uint8_t  getTouchRaw(uint16_t *x, uint16_t *y);
           // Get raw z (i.e. pressure) ADC value from touch controller
  uint16_t getTouchRawZ(void);
           // Convert raw x,y values to calibrated and correctly rotated screen coordinates
  void     convertRawXY(uint16_t *x, uint16_t *y);
           // Get the screen touch coordinates, returns true if screen has been touched
           // if the touch coordinates are off screen then x and y are not updated
           // The returned value can be treated as a bool type, false or 0 means touch not detected
           // In future the function may return an 8-bit "quality" (jitter) value.
           // The threshold value is optional, this must be higher than the bias level for z (pressure)
           // reported by Test_Touch_Controller when the screen is NOT touched. When touched the z value
           // must be higher than the threshold for a touch to be detected.
  uint8_t  getTouch(uint16_t *x, uint16_t *y, uint16_t threshold = 600);

           // Run screen calibration and test, report calibration values to the serial port
  void     calibrateTouch(uint16_t *data, uint32_t color_fg, uint32_t color_bg, uint8_t size);
           // Set the screen calibration values
  void     setTouch(uint16_t *data);

 private:
           // Legacy support only - deprecated TODO: delete
  void     spi_begin_touch();
  void     spi_end_touch();

           // Handlers for the touch controller bus settings
  inline void begin_touch_read_write() __attribute__((always_inline));
  inline void end_touch_read_write()   __attribute__((always_inline));

           // Private function to validate a touch, allow settle time and reduce spurious coordinates
  uint8_t  validTouch(uint16_t *x, uint16_t *y, uint16_t threshold = 600);

           // Initialise with example calibration values so processor does not crash if setTouch() not called in setup()
  uint16_t touchCalibration_x0 = 300, touchCalibration_x1 = 3600, touchCalibration_y0 = 300, touchCalibration_y1 = 3600;
  uint8_t  touchCalibration_rotate = 1, touchCalibration_invert_x = 2, touchCalibration_invert_y = 0;

  uint32_t _pressTime;        // Press and hold time-out
  uint16_t _pressX, _pressY;  // For future use (last sampled calibrated coordinates)
