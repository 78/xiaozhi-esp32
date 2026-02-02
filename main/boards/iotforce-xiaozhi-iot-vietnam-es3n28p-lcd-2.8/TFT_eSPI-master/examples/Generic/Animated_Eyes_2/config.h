// Pin selections here are based on the original Adafruit Learning System
// guide for the Teensy 3.x project.  Some of these pin numbers don't even
// exist on the smaller SAMD M0 & M4 boards, so you may need to make other
// selections:

// GRAPHICS SETTINGS (appearance of eye) -----------------------------------

// If using a SINGLE EYE, you might want this next line enabled, which
// uses a simpler "football-shaped" eye that's left/right symmetrical.
// Default shape includes the caruncle, creating distinct left/right eyes.

//#define SYMMETRICAL_EYELID

// Enable ONE of these #includes -- HUGE graphics tables for various eyes:
#include "data/defaultEye.h"      // Standard human-ish hazel eye -OR-
//#include "data/dragonEye.h"     // Slit pupil fiery dragon/demon eye -OR-
//#include "data/noScleraEye.h"   // Large iris, no sclera -OR-
//#include "data/goatEye.h"       // Horizontal pupil goat/Krampus eye -OR-
//#include "data/newtEye.h"       // Eye of newt -OR-
//#include "data/terminatorEye.h" // Git to da choppah!
//#include "data/catEye.h"        // Cartoonish cat (flat "2D" colors)
//#include "data/owlEye.h"        // Minerva the owl (DISABLE TRACKING)
//#include "data/naugaEye.h"      // Nauga googly eye (DISABLE TRACKING)
//#include "data/doeEye.h"        // Cartoon deer eye (DISABLE TRACKING)

// DISPLAY HARDWARE SETTINGS (screen type & connections) -------------------
#define TFT_COUNT 2        // Number of screens (1 or 2)
#define TFT1_CS 22         // TFT 1 chip select pin (set to -1 to use TFT_eSPI setup)
#define TFT2_CS 21         // TFT 2 chip select pin (set to -1 to use TFT_eSPI setup)
#define TFT_1_ROT 1        // TFT 1 rotation
#define TFT_2_ROT 3        // TFT 2 rotation
#define EYE_1_XPOSITION  0 // x shift for eye 1 image on display
#define EYE_2_XPOSITION  0 // x shift for eye 2 image on display

#define DISPLAY_BACKLIGHT  -1 // Pin for backlight control (-1 for none)
#define BACKLIGHT_MAX    255

// EYE LIST ----------------------------------------------------------------
#define NUM_EYES 2 // Number of eyes to display (1 or 2)

#define BLINK_PIN   -1 // Pin for manual blink button (BOTH eyes)
#define LH_WINK_PIN -1 // Left wink pin (set to -1 for no pin)
#define RH_WINK_PIN -1 // Right wink pin (set to -1 for no pin)

// This table contains ONE LINE PER EYE.  The table MUST be present with
// this name and contain ONE OR MORE lines.  Each line contains THREE items:
// a pin number for the corresponding TFT/OLED display's SELECT line, a pin
// pin number for that eye's "wink" button (or -1 if not used), a screen
// rotation value (0-3) and x position offset for that eye.

#if (NUM_EYES == 2)
  eyeInfo_t eyeInfo[] = {
    { TFT1_CS, LH_WINK_PIN, TFT_1_ROT, EYE_1_XPOSITION }, // LEFT EYE chip select and wink pins, rotation and offset
    { TFT2_CS, RH_WINK_PIN, TFT_2_ROT, EYE_2_XPOSITION }, // RIGHT EYE chip select and wink pins, rotation and offset
  };
#else
  eyeInfo_t eyeInfo[] = {
    { TFT1_CS, LH_WINK_PIN, TFT_1_ROT, EYE_1_XPOSITION }, // EYE chip select and wink pins, rotation and offset
  };
#endif

// INPUT SETTINGS (for controlling eye motion) -----------------------------

// JOYSTICK_X_PIN and JOYSTICK_Y_PIN specify analog input pins for manually
// controlling the eye with an analog joystick.  If set to -1 or if not
// defined, the eye will move on its own.
// IRIS_PIN specifies an analog input pin for a photocell to make pupils
// react to light (or potentiometer for manual control).  If set to -1 or
// if not defined, the pupils will change on their own.
// BLINK_PIN specifies an input pin for a button (to ground) that will
// make any/all eyes blink.  If set to -1 or if not defined, the eyes will
// only blink if AUTOBLINK is defined, or if the eyeInfo[] table above
// includes wink button settings for each eye.

//#define JOYSTICK_X_PIN A0 // Analog pin for eye horiz pos (else auto)
//#define JOYSTICK_Y_PIN A1 // Analog pin for eye vert position (")
//#define JOYSTICK_X_FLIP   // If defined, reverse stick X axis
//#define JOYSTICK_Y_FLIP   // If defined, reverse stick Y axis
#define TRACKING            // If defined, eyelid tracks pupil
#define AUTOBLINK           // If defined, eyes also blink autonomously

//  #define LIGHT_PIN      -1 // Light sensor pin
  #define LIGHT_CURVE  0.33 // Light sensor adjustment curve
  #define LIGHT_MIN       0 // Minimum useful reading from light sensor
  #define LIGHT_MAX    1023 // Maximum useful reading from sensor

#define IRIS_SMOOTH         // If enabled, filter input from IRIS_PIN
#if !defined(IRIS_MIN)      // Each eye might have its own MIN/MAX
  #define IRIS_MIN       90 // Iris size (0-1023) in brightest light
#endif
#if !defined(IRIS_MAX)
  #define IRIS_MAX      130 // Iris size (0-1023) in darkest light
#endif
