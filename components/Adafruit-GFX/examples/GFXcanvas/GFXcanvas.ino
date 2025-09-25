/***
This example is intended to demonstrate the use of getPixel() versus
getRawPixel() and the fast horizontal and vertical drawing routines
in the GFXcanvas family of classes,

When using the GFXcanvas* classes as the image buffer for a hardware driver,
there is a need to get individual pixel color values at given physical
coordinates. Rather than subclasses or client classes call getBuffer() and
reinterpret the byte layout of the buffer, two methods are added to each of the
GFXcanvas* classes that allow fetching of specific pixel values.

  * getPixel(x, y)   : Gets the pixel color value at the rotated coordinates in
the image.
  * getRawPixel(x,y) : Gets the pixel color value at the unrotated coordinates
in the image. This is useful for getting the pixel value to map to a hardware
pixel location. This method was made protected as only the hardware driver
should be accessing it.

The GFXcanvas*SerialDemo classes in this example will print to Serial the
contents of the underlying GFXcanvas buffer in both the current rotated layout
and the underlying physical layout.
***/

#include "GFXcanvasSerialDemo.h"
#include <Arduino.h>

void setup() {
  Serial.begin(115200);

  // first create a rectangular GFXcanvas8SerialDemo object and draw to it
  GFXcanvas8SerialDemo demo8(21, 11);

  demo8.fillScreen(0x00);
  demo8.setRotation(1); // now canvas is 11x21
  demo8.fillCircle(5, 10, 5, 0xAA);
  demo8.writeFastHLine(0, 0, 11, 0x11);
  demo8.writeFastHLine(10, 10, -11, 0x22);
  demo8.writeFastHLine(0, 20, 11, 0x33);
  demo8.writeFastVLine(0, 0, 21, 0x44);
  demo8.writeFastVLine(10, 20, -21, 0x55);

  Serial.println("Demonstrating GFXcanvas rotated and raw pixels.\n");

  // print it out rotated

  Serial.println("The canvas's content in the rotation of '0':\n");
  demo8.setRotation(0);
  demo8.print(true);
  Serial.println("\n");

  Serial.println("The canvas's content in the rotation of '1' (which is what "
                 "it was drawn in):\n");
  demo8.setRotation(1);
  demo8.print(true);
  Serial.println("\n");

  Serial.println("The canvas's content in the rotation of '2':\n");
  demo8.setRotation(2);
  demo8.print(true);
  Serial.println("\n");

  Serial.println("The canvas's content in the rotation of '3':\n");
  demo8.setRotation(3);
  demo8.print(true);
  Serial.println("\n");

  // print it out unrotated
  Serial.println("The canvas's content in it's raw, physical layout:\n");
  demo8.print(false);
  Serial.println("\n");

  // Demonstrate GFXcanvas1SerialDemo

  GFXcanvas1SerialDemo demo1(21, 11);
  demo1.fillScreen(0);
  demo1.setRotation(0);
  demo1.writeFastHLine(0, 0, 9, 1);
  demo1.setRotation(1);
  demo1.writeFastHLine(0, 0, 9, 1);
  demo1.setRotation(2);
  demo1.writeFastHLine(0, 0, 9, 1);
  demo1.setRotation(3);
  demo1.writeFastHLine(0, 0, 9, 1);
  demo1.setRotation(1);
  demo1.fillRect(3, 8, 5, 5, 1);

  Serial.println("\nThe GFXcanvas1 raw content after drawing a fast horizontal "
                 "line in each rotation:\n");
  demo1.print(false);
  Serial.println("\n");

  // Demonstrate GFXcanvas16SerialDemo

  GFXcanvas16SerialDemo demo16(21, 11);
  demo16.fillScreen(0);
  demo16.setRotation(0);
  demo16.writeFastHLine(0, 0, 9, 0x1111);
  demo16.setRotation(1);
  demo16.writeFastHLine(0, 0, 9, 0x2222);
  demo16.setRotation(2);
  demo16.writeFastHLine(0, 0, 9, 0x3333);
  demo16.setRotation(3);
  demo16.writeFastHLine(0, 0, 9, 0x4444);
  demo16.setRotation(1);
  demo16.fillRect(3, 8, 5, 5, 0x8888);

  Serial.println("\nThe GFXcanvas16 raw content after drawing a fast "
                 "horizontal line in each rotation:\n");
  demo16.print(false);
  Serial.println("\n");
}

void loop() {}
