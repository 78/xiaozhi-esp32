// Include code in this tab and call screenshotToConsole() this dumps an
// image off the screen and sends it to a PC via the serial port in a Run
// Length Encoded format for viewing with a "ILIScreenshotViewer" utility.

// The PC "ILIScreenshotViewer" is part of the ILI9241_due library in the
// Tools folder, that library can be found here:
// https://github.com/marekburiak/ILI9341_Due

// Converted by Bodmer to operate with the TFT_ILI9341_ESP library:
// https://github.com/Bodmer/TFT_ILI9341_ESP

/*
  The functions below have been adapted from the ILI9341_due library, the file
  header from the .cpp source file is included below:

  ILI9341_due_.cpp - Arduino Due library for interfacing with ILI9341-based TFTs

  Copyright (c) 2014  Marek Buriak

  This library is based on ILI9341_t3 library from Paul Stoffregen
  (https://github.com/PaulStoffregen/ILI9341_t3), Adafruit_ILI9341
  and Adafruit_GFX libraries from Limor Fried/Ladyada
  (https://github.com/adafruit/Adafruit_ILI9341).

  This file is part of the Arduino ILI9341_due library.
  Sources for this library can be found at https://github.com/marekburiak/ILI9341_Due.

  ILI9341_due is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation, either version 2.1 of the License, or
  (at your option) any later version.

  ILI9341_due is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details:
  <http://www.gnu.org/licenses/>.

*/
//====================================================================================

void screenshotToConsole()
{
  uint8_t e = 0;
  uint8_t lastColor[3];
  uint8_t color[3];
  uint32_t sameColorPixelCount = 0;
  uint16_t sameColorPixelCount16 = 0;
  uint32_t sameColorStartIndex = 0;
  uint32_t totalImageDataLength = 0;

//  delay(1000);

  // Header text
  Serial.println((eye[e].tft.width() - 1));
  Serial.println((eye[e].tft.height() - 1));
  Serial.println(F("==== PIXEL DATA START ===="));

  // Get first pixel to prime the Run Length Encoded
  // Function format is: tft.readRectRGB( x, y, width, height, buffer);
  // color is a pointer to a buffer that the RGB 8-bit values are piped into
  // the buffer size must be >= (width * height * 3) bytes
  eye[e].tft.readRectRGB(0, 0, 1, 1, color); // 1 x 1 so reading 1 pixel at 0,0

  lastColor[0] = color[0];  // Red
  lastColor[1] = color[1];  // Green
  lastColor[2] = color[2];  // Blue

  printHex8(color, 3);  //Send color of the first pixel to serial port
  totalImageDataLength += 6;
  sameColorStartIndex = 0;

  for (uint32_t py = 0; py < (eye[e].tft.height() - 1); py++)
  {
    for (uint32_t px = 0; px < (eye[e].tft.width() - 1); px++)
    {
      uint32_t i = px + eye[e].tft.width() * py;
      yield();
      if (i)
      {
        eye[e].tft.readRectRGB(px, py, 1, 1, color);

        if (color[0] != lastColor[0] ||
            color[1] != lastColor[1] ||
            color[2] != lastColor[2])
        {
          sameColorPixelCount = i - sameColorStartIndex;
          if (sameColorPixelCount > 65535)
          {
            sameColorPixelCount16 = 65535;
            printHex16(&sameColorPixelCount16, 1);
            printHex8(lastColor, 3);
            totalImageDataLength += 10;
            sameColorPixelCount16 = sameColorPixelCount - 65535;
          }
          else
            sameColorPixelCount16 = sameColorPixelCount;
          printHex16(&sameColorPixelCount16, 1);
          printHex8(color, 3);
          totalImageDataLength += 10;

          sameColorStartIndex = i;
          lastColor[0] = color[0];
          lastColor[1] = color[1];
          lastColor[2] = color[2];
        }
      }
    }
  }
  sameColorPixelCount = (uint32_t)eye[e].tft.width() * (uint32_t)eye[e].tft.height() - sameColorStartIndex;
  if (sameColorPixelCount > 65535)
  {
    sameColorPixelCount16 = 65535;
    printHex16(&sameColorPixelCount16, 1);
    printHex8(lastColor, 3);
    totalImageDataLength += 10;
    sameColorPixelCount16 = sameColorPixelCount - 65535;
  }
  else
    sameColorPixelCount16 = sameColorPixelCount;
  printHex16(&sameColorPixelCount16, 1);
  totalImageDataLength += 4;
  printHex32(&totalImageDataLength, 1);

  // Footer text
  Serial.println();
  Serial.println(F("==== PIXEL DATA END ===="));
  Serial.print(F("Total Image Data Length: "));
  Serial.println(totalImageDataLength);
}

void printHex8(uint8_t *data, uint8_t length) // prints 8-bit data in hex
{
  char tmp[length * 2 + 1];
  byte first;
  byte second;
  for (int i = 0; i < length; i++) {
    first = (data[i] >> 4) & 0x0f;
    second = data[i] & 0x0f;
    // base for converting single digit numbers to ASCII is 48
    // base for 10-16 to become upper-case characters A-F is 55
    // note: difference is 7
    tmp[i * 2] = first + 48;
    tmp[i * 2 + 1] = second + 48;
    if (first > 9) tmp[i * 2] += 7;
    if (second > 9) tmp[i * 2 + 1] += 7;
  }
  tmp[length * 2] = 0;
  Serial.print(tmp);
}

void printHex16(uint16_t *data, uint8_t length) // prints 8-bit data in hex
{
  char tmp[length * 4 + 1];
  byte first;
  byte second;
  byte third;
  byte fourth;
  for (int i = 0; i < length; i++) {
    first = (data[i] >> 12) & 0x0f;
    second = (data[i] >> 8) & 0x0f;
    third = (data[i] >> 4) & 0x0f;
    fourth = data[i] & 0x0f;
    //Serial << first << " " << second << " " << third << " " << fourth << endl;
    // base for converting single digit numbers to ASCII is 48
    // base for 10-16 to become upper-case characters A-F is 55
    // note: difference is 7
    tmp[i * 4] = first + 48;
    tmp[i * 4 + 1] = second + 48;
    tmp[i * 4 + 2] = third + 48;
    tmp[i * 4 + 3] = fourth + 48;
    //tmp[i*5+4] = 32; // add trailing space
    if (first > 9) tmp[i * 4] += 7;
    if (second > 9) tmp[i * 4 + 1] += 7;
    if (third > 9) tmp[i * 4 + 2] += 7;
    if (fourth > 9) tmp[i * 4 + 3] += 7;
  }
  tmp[length * 4] = 0;
  Serial.print(tmp);
}

void printHex32(uint32_t *data, uint8_t length) // prints 8-bit data in hex
{
  char tmp[length * 8 + 1];
  byte dataByte[8];
  for (int i = 0; i < length; i++) {
    dataByte[0] = (data[i] >> 28) & 0x0f;
    dataByte[1] = (data[i] >> 24) & 0x0f;
    dataByte[2] = (data[i] >> 20) & 0x0f;
    dataByte[3] = (data[i] >> 16) & 0x0f;
    dataByte[4] = (data[i] >> 12) & 0x0f;
    dataByte[5] = (data[i] >> 8) & 0x0f;
    dataByte[6] = (data[i] >> 4) & 0x0f;
    dataByte[7] = data[i] & 0x0f;
    //Serial << first << " " << second << " " << third << " " << fourth << endl;
    // base for converting single digit numbers to ASCII is 48
    // base for 10-16 to become upper-case characters A-F is 55
    // note: difference is 7
    tmp[i * 4] = dataByte[0] + 48;
    tmp[i * 4 + 1] = dataByte[1] + 48;
    tmp[i * 4 + 2] = dataByte[2] + 48;
    tmp[i * 4 + 3] = dataByte[3] + 48;
    tmp[i * 4 + 4] = dataByte[4] + 48;
    tmp[i * 4 + 5] = dataByte[5] + 48;
    tmp[i * 4 + 6] = dataByte[6] + 48;
    tmp[i * 4 + 7] = dataByte[7] + 48;
    //tmp[i*5+4] = 32; // add trailing space
    if (dataByte[0] > 9) tmp[i * 4] += 7;
    if (dataByte[1] > 9) tmp[i * 4 + 1] += 7;
    if (dataByte[2] > 9) tmp[i * 4 + 2] += 7;
    if (dataByte[3] > 9) tmp[i * 4 + 3] += 7;
    if (dataByte[4] > 9) tmp[i * 4 + 4] += 7;
    if (dataByte[5] > 9) tmp[i * 4 + 5] += 7;
    if (dataByte[6] > 9) tmp[i * 4 + 6] += 7;
    if (dataByte[7] > 9) tmp[i * 4 + 7] += 7;
  }
  tmp[length * 8] = 0;
  Serial.print(tmp);
}

