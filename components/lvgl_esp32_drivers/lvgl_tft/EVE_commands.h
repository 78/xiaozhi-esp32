/*
@file    EVE_commands.h
@brief   contains FT8xx / BT8xx function prototypes
@version 4.1 LvGL edition
@date    2020-04-13
@author  Rudolph Riedel, David Jade

@section LICENSE

MIT License

Copyright (c) 2016-2020 Rudolph Riedel and David Jade

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

@section History

4.1 LvGL edition

	- This version is a heavily modified version of the MIT licensed FT81x code from https://github.com/RudolphRiedel/FT800-FT813
	  This version is based on a fork by David Jade that added native SPI DMA support and stripped out non-ESP32 code.
	  It has also been trimmed down to suit LvGL's needs. Extra features can be enabled by defining FT81X_FULL
*/

#include "EVE.h"

#ifndef EVE_COMMANDS_H_
#define EVE_COMMANDS_H_

#define BLOCK_TRANSFER_SIZE 3840		// block transfer size when write data to CMD buffer

void DELAY_MS(uint16_t ms);

#if EVE_USE_PDN
void EVE_pdn_set(void);
void EVE_pdn_clear(void);
#endif

void spi_acquire();
void spi_release();

void EVE_cmdWrite(uint8_t command, uint8_t parameter);

uint8_t EVE_memRead8(uint32_t ftAddress);
uint16_t EVE_memRead16(uint32_t ftAddress);
uint32_t EVE_memRead32(uint32_t ftAddress);

void EVE_memWrite8(uint32_t ftAddress, uint8_t ftData8);
void EVE_memWrite16(uint32_t ftAddress, uint16_t ftData16);
void EVE_memWrite32(uint32_t ftAddress, uint32_t ftData32);

void EVE_memWrite_buffer(uint32_t ftAddress, const uint8_t *data, uint32_t len, bool LvGL_Flush);

uint8_t EVE_busy(void);

void EVE_get_cmdoffset(void);


/* commands to operate on memory: */
void EVE_cmd_memzero(uint32_t ptr, uint32_t num);
void EVE_cmd_memset(uint32_t ptr, uint8_t value, uint32_t num);
void EVE_cmd_memwrite(uint32_t dest, uint32_t num, const uint8_t *data);
void EVE_cmd_memcpy(uint32_t dest, uint32_t src, uint32_t num);

#if FT81X_FULL
/* commands for loading image data into FT8xx memory: */
void EVE_cmd_inflate(uint32_t ptr, const uint8_t *data, uint16_t len);
void EVE_cmd_loadimage(uint32_t ptr, uint32_t options, const uint8_t *data, uint16_t len);

#if defined (FT81X_ENABLE)
void EVE_cmd_mediafifo(uint32_t ptr, uint32_t size);
#endif
#endif // FT81X_FULL

void EVE_cmd_start(void);
void EVE_cmd_execute(void);

void EVE_start_cmd_burst(void);
void EVE_end_cmd_burst(void);

void EVE_cmd_dl(uint32_t command);


#if FT81X_FULL
/* EVE3 commands */
#if defined (BT81X_ENABLE)

void EVE_cmd_flashwrite(uint32_t ptr, uint32_t num, const uint8_t *data);
void EVE_cmd_flashread(uint32_t dest, uint32_t src, uint32_t num);
void EVE_cmd_flashupdate(uint32_t dest, uint32_t src, uint32_t num);
void EVE_cmd_flasherase(void);
void EVE_cmd_flashattach(void);
void EVE_cmd_flashdetach(void);
void EVE_cmd_flashspidesel(void);
uint32_t EVE_cmd_flashfast(void);
void EVE_cmd_flashspitx(uint32_t num, const uint8_t *data);
void EVE_cmd_flashspirx(uint32_t dest, uint32_t num);
void EVE_cmd_flashsource(uint32_t ptr);

void EVE_cmd_inflate2(uint32_t ptr, uint32_t options, const uint8_t *data, uint16_t len);
void EVE_cmd_rotatearound(int32_t x0, int32_t y0, int32_t angle, int32_t scale);
void EVE_cmd_animstart(int32_t ch, uint32_t aoptr, uint32_t loop);
void EVE_cmd_animstop(int32_t ch);
void EVE_cmd_animxy(int32_t ch, int16_t x0, int16_t y0);
void EVE_cmd_animdraw(int32_t ch);
void EVE_cmd_animframe(int16_t x0, int16_t y0, uint32_t aoptr, uint32_t frame);
void EVE_cmd_gradienta(int16_t x0, int16_t y0, uint32_t argb0, int16_t x1, int16_t y1, uint32_t argb1);
void EVE_cmd_fillwidth(uint32_t s);
void EVE_cmd_appendf(uint32_t ptr, uint32_t num);

uint8_t EVE_init_flash(void);
#endif


/* commands to draw graphics objects: */

#if defined (BT81X_ENABLE)
void EVE_cmd_text_var(int16_t x0, int16_t y0, int16_t font, uint16_t options, const char* text, uint8_t numargs, ...);
void EVE_cmd_button_var(int16_t x0, int16_t y0, int16_t w0, int16_t h0, int16_t font, uint16_t options, const char* text, uint8_t num_args, ...);
void EVE_cmd_toggle_var(int16_t x0, int16_t y0, int16_t w0, int16_t font, uint16_t options, uint16_t state, const char* text, uint8_t num_args, ...);
#endif

void EVE_cmd_text(int16_t x0, int16_t y0, int16_t font, uint16_t options, const char* text);
void EVE_cmd_button(int16_t x0, int16_t y0, int16_t w0, int16_t h0, int16_t font, uint16_t options, const char* text);
void EVE_cmd_clock(int16_t x0, int16_t y0, int16_t r0, uint16_t options, uint16_t hours, uint16_t minutes, uint16_t seconds, uint16_t millisecs);
void EVE_color_rgb(uint8_t red, uint8_t green, uint8_t blue);
void EVE_cmd_bgcolor(uint32_t color);
void EVE_cmd_fgcolor(uint32_t color);
void EVE_cmd_gradcolor(uint32_t color);
void EVE_cmd_gauge(int16_t x0, int16_t y0, int16_t r0, uint16_t options, uint16_t major, uint16_t minor, uint16_t val, uint16_t range);
void EVE_cmd_gradient(int16_t x0, int16_t y0, uint32_t rgb0, int16_t x1, int16_t y1, uint32_t rgb1);
void EVE_cmd_keys(int16_t x0, int16_t y0, int16_t w0, int16_t h0, int16_t font, uint16_t options, const char* text);
void EVE_cmd_progress(int16_t x0, int16_t y0, int16_t w0, int16_t h0, uint16_t options, uint16_t val, uint16_t range);
void EVE_cmd_scrollbar(int16_t x0, int16_t y0, int16_t w0, int16_t h0, uint16_t options, uint16_t val, uint16_t size, uint16_t range);
void EVE_cmd_slider(int16_t x1, int16_t y1, int16_t w1, int16_t h1, uint16_t options, uint16_t val, uint16_t range);
void EVE_cmd_dial(int16_t x0, int16_t y0, int16_t r0, uint16_t options, uint16_t val);
void EVE_cmd_toggle(int16_t x0, int16_t y0, int16_t w0, int16_t font, uint16_t options, uint16_t state, const char* text);
void EVE_cmd_number(int16_t x0, int16_t y0, int16_t font, uint16_t options, int32_t number);
#endif // FT81X_FULL

#if defined (FT81X_ENABLE)
#if FT81X_FULL
void EVE_cmd_setbase(uint32_t base);
#endif
void EVE_cmd_setbitmap(uint32_t addr, uint16_t fmt, uint16_t width, uint16_t height);
#endif


#if FT81X_FULL
void EVE_cmd_append(uint32_t ptr, uint32_t num);


/* commands for setting the bitmap transform matrix: */
void EVE_cmd_getmatrix(int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f);
void EVE_cmd_translate(int32_t tx, int32_t ty);
void EVE_cmd_scale(int32_t sx, int32_t sy);
void EVE_cmd_rotate(int32_t ang);


/* other commands: */
void EVE_cmd_calibrate(void);
void EVE_cmd_interrupt(uint32_t ms);
void EVE_cmd_setfont(uint32_t font, uint32_t ptr);
#if defined (FT81X_ENABLE)
void EVE_cmd_romfont(uint32_t font, uint32_t romslot);
void EVE_cmd_setfont2(uint32_t font, uint32_t ptr, uint32_t firstchar);
void EVE_cmd_setrotate(uint32_t r);
void EVE_cmd_setscratch(uint32_t handle);
#endif
void EVE_cmd_sketch(int16_t x0, int16_t y0, uint16_t w0, uint16_t h0, uint32_t ptr, uint16_t format);
void EVE_cmd_snapshot(uint32_t ptr);
#if defined (FT81X_ENABLE)
void EVE_cmd_snapshot2(uint32_t fmt, uint32_t ptr, int16_t x0, int16_t y0, int16_t w0, int16_t h0);
#endif
void EVE_cmd_spinner(int16_t x0, int16_t y0, uint16_t style, uint16_t scale);
void EVE_cmd_track(int16_t x0, int16_t y0, int16_t w0, int16_t h0, int16_t tag);


/* commands that return values by writing to the command-fifo */
uint32_t EVE_cmd_memcrc(uint32_t ptr, uint32_t num);
uint32_t EVE_cmd_getptr(void);
uint32_t EVE_cmd_regread(uint32_t ptr);
void EVE_LIB_GetProps(uint32_t *pointer, uint32_t *width, uint32_t *height);


/* meta-commands, sequences of several display-list entries condensed into simpler to use functions at the price of some overhead */
void EVE_cmd_point(int16_t x0, int16_t y0, uint16_t size);
void EVE_cmd_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t w0);
void EVE_cmd_rect(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t corner);

void EVE_calibrate_manual(uint16_t height);
#endif // FT81X_FULL


/* startup FT8xx: */
uint8_t EVE_init(void);

#endif /* EVE_COMMANDS_H_ */
