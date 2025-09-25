// GxEPD2_SD_WriteBitmap: utility for Display Library for SPI e-paper panels from Dalian Good Display and boards from Waveshare.
//
// Display Library based on Demo Example from Good Display: https://www.good-display.com/companyfile/32/
//
// Author: Jean-Marc Zingg
//
// Version: see library.properties
//
// Library: https://github.com/ZinggJM/GxEPD2

#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <Fonts/FreeMonoBold9pt7b.h>

#include "bitmaps/Bitmaps200x200.h" // 1.54" b/w
//#include "bitmaps/Bitmaps128x250.h" // 2.13" b/w
//#include "bitmaps/Bitmaps128x296.h" // 2.9"  b/w
//#include "bitmaps/Bitmaps176x264.h" // 2.7"  b/w
//#include "bitmaps/Bitmaps400x300.h" // 4.2"  b/w
//#include "bitmaps/Bitmaps640x384.h" // 7.5"  b/w
// 3-color
//#include "bitmaps/Bitmaps3c200x200.h" // 1.54" b/w/r
//#include "bitmaps/Bitmaps3c104x212.h" // 2.13" b/w/r
//#include "bitmaps/Bitmaps3c128x296.h" // 2.9"  b/w/r
//#include "bitmaps/Bitmaps3c176x264.h" // 2.7"  b/w/r
//#include "bitmaps/Bitmaps3c400x300.h" // 4.2"  b/w/r

// include SdFat for FAT32 support with long filenames; available through Library Manager
#include <SdFat.h>

#if defined(SdFat_h)
SdFat SD;
#define FileClass SdFile
#define position curPosition
#define seek seekSet
#else
#include <SD.h>
#define FileClass File
#undef SdFat_h
#endif

void setup()
{
  Serial.begin(115200);
  Serial.print("Initializing SD card...");
  if (!SD.begin(SS))
  {
    Serial.println("initialization failed!");
    while (1) yield();
  }
  Serial.println("initialization done.");
  writeBitmaps();
}

void loop()
{
}

void writeBitmap(const char filename[], const uint8_t bitmap[], int16_t w, int16_t h, uint16_t depth = 1);
uint8_t filldata[] = {0x0, 0x23, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFF, 0xFF, 0xFF, 0x0};

void writeBitmaps()
{
#ifdef _GxBitmaps200x200_H_
  writeBitmap("logo.bmp", logo200x200, 200, 200);
  writeBitmap("logo200x200.bmp", logo200x200, 200, 200);
  writeBitmap("first200x200.bmp", first200x200, 200, 200);
  writeBitmap("second200x200.bmp", second200x200, 200, 200);
  writeBitmap("third200x200.bmp", third200x200, 200, 200);
  writeBitmap("fourth200x200.bmp", fourth200x200, 200, 200);
  writeBitmap("fifth200x200.bmp", fifth200x200, 200, 200);
  writeBitmap("sixth200x200.bmp", sixth200x200, 200, 200);
  writeBitmap("senventh200x200.bmp", senventh200x200, 200, 200);
  writeBitmap("eighth200x200.bmp", eighth200x200, 200, 200);
#endif
#ifdef _GxBitmaps128x250_H_
  writeBitmap("Bitmap128x250_1.bmp", Bitmap128x250_1, 128, 250);
  writeBitmap("logo128x250.bmp", logo128x250, 128, 250);
  writeBitmap("first128x250.bmp", first128x250, 128, 250);
  writeBitmap("second128x250.bmp", second128x250, 128, 250);
  writeBitmap("third128x250.bmp", third128x250, 128, 250);
#endif
#ifdef _GxBitmaps128x296_H_
  writeBitmap("Bitmap128x296_1.bmp", Bitmap128x296_1, 128, 296);
  writeBitmap("logo128x296.bmp", logo128x296, 128, 296);
  writeBitmap("first128x296.bmp", first128x296, 128, 296);
  writeBitmap("second128x296.bmp", second128x296, 128, 296);
  writeBitmap("third128x296.bmp", third128x296, 128, 296);
#endif
#ifdef _GxBitmaps176x264_H_
  writeBitmap("Bitmap176x264_1.bmp", Bitmap176x264_1, 176, 264);
  writeBitmap("Bitmap176x264_2.bmp", Bitmap176x264_2, 176, 264);
#endif
#ifdef _GxBitmaps400x300_H_
  writeBitmap("Bitmap400x300_1.bmp", Bitmap400x300_1, 400, 300);
  writeBitmap("Bitmap400x300_2.bmp", Bitmap400x300_2, 400, 300);
#endif
#ifdef _GxBitmaps640x384_H_
  writeBitmap("Bitmap640x384_1.bmp", Bitmap640x384_1, 640, 384);
  writeBitmap("Bitmap640x384_2.bmp", Bitmap640x384_2, 640, 384);
#endif
  // 3-color
#ifdef _GxBitmaps3c200x200_H_
  writeBitmap("Bitmap3c200x200_black.bmp", Bitmap3c200x200_black, 200, 200);
  writeBitmap("Bitmap3c200x200_red.bmp", Bitmap3c200x200_red, 200, 200);
  writeBitmap("WS_Bitmap3c200x200_black.bmp", WS_Bitmap3c200x200_black, 200, 200);
  writeBitmap("WS_Bitmap3c200x200_red.bmp", WS_Bitmap3c200x200_red, 200, 200);
#endif
#ifdef _GxBitmaps3c104x212_H_
  writeBitmap("Bitmap3c104x212_1_black.bmp", Bitmap3c104x212_1_black, 104, 212);
  writeBitmap("Bitmap3c104x212_1_red.bmp", Bitmap3c104x212_1_red, 104, 212);
  writeBitmap("Bitmap3c104x212_2_black.bmp", Bitmap3c104x212_2_black, 104, 212);
  writeBitmap("Bitmap3c104x212_2_red.bmp", Bitmap3c104x212_2_red, 104, 212);
  writeBitmap("WS_Bitmap3c104x212_black.bmp", WS_Bitmap3c104x212_black, 104, 212);
  writeBitmap("WS_Bitmap3c104x212_red.bmp", WS_Bitmap3c104x212_red, 104, 212);
#endif
#ifdef _GxBitmaps3c128x296_H_
  writeBitmap("Bitmap3c128x296_1_black.bmp", Bitmap3c128x296_1_black, 128, 296);
  writeBitmap("Bitmap3c128x296_1_red.bmp", Bitmap3c128x296_1_red, 128, 296);
  writeBitmap("Bitmap3c128x296_2_black.bmp", Bitmap3c128x296_2_black, 128, 296);
  writeBitmap("Bitmap3c128x296_2_red.bmp", Bitmap3c128x296_2_red, 128, 296);
  writeBitmap("WS_Bitmap3c128x296_black.bmp", WS_Bitmap3c128x296_black, 128, 296);
  writeBitmap("WS_Bitmap3c128x296_red.bmp", WS_Bitmap3c128x296_red, 128, 296);
#endif
#ifdef _GxBitmaps3c176x264_H_
  writeBitmap("Bitmap3c176x264_black.bmp", Bitmap3c176x264_black, 176, 264);
  writeBitmap("Bitmap3c176x264_red.bmp", Bitmap3c176x264_red, 176, 264);
#endif
#ifdef _GxBitmaps3c400x300_H_
  writeBitmap("Bitmap3c400x300_1_black.bmp", Bitmap3c400x300_1_black, 400, 300);
  writeBitmap("Bitmap3c400x300_1_red.bmp", Bitmap3c400x300_1_red, 400, 300);
  writeBitmap("Bitmap3c400x300_2_black.bmp", Bitmap3c400x300_2_black, 400, 300);
  writeBitmap("Bitmap3c400x300_2_red.bmp", Bitmap3c400x300_2_red, 400, 300);
  writeBitmap("WS_Bitmap3c400x300_black.bmp", WS_Bitmap3c400x300_black, 400, 300);
  writeBitmap("WS_Bitmap3c400x300_red.bmp", WS_Bitmap3c400x300_red, 400, 300);
#endif
}

void writeBitmap(const char filename[], const uint8_t bitmap[], int16_t w, int16_t h, uint16_t depth)
{
  FileClass bitmapFile;
  uint32_t rowSizeCode = (w + 8 - depth) * depth / 8;
  // BMP rows are padded (if needed) to 4-byte boundary
  uint32_t rowSizeBMP = (w * depth / 8 + 3) & ~3;
  Serial.print("writeBitmap(\""); Serial.print(filename); Serial.println("\")");
  Serial.print("rowSizeCode "); Serial.println(rowSizeCode);
  Serial.print("rowSizeBMP  "); Serial.println(rowSizeBMP);
  uint32_t headerSize = 40;
  uint32_t imageOffset = 62;
  uint32_t fileSize = imageOffset + h * rowSizeBMP;
#if defined(SdFat_h)
  if (bitmapFile.open(filename, FILE_WRITE))
#else
  bitmapFile = SD.open(filename, FILE_WRITE);
  if (bitmapFile)
#endif
  {
    bitmapFile.seek(0);
    write16(bitmapFile, 0x4D42); // BMP signature
    write32(bitmapFile, fileSize); // fileSize
    write32(bitmapFile, 0); // creator bytes
    write32(bitmapFile, imageOffset); // image offset
    write32(bitmapFile, headerSize); // Header size
    write32(bitmapFile, w); // image width
    write32(bitmapFile, h); // image height
    write16(bitmapFile, 1); // # planes
    write16(bitmapFile, depth); // // bits per pixel
    write32(bitmapFile, 0); // format uncompressed

    //Serial.print("first position "); Serial.println(bitmapFile.position());
    uint32_t j = 0;
    for (uint32_t i = bitmapFile.position(); i < imageOffset; i++)
    {
      //bitmapFile.write(uint8_t(0)); // remaining header bytes
      bitmapFile.write(filldata[j++]); // remaining header bytes
    }
    //Serial.print("start position "); Serial.println(bitmapFile.position());
    uint32_t rowidx = 0;
    for (uint16_t row = 0; row < h; row++) // for each line
    {
      uint32_t colidx;
      for (colidx = 0; colidx < rowSizeCode; colidx++)
      {
#if defined(__AVR) || defined(ESP8266) || defined(ESP32)
        uint8_t data = pgm_read_byte(&bitmap[rowidx + colidx]);
#else
        uint8_t data = bitmap[rowidx + colidx];
#endif
        bitmapFile.write(data);
      }
      rowidx += rowSizeCode;
      while (colidx++ < rowSizeBMP) bitmapFile.write(uint8_t (0)); // padding
      //Serial.println(bitmapFile.position());
    }
    bitmapFile.close();
    Serial.println("done");
  }
  else Serial.print("open file for write failed!");
}

void write16(FileClass& f, uint16_t v)
{
  f.write(uint8_t(v));
  f.write(uint8_t(v >> 8));
}

void write32(FileClass& f, uint32_t v)
{
  f.write(uint8_t(v));
  f.write(uint8_t(v >> 8));
  f.write(uint8_t(v >> 16));
  f.write(uint8_t(v >> 24));
}
