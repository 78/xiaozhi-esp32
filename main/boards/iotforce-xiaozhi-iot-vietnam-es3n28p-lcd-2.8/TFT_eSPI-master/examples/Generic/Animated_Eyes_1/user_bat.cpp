// SERVO BAT: flapping paper-cutout bat (attached to servo on SERVO_PIN)
// triggered by contact-sensitive conductive thread on CAPTOUCH_PIN.
// See user.cpp for basics of connecting user code to animated eyes.

#if 0 // Change to 1 to enable this code (must enable ONE user*.cpp only!)

#include "Adafruit_FreeTouch.h"
#include <Servo.h>

#define CAPTOUCH_PIN A5 // Capacitive touch pin - attach conductive thread here
#define SERVO_PIN     4 // Servo plugged in here

// Set up capacitive touch button using the FreeTouch library
static Adafruit_FreeTouch touch(CAPTOUCH_PIN, OVERSAMPLE_4, RESISTOR_50K, FREQ_MODE_NONE);
static long     oldState;          // Last-read touch value
static bool     isTouched = false; // When true, bat is flapping
static uint32_t touchTime = 0;     // millis() time when flapping started
static uint32_t touchThreshold;

Servo servo;

void user_setup(void) {
  if (!touch.begin())
    Serial.println("Cap touch init failed");
  servo.attach(SERVO_PIN);
  servo.write(0); // Move servo to idle position
  servo.detach();

  // Attempt to auto-calibrate the touch threshold
  // (assumes thread is NOT touched on startup!)
  touchThreshold = 0;
  for(int i=0; i<10; i++) {
    touchThreshold += touch.measure(); // Accumulate 10 readings
    delay(50);
  }
  touchThreshold /= 10; // Average "not touched" value
  touchThreshold = ((touchThreshold * 127) + 1023) / 128; // Threshold = ~1% toward max

  oldState = touch.measure();
}

#define FLAP_TIME_RISING   900 // 0-to-180 degree servo sweep time, in milliseconds
#define FLAP_TIME_FALLING 1200 // 180-to-0 servo sweep time
#define FLAP_REPS            3 // Number of times to flap
#define FLAP_TIME_PER     (FLAP_TIME_RISING + FLAP_TIME_FALLING)
#define FLAP_TIME_TOTAL   (FLAP_TIME_PER * FLAP_REPS)

void user_loop(void) {
  long newState = touch.measure();
  Serial.println(newState);

  if (isTouched) {
    uint32_t elapsed = millis() - touchTime;
    if (elapsed >= FLAP_TIME_TOTAL) {   // After all flaps are completed
      isTouched = false;                // Bat goes idle again
      servo.write(0);
      servo.detach();
    } else {
      elapsed %= FLAP_TIME_PER;         // Time within current flap cycle
      if (elapsed < FLAP_TIME_RISING) { // Over the course of 0 to FLAP_TIME_RISING...
        servo.write(elapsed * 180 / FLAP_TIME_RISING); // Move 0 to 180 degrees
      } else {                          // Over course of FLAP_TIME_FALLING, return to 0
        servo.write(180 - ((elapsed - FLAP_TIME_RISING) * 180 / FLAP_TIME_FALLING));
      }
    }
  } else {
    // Bat is idle...check for capacitive touch...
    if (newState > touchThreshold && oldState < touchThreshold) {
      delay(100);                      // Short delay to debounce
      newState = touch.measure();      // Verify whether still touched
      if (newState > touchThreshold) { // It is!
        isTouched = true;              // Start a new flap session
        touchTime = millis();
        servo.attach(SERVO_PIN);
        servo.write(0);
      }
    }
  }

  oldState = newState; // Save cap touch state
}

#endif // 0
