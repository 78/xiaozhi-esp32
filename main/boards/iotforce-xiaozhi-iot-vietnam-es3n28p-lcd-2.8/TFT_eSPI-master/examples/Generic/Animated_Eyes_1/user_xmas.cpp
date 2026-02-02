#if 0 // Change to 1 to enable this code (must enable ONE user*.cpp only!)

// Christmas demo for eye + NeoPixels. Randomly sets pixels in holiday-themed colors.

#include <Adafruit_NeoPixel.h>

// Pin 8 is the built-in NeoPixels on Circuit Playground Express & Bluetooth.
// With a TFT Gizmo attached, you can use A1 or A2 to easily connect a strand.
#define LED_PIN          8
#define LED_COUNT        10
#define LED_BRIGHTNESS   50 // about 1/5 brightness (max = 255)
#define TWINKLE_INTERVAL 333 // Every 333 ms (1/3 second), change a pixel
#define LIT_PIXELS       (LED_COUNT / 3) // Must be LESS than LED_COUNT/2

Adafruit_NeoPixel pixels(LED_COUNT, LED_PIN);


uint32_t timeOfLastTwinkle = 0;  // Used for timing pixel changes
uint8_t litPixel[LIT_PIXELS];    // Indices of which pixels are lit
uint8_t pixelIndex = LIT_PIXELS; // Index of currently-changing litPixel

uint32_t colors[] = { 0xFF0000, 0x00FF00, 0xFFFFFF }; // Red, green, white
#define NUM_COLORS (sizeof colors / sizeof colors[0])

void user_setup(void) {
  pixels.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
  pixels.show();            // Turn OFF all pixels ASAP
  pixels.setBrightness(LED_BRIGHTNESS);
  memset(litPixel, 255, sizeof litPixel); // Fill with out-of-range nonsense
}

void user_loop(void) {
  uint32_t t = millis();

  if((t - timeOfLastTwinkle) >= TWINKLE_INTERVAL) { // Time to update pixels?
    timeOfLastTwinkle = t;
    if(++pixelIndex >= LIT_PIXELS) pixelIndex = 0;

    // Pick a NEW pixel that's not currently lit and not adjacent to a lit one.
    // This just brute-force randomly tries pixels until a valid one is found,
    // no mathematical cleverness. Should only take a few iterations and won't
    // significantly slow down the eyes.
    int newPixel, pixelAfter, pixelBefore;
    do {
      newPixel    = random(LED_COUNT);
      pixelAfter  = (newPixel + 1) % LED_COUNT;
      pixelBefore = (newPixel - 1);
      if(pixelBefore < 0) pixelBefore = LED_COUNT - 1;
    } while(pixels.getPixelColor(newPixel)   ||
            pixels.getPixelColor(pixelAfter) ||
            pixels.getPixelColor(pixelBefore));

    // Turn OFF litPixel[pixelIndex]
    pixels.setPixelColor(litPixel[pixelIndex], 0);
    // 'newPixel' is the winner. Save in the litPixel[] array for later...
    litPixel[pixelIndex] = newPixel;
    // Turn ON newPixel with a random color from the colors[] list.
    pixels.setPixelColor(newPixel, colors[random(NUM_COLORS)]);

    pixels.show();
  }
}

#endif // 0
