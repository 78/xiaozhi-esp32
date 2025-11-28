// Display Library for SPI e-paper panels from Dalian Good Display and boards from Waveshare.
// Requires HW SPI and Adafruit_GFX. Caution: the e-paper panels require 3.3V supply AND data lines!
//
// Display Library based on Demo Example from Good Display: https://www.good-display.com/companyfile/32/
//
// Author: Jean-Marc Zingg
//
// Version: see library.properties
//
// Library: https://github.com/ZinggJM/GxEPD2

#ifndef _GxEPD2_7C_H_
#define _GxEPD2_7C_H_
// uncomment next line to use class GFX of library GFX_Root instead of Adafruit_GFX
//#include <GFX.h>

#ifndef ENABLE_GxEPD2_GFX
// default is off
#define ENABLE_GxEPD2_GFX 0
#endif

#if ENABLE_GxEPD2_GFX
#include "GxEPD2_GFX.h"
#define GxEPD2_GFX_BASE_CLASS GxEPD2_GFX
#elif defined(_GFX_H_)
#define GxEPD2_GFX_BASE_CLASS GFX
#else
#include <Adafruit_GFX.h>
#define GxEPD2_GFX_BASE_CLASS Adafruit_GFX
#endif

#include "GxEPD2_EPD.h"

// for __has_include see https://en.cppreference.com/w/cpp/preprocessor/include
// see also https://gcc.gnu.org/onlinedocs/cpp/_005f_005fhas_005finclude.html
// #if !defined(__has_include) || __has_include("epd/GxEPD2_102.h") is not portable!

#if defined __has_include
#  if __has_include("GxEPD2.h")
#    // __has_include can be used
#  else
#    // __has_include doesn't work for us, include anyway
#    undef __has_include
#    define __has_include(x) true
#  endif
#else
#  // no __has_include, include anyway
#  define __has_include(x) true
#endif

#if __has_include("epd7c/GxEPD2_565c.h")
#include "epd7c/GxEPD2_565c.h"
#endif
#if __has_include("epd7c/GxEPD2_565c_GDEP0565D90.h")
#include "epd7c/GxEPD2_565c_GDEP0565D90.h"
#endif
#if __has_include("epd7c/GxEPD2_730c_GDEY073D46.h")
#include "epd7c/GxEPD2_730c_GDEY073D46.h"
#endif
#if __has_include("epd7c/GxEPD2_730c_ACeP_730.h")
#include "epd7c/GxEPD2_730c_ACeP_730.h"
#endif
#if __has_include("epd7c/GxEPD2_730c_GDEP073E01.h")
#include "epd7c/GxEPD2_730c_GDEP073E01.h"
#endif

template<typename GxEPD2_Type, const uint16_t page_height>
class GxEPD2_7C : public GxEPD2_GFX_BASE_CLASS
{
  public:
    GxEPD2_Type epd2;
#if ENABLE_GxEPD2_GFX
    GxEPD2_7C(GxEPD2_Type epd2_instance) : GxEPD2_GFX_BASE_CLASS(epd2, GxEPD2_Type::WIDTH, GxEPD2_Type::HEIGHT), epd2(epd2_instance)
#else
    GxEPD2_7C(GxEPD2_Type epd2_instance) : GxEPD2_GFX_BASE_CLASS(GxEPD2_Type::WIDTH, GxEPD2_Type::HEIGHT), epd2(epd2_instance)
#endif
    {
      _page_height = page_height;
      _pages = (HEIGHT / _page_height) + ((HEIGHT % _page_height) > 0);
      _mirror = false;
      _using_partial_mode = false;
      _current_page = 0;
      setFullWindow();
    }

    uint16_t pages()
    {
      return _pages;
    }

    uint16_t pageHeight()
    {
      return _page_height;
    }

    bool mirror(bool m)
    {
      _swap_ (_mirror, m);
      return m;
    }

    void drawPixel(int16_t x, int16_t y, uint16_t color)
    {
      if ((x < 0) || (x >= width()) || (y < 0) || (y >= height())) return;
      if (_mirror) x = width() - x - 1;
      // check rotation, move pixel around if necessary
      switch (getRotation())
      {
        case 1:
          _swap_(x, y);
          x = WIDTH - x - 1;
          break;
        case 2:
          x = WIDTH - x - 1;
          y = HEIGHT - y - 1;
          break;
        case 3:
          _swap_(x, y);
          y = HEIGHT - y - 1;
          break;
      }
      // transpose partial window to 0,0
      x -= _pw_x;
      y -= _pw_y;
      // clip to (partial) window
      if ((x < 0) || (x >= int16_t(_pw_w)) || (y < 0) || (y >= int16_t(_pw_h))) return;
      // adjust for current page
      y -= _current_page * _page_height;
      // check if in current page
      if ((y < 0) || (y >= int16_t(_page_height))) return;
      uint32_t i = x / 2 + uint32_t(y) * (_pw_w / 2);
      uint8_t pv = color7(color);
      if (x & 1) _pixel_buffer[i] = (_pixel_buffer[i] & 0xF0) | pv;
      else _pixel_buffer[i] = (_pixel_buffer[i] & 0x0F) | (pv << 4);
    }

    void init(uint32_t serial_diag_bitrate = 0) // = 0 : disabled
    {
      epd2.init(serial_diag_bitrate);
      _using_partial_mode = false;
      _current_page = 0;
      setFullWindow();
    }

    // init method with additional parameters:
    // initial false for re-init after processor deep sleep wake up, if display power supply was kept
    // only relevant for b/w displays with fast partial update
    // reset_duration = 20 is default; a value of 2 may help with "clever" reset circuit of newer boards from Waveshare
    // pulldown_rst_mode true for alternate RST handling to avoid feeding 5V through RST pin
    void init(uint32_t serial_diag_bitrate, bool initial, uint16_t reset_duration = 20, bool pulldown_rst_mode = false)
    {
      epd2.init(serial_diag_bitrate, initial, reset_duration, pulldown_rst_mode);
      _using_partial_mode = false;
      _current_page = 0;
      setFullWindow();
    }

    // init method with additional parameters:
    // SPIClass& spi: either SPI or alternate HW SPI channel
    // SPISettings spi_settings: e.g. for higher SPI speed selection
    void init(uint32_t serial_diag_bitrate, bool initial, uint16_t reset_duration, bool pulldown_rst_mode, SPIClass& spi, SPISettings spi_settings)
    {
      epd2.selectSPI(spi, spi_settings);
      epd2.init(serial_diag_bitrate, initial, reset_duration, pulldown_rst_mode);
      _using_partial_mode = false;
      _current_page = 0;
      setFullWindow();
    }

    // release SPI and control pins
    void end() 
    {
      epd2.end();
    }

    void fillScreen(uint16_t color)
    {
      uint8_t pv = color7(color);
      uint8_t pv2 = pv | pv << 4;
      for (uint32_t x = 0; x < sizeof(_pixel_buffer); x++)
      {
        _pixel_buffer[x] = pv2;
      }
    }

    // display buffer content to screen, useful for full screen buffer
    void display(bool partial_update_mode = false)
    {
      epd2.writeNative(_pixel_buffer, 0, 0, 0, GxEPD2_Type::WIDTH, _page_height);
      epd2.refresh(partial_update_mode);
      if (!partial_update_mode) epd2.powerOff();
    }

    // display part of buffer content to screen, useful for full screen buffer
    // displayWindow, use parameters according to actual rotation.
    // x and w should be multiple of 8, for rotation 0 or 2,
    // y and h should be multiple of 8, for rotation 1 or 3,
    // else window is increased as needed,
    // this is an addressing limitation of the e-paper controllers
    void displayWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
    {
      x = gx_uint16_min(x, width());
      y = gx_uint16_min(y, height());
      w = gx_uint16_min(w, width() - x);
      h = gx_uint16_min(h, height() - y);
      _rotate(x, y, w, h);
      epd2.writeNativePart(_pixel_buffer, 0, x, y, GxEPD2_Type::WIDTH, _page_height, x, y, w, h);
      epd2.refresh(x, y, w, h);
    }

    void setFullWindow()
    {
      _using_partial_mode = false;
      _pw_x = 0;
      _pw_y = 0;
      _pw_w = GxEPD2_Type::WIDTH;
      _pw_h = HEIGHT;
    }

    // setPartialWindow, use parameters according to actual rotation.
    // x and w should be multiple of 8, for rotation 0 or 2,
    // y and h should be multiple of 8, for rotation 1 or 3,
    // else window is increased as needed,
    // this is an addressing limitation of the e-paper controllers
    void setPartialWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
    {
      if (!epd2.hasPartialUpdate) return;
      _pw_x = gx_uint16_min(x, width());
      _pw_y = gx_uint16_min(y, height());
      _pw_w = gx_uint16_min(w, width() - _pw_x);
      _pw_h = gx_uint16_min(h, height() - _pw_y);
      _rotate(_pw_x, _pw_y, _pw_w, _pw_h);
      _using_partial_mode = true;
      // make _pw_x, _pw_w multiple of 2
      _pw_w += _pw_x % 2;
      if (_pw_w % 2 > 0) _pw_w += 2 - _pw_w % 2;
      _pw_x -= _pw_x % 2;
    }

    void firstPage()
    {
      fillScreen(GxEPD_WHITE);
      _current_page = 0;
      _second_phase = false;
      epd2.setPaged(); // for GxEPD2_565c paged workaround
    }

    bool nextPage()
    {
      uint16_t page_ys = _current_page * _page_height;
      if (_using_partial_mode)
      {
        //Serial.print("  nextPage("); Serial.print(_pw_x); Serial.print(", "); Serial.print(_pw_y); Serial.print(", ");
        //Serial.print(_pw_w); Serial.print(", "); Serial.print(_pw_h); Serial.print(") P"); Serial.println(_current_page);
        uint16_t page_ye = _current_page < int16_t(_pages - 1) ? page_ys + _page_height : HEIGHT;
        uint16_t dest_ys = _pw_y + page_ys; // transposed
        uint16_t dest_ye = gx_uint16_min(_pw_y + _pw_h, _pw_y + page_ye);
        if (dest_ye > dest_ys)
        {
          //Serial.print("writeImage("); Serial.print(_pw_x); Serial.print(", "); Serial.print(dest_ys); Serial.print(", ");
          //Serial.print(_pw_w); Serial.print(", "); Serial.print(dest_ye - dest_ys); Serial.println(")");
          epd2.writeNative(_pixel_buffer, 0, _pw_x, dest_ys, _pw_w, dest_ye - dest_ys);
        }
        else
        {
          //Serial.print("writeImage("); Serial.print(_pw_x); Serial.print(", "); Serial.print(dest_ys); Serial.print(", ");
          //Serial.print(_pw_w); Serial.print(", "); Serial.print(dest_ye - dest_ys); Serial.print(") skipped ");
          //Serial.print(dest_ys); Serial.print(".."); Serial.println(dest_ye);
        }
        _current_page++;
        if (_current_page == int16_t(_pages))
        {
          _current_page = 0;
          if (!_second_phase)
          {
            epd2.refresh(_pw_x, _pw_y, _pw_w, _pw_h);
            if (epd2.hasFastPartialUpdate)
            {
              _second_phase = true;
              return true;
            }
          }
          return false;
        }
        fillScreen(GxEPD_WHITE);
        return true;
      }
      else // full update
      {
        epd2.writeNative(_pixel_buffer, 0, 0, page_ys, GxEPD2_Type::WIDTH, gx_uint16_min(_page_height, HEIGHT - page_ys));
        _current_page++;
        if (_current_page == int16_t(_pages))
        {
          _current_page = 0;
          if ((epd2.panel == GxEPD2::GDEW0154Z04) && (_pages > 1))
          {
            if (!_second_phase)
            {
              epd2.refresh(false); // full update after first phase
              _second_phase = true;
              fillScreen(GxEPD_WHITE);
              return true;
            }
            else epd2.refresh(true); // partial update after second phase
          } else epd2.refresh(false); // full update after only phase
          epd2.powerOff();
          return false;
        }
        fillScreen(GxEPD_WHITE);
        return true;
      }
    }

    // GxEPD style paged drawing; drawCallback() is called as many times as needed
    void drawPaged(void (*drawCallback)(const void*), const void* pv)
    {
      if (_using_partial_mode)
      {
        for (_current_page = 0; _current_page < _pages; _current_page++)
        {
          uint16_t page_ys = _current_page * _page_height;
          uint16_t page_ye = _current_page < (_pages - 1) ? page_ys + _page_height : HEIGHT;
          uint16_t dest_ys = _pw_y + page_ys; // transposed
          uint16_t dest_ye = gx_uint16_min(_pw_y + _pw_h, _pw_y + page_ye);
          if (dest_ye > dest_ys)
          {
            fillScreen(GxEPD_WHITE);
            drawCallback(pv);
            epd2.writeNative(_pixel_buffer, 0, _pw_x, dest_ys, _pw_w, dest_ye - dest_ys);
          }
        }
        epd2.refresh(_pw_x, _pw_y, _pw_w, _pw_h);
      }
      else // full update
      {
        epd2.setPaged(); // for GxEPD2_154c paged workaround
        for (_current_page = 0; _current_page < _pages; _current_page++)
        {
          uint16_t page_ys = _current_page * _page_height;
          fillScreen(GxEPD_WHITE);
          drawCallback(pv);
          epd2.writeNative(_pixel_buffer, 0, 0, page_ys, GxEPD2_Type::WIDTH, gx_uint16_min(_page_height, HEIGHT - page_ys));
        }
        epd2.refresh(false); // full update
        epd2.powerOff();
      }
      _current_page = 0;
    }

    void drawInvertedBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h, uint16_t color)
    {
      // taken from Adafruit_GFX.cpp, modified
      int16_t byteWidth = (w + 7) / 8; // Bitmap scanline pad = whole byte
      uint8_t byte = 0;
      for (int16_t j = 0; j < h; j++)
      {
        for (int16_t i = 0; i < w; i++ )
        {
          if (i & 7) byte <<= 1;
          else
          {
#if defined(__AVR) || defined(ESP8266) || defined(ESP32)
            byte = pgm_read_byte(&bitmap[j * byteWidth + i / 8]);
#else
            byte = bitmap[j * byteWidth + i / 8];
#endif
          }
          if (!(byte & 0x80))
          {
            drawPixel(x + i, y + j, color);
          }
        }
      }
    }

    //  Support for Bitmaps (Sprites) to Controller Buffer and to Screen
    void clearScreen(uint8_t value = 0xFF) // init controller memory and screen (default white)
    {
      epd2.clearScreen(value);
    }
    void writeScreenBuffer(uint8_t value = 0xFF) // init controller memory (default white)
    {
      epd2.writeScreenBuffer(value);
    }
    // write to controller memory, without screen refresh; x and w should be multiple of 8
    void writeImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false)
    {
      epd2.writeImage(bitmap, x, y, w, h, invert, mirror_y, pgm);
    }
    void writeImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                        int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false)
    {
      epd2.writeImagePart(bitmap, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
    }
    void writeImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
    {
      epd2.writeImage(black, color, x, y, w, h, invert, mirror_y, pgm);
    }
    void writeImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h)
    {
      epd2.writeImage(black, color, x, y, w, h, false, false, false);
    }
    void writeImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                        int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
    {
      epd2.writeImagePart(black, color, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
    }
    void writeImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                        int16_t x, int16_t y, int16_t w, int16_t h)
    {
      epd2.writeImagePart(black, color, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, false, false, false);
    }
    // write sprite of native data to controller memory, without screen refresh; x and w should be multiple of 8
    void writeNative(const uint8_t* data1, const uint8_t* data2, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
    {
      epd2.writeNative(data1, data2, x, y, w, h, invert, mirror_y, pgm);
    }
    // write to controller memory, with screen refresh; x and w should be multiple of 8
    void drawImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false)
    {
      epd2.drawImage(bitmap, x, y, w, h, invert, mirror_y, pgm);
    }
    void drawImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                       int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false)
    {
      epd2.drawImagePart(bitmap, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
    }
    void drawImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
    {
      epd2.drawImage(black, color, x, y, w, h, invert, mirror_y, pgm);
    }
    void drawImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h)
    {
      epd2.drawImage(black, color, x, y, w, h, false, false, false);
    }
    void drawImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                       int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
    {
      epd2.drawImagePart(black, color, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
    }
    void drawImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                       int16_t x, int16_t y, int16_t w, int16_t h)
    {
      epd2.drawImagePart(black, color, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, false, false, false);
    }
    // write sprite of native data to controller memory, with screen refresh; x and w should be multiple of 8
    void drawNative(const uint8_t* data1, const uint8_t* data2, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
    {
      epd2.drawNative(data1, data2, x, y, w, h, invert, mirror_y, pgm);
    }
    void refresh(bool partial_update_mode = false) // screen refresh from controller memory to full screen
    {
      epd2.refresh(partial_update_mode);
      if (!partial_update_mode) epd2.powerOff();
    }
    void refresh(int16_t x, int16_t y, int16_t w, int16_t h) // screen refresh from controller memory, partial screen
    {
      epd2.refresh(x, y, w, h);
    }
    // turns off generation of panel driving voltages, avoids screen fading over time
    void powerOff()
    {
      epd2.powerOff();
    }
    // turns powerOff() and sets controller to deep sleep for minimum power use, ONLY if wakeable by RST (rst >= 0)
    void hibernate()
    {
      epd2.hibernate();
    }
  private:
    template <typename T> static inline void
    _swap_(T & a, T & b)
    {
      T t = a;
      a = b;
      b = t;
    };
    static inline uint16_t gx_uint16_min(uint16_t a, uint16_t b)
    {
      return (a < b ? a : b);
    };
    static inline uint16_t gx_uint16_max(uint16_t a, uint16_t b)
    {
      return (a > b ? a : b);
    };
    void _rotate(uint16_t& x, uint16_t& y, uint16_t& w, uint16_t& h)
    {
      switch (getRotation())
      {
        case 1:
          _swap_(x, y);
          _swap_(w, h);
          x = WIDTH - x - w;
          break;
        case 2:
          x = WIDTH - x - w;
          y = HEIGHT - y - h;
          break;
        case 3:
          _swap_(x, y);
          _swap_(w, h);
          y = HEIGHT - y - h;
          break;
      }
    }
    uint8_t color7(uint16_t color)
    {
      static uint16_t _prev_color = GxEPD_BLACK;
      static uint8_t _prev_color7 = 0x00; // black
      if (color == _prev_color) return _prev_color7;
      uint8_t cv7 = 0x00;
      switch (color)
      {
        case GxEPD_BLACK: cv7 = 0x00; break;
        case GxEPD_WHITE: cv7 = 0x01; break;
        case GxEPD_GREEN: cv7 = 0x02; break;
        case GxEPD_BLUE:  cv7 = 0x03; break;
        case GxEPD_RED:   cv7 = 0x04; break;
        case GxEPD_YELLOW: cv7 = 0x05; break;
        case GxEPD_ORANGE: cv7 = 0x06; break;
        default:
          {
            uint16_t red = color & 0xF800;
            uint16_t green = (color & 0x07E0) << 5;
            uint16_t blue = (color & 0x001F) << 11;
            if ((red < 0x8000) && (green < 0x8000) && (blue < 0x8000)) cv7 = 0x00; // black
            else if ((red >= 0x8000) && (green >= 0x8000) && (blue >= 0x8000)) cv7 = 0x01; // white
            else if ((red >= 0x8000) && (blue >= 0x8000)) cv7 = red > blue ? 0x04 : 0x03; // red, blue
            else if ((green >= 0x8000) && (blue >= 0x8000)) cv7 = green > blue ? 0x02 : 0x03; // green, blue
            else if ((red >= 0x8000) && (green >= 0xC000)) cv7 = 0x05; // yellow
            else if ((red >= 0x8000) && (green >= 0x4000)) cv7 = 0x06; // orange
            else if (red >= 0x8000) cv7 = 0x04; // red
            else if (green >= 0x8000) cv7 = 0x02; // green
            else cv7 = 0x03; // blue
          }
      }
      _prev_color = color;
      _prev_color7 = cv7;
      return cv7;
    }
  private:
    uint8_t _pixel_buffer[(GxEPD2_Type::WIDTH / 2) * page_height];
    bool _using_partial_mode, _second_phase, _mirror;
    uint16_t _width_bytes, _pixel_bytes;
    int16_t _current_page;
    uint16_t _pages, _page_height;
    uint16_t _pw_x, _pw_y, _pw_w, _pw_h;
};

#endif
