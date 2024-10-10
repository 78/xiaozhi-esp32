/*
@file    EVE_config.h
@brief   configuration information for some TFTs
@version 4.0
@date    2020-02-16
@author  Rudolph Riedel, David Jade

@section LICENSE

MIT License

Copyright (c) 2016-2020 Rudolph Riedel amd David Jade

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


#ifndef EVE_CONFIG_H_
#define EVE_CONFIG_H_

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif
#include "../lvgl_spi_conf.h"

#include "FT81x.h"

#define EVE_CLK		    DISP_SPI_CLK				    // orange
#define EVE_MISO	    DISP_SPI_MISO				    // yellow
#define EVE_MOSI	    DISP_SPI_MOSI				    // green
#define EVE_CS 		    DISP_SPI_CS					    // blue
#define EVE_PDN		    CONFIG_LV_DISP_PIN_RST	// grey
#define EVE_USE_PDN		CONFIG_LV_DISP_USE_RST

#define SPI_TRANSER_SIZE (DISP_BUF_SIZE * (LV_COLOR_DEPTH / 8))

#define BYTES_PER_PIXEL (LV_COLOR_DEPTH / 8)	// bytes per pixel for (16 for RGB565)
#define BYTES_PER_LINE (EVE_HSIZE * BYTES_PER_PIXEL)
#define SCREEN_BUFFER_SIZE (EVE_HSIZE * EVE_VSIZE * BYTES_PER_PIXEL)

#define SPI_BUFFER_SIZE 256				// size in bytes (multiples of 4) of SPI transaction buffer for streaming commands

/* select the settings for the TFT attached */
#if 0
	#define EVE_VM800B35A
	#define EVE_VM800B43A
	#define EVE_VM800B50A
	#define EVE_VM810C
	#define EVE_ME812A
	#define EVE_ME813A
	#define EVE_FT810CB_HY50HD
	#define EVE_FT811CB_HY50HD
	#define EVE_ET07
	#define EVE_RVT28
	#define EVE_RVT35
	#define EVE_RVT43
	#define EVE_RVT50
	#define EVE_RVT70
	#define EVE_RiTFT43
	#define EVE_RiTFT50
	#define EVE_RiTFT70
	#define EVE_EVE2_29
	#define EVE_EVE2_35
	#define EVE_EVE2_35G
	#define EVE_EVE2_38
	#define EVE_EVE2_38G
	#define EVE_EVE2_43
	#define EVE_EVE2_43G
	#define EVE_EVE2_50
	#define EVE_EVE2_50G
	#define EVE_EVE2_70
	#define EVE_EVE2_70G
	#define EVE_EVE3_35
	#define EVE_EVE3_35G
	#define EVE_EVE3_43
	#define EVE_EVE3_43G
	#define EVE_EVE3_50
	#define EVE_EVE3_50G
	#define EVE_EVE3_70
	#define EVE_EVE3_70G
	#define EVE_NHD_35
	#define EVE_NHD_43
	#define EVE_NHD_50
	#define EVE_NHD_70
	#define EVE_ADAM101
	#define EVE_CFAF240400C1_030SC
	#define EVE_CFAF320240F_035T
	#define EVE_CFAF480128A0_039TC
	#define EVE_CFAF800480E0_050SC
	#define EVE_PAF90
	#define EVE_SUNFLOWER
	#define EVE_CONNECTEVE
#endif


#if defined(CONFIG_LV_FT81X_CONFIG_EVE_VM800B35A)
#define EVE_VM800B35A
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_VM800B43A)
#define EVE_VM800B43A
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_VM800B50A)
#define EVE_VM800B50A
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_VM810C)
#define EVE_VM810C
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_ME812A)
#define EVE_ME812A
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_ME813A)
#define EVE_ME813A
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_FT810CB_HY50HD)
#define EVE_FT810CB_HY50HD
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_FT811CB_HY50HD)
#define EVE_FT811CB_HY50HD
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_ET07)
#define EVE_ET07
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_RVT28)
#define EVE_RVT28
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_RVT35)
#define EVE_RVT35
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_RVT43)
#define EVE_RVT43
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_RVT50)
#define EVE_RVT50
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_RVT70)
#define EVE_RVT70
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_RiTFT43)
#define EVE_RiTFT43
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_RiTFT50)
#define EVE_RiTFT50
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_RiTFT70)
#define EVE_RiTFT70
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_EVE2_29)
#define EVE_EVE2_29
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_EVE2_35)
#define EVE_EVE2_35
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_EVE2_35G)
#define EVE_EVE2_35G
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_EVE2_38)
#define EVE_EVE2_38
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_EVE2_38G)
#define EVE_EVE2_38G
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_EVE2_43)
#define EVE_EVE2_43
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_EVE2_43G)
#define EVE_EVE2_43G
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_EVE2_50)
#define EVE_EVE2_50
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_EVE2_50G)
#define EVE_EVE2_50G
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_EVE2_70)
#define EVE_EVE2_70
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_EVE2_70G)
#define EVE_EVE2_70G
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_EVE3_35)
#define EVE_EVE3_35
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_EVE3_35G)
#define EVE_EVE3_35G
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_EVE3_43)
#define EVE_EVE3_43
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_EVE3_43G)
#define EVE_EVE3_43G
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_EVE3_50)
#define EVE_EVE3_50
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_EVE3_50G)
#define EVE_EVE3_50G
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_EVE3_70)
#define EVE_EVE3_70
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_EVE3_70G)
#define EVE_EVE3_70G
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_NHD_35)
#define EVE_NHD_35
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_NHD_43)
#define EVE_NHD_43
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_NHD_50)
#define EVE_NHD_50
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_NHD_70)
#define EVE_NHD_70
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_ADAM101)
#define EVE_ADAM101
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_CFAF240400C1_030SC)
#define EVE_CFAF240400C1_030SC
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_CFAF320240F_035T)
#define EVE_CFAF320240F_035T
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_CFAF480128A0_039TC)
#define EVE_CFAF480128A0_039TC
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_CFAF800480E0_050SC)
#define EVE_CFAF800480E0_050SC
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_PAF90)
#define EVE_PAF90
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_SUNFLOWER)
#define EVE_SUNFLOWER
#elif defined(CONFIG_LV_FT81X_CONFIG_EVE_CONNECTEVE)
#define EVE_CONNECTEVE
#endif

/* display timing parameters below */

/* untested */
#if defined (EVE_EVE3_35)
#define EVE_EVE2_35
#define EVE_HAS_CRYSTAL
#define BT81X_ENABLE
#endif

/* untested */
#if defined (EVE_EVE3_35G)
#define EVE_EVE2_35G
#define EVE_HAS_CRYSTAL
#define BT81X_ENABLE
#endif

/* untested */
#if defined (EVE_EVE3_43)
#define EVE_EVE2_43
#define EVE_HAS_CRYSTAL
#define BT81X_ENABLE
#endif

#if defined (EVE_EVE3_43G)
#define EVE_EVE2_43G
#define EVE_HAS_CRYSTAL
#define BT81X_ENABLE
#endif

/* untested */
#if defined (EVE_EVE3_50)
#define EVE_EVE2_50
#define EVE_HAS_CRYSTAL
#define BT81X_ENABLE
#endif

#if defined (EVE_EVE3_50G)
#define EVE_EVE2_50G
#define EVE_HAS_CRYSTAL
#define BT81X_ENABLE
#endif

/* untested */
#if defined (EVE_EVE3_70)
#define EVE_EVE2_70
#define EVE_HAS_CRYSTAL
#define BT81X_ENABLE
#endif

/* untested */
#if defined (EVE_EVE3_70G)
#define EVE_EVE2_70G
#define EVE_HAS_CRYSTAL
#define BT81X_ENABLE
#endif

#if defined (EVE_RiTFT43)
#define EVE_RVT43
#define EVE_HAS_CRYSTAL
#define FT81X_ENABLE
#define BT81X_ENABLE
#endif

/* untested */
#if defined (EVE_RiTFT50)
#define EVE_RVT70
#define EVE_HAS_CRYSTAL
#define BT81X_ENABLE
#endif

/* untested */
#if defined (EVE_RiTFT70)
#define EVE_RVT70
#define EVE_HAS_CRYSTAL
#define BT81X_ENABLE
#endif


/* some test setup */
#if defined (EVE_800x480x)
#define EVE_HSIZE	(800L)	/* Thd Length of visible part of line (in PCLKs) - display width */
#define EVE_VSIZE	(480L)	/* Tvd Number of visible lines (in lines) - display height */

#define EVE_VSYNC0	(0L)	/* Tvf Vertical Front Porch */
#define EVE_VSYNC1	(10L)	/* Tvf + Tvp Vertical Front Porch plus Vsync Pulse width */
#define EVE_VOFFSET	(35L)	/* Tvf + Tvp + Tvb Number of non-visible lines (in lines) */
#define EVE_VCYCLE	(516L)	/* Tv Total number of lines (visible and non-visible) (in lines) */
#define EVE_HSYNC0	(0L)	 /* (40L)	// Thf Horizontal Front Porch */
#define EVE_HSYNC1	(88L)	/* Thf + Thp Horizontal Front Porch plus Hsync Pulse width */
#define EVE_HOFFSET	(169L)	/* Thf + Thp + Thb Length of non-visible part of line (in PCLK cycles) */
#define EVE_HCYCLE 	(969L)	/* Th Total length of line (visible and non-visible) (in PCLKs) */
#define EVE_PCLKPOL	(1L)	/* PCLK polarity (0 = rising edge, 1 = falling edge) */
#define EVE_SWIZZLE	(0L)	/* Defines the arrangement of the RGB pins of the FT800 */
#define EVE_PCLK	(2L)	/* 60MHz / REG_PCLK = PCLK frequency	30 MHz */
#define EVE_CSPREAD	(1L)	/* helps with noise, when set to 1 fewer signals are changed simultaneously, reset-default: 1 */
#define EVE_TOUCH_RZTHRESH (1200L)	/* touch-sensitivity */
#define EVE_HAS_CRYSTAL
#define FT81X_ENABLE
#endif


/* VM800B35A: FT800 320x240 3.5" FTDI FT800 */
#if defined (EVE_VM800B35A)
#define EVE_HSIZE	(320L)	/* Thd Length of visible part of line (in PCLKs) - display width */
#define EVE_VSIZE	(240L)	/* Tvd Number of visible lines (in lines) - display height */

#define EVE_VSYNC0	(0L)	/* Tvf Vertical Front Porch */
#define EVE_VSYNC1	(2L)	/* Tvf + Tvp Vertical Front Porch plus Vsync Pulse width */
#define EVE_VOFFSET	(13L)	/* Tvf + Tvp + Tvb Number of non-visible lines (in lines) */
#define EVE_VCYCLE	(263L)	/* Tv Total number of lines (visible and non-visible) (in lines) */
#define EVE_HSYNC0	(0L)	/* Thf Horizontal Front Porch */
#define EVE_HSYNC1	(10L)	/* Thf + Thp Horizontal Front Porch plus Hsync Pulse width */
#define EVE_HOFFSET	(70L)	/* Thf + Thp + Thb Length of non-visible part of line (in PCLK cycles) */
#define EVE_HCYCLE 	(408L)	/* Th Total length of line (visible and non-visible) (in PCLKs) */
#define EVE_PCLKPOL	(0L)	/* PCLK polarity (0 = rising edge, 1 = falling edge) */
#define EVE_SWIZZLE	(2L)	/* Defines the arrangement of the RGB pins of the FT800 */
#define EVE_PCLK	(8L)	/* 48MHz / REG_PCLK = PCLK frequency */
#define EVE_CSPREAD	(1L)	/* helps with noise, when set to 1 fewer signals are changed simultaneously, reset-default: 1 */
#define EVE_TOUCH_RZTHRESH (1200L)	/* touch-sensitivity */
#define EVE_HAS_CRYSTAL		/* use external crystal or internal oscillator? */
#endif


/* FTDI/BRT EVE modules VM800B43A and VM800B50A  FT800 480x272 4.3" and 5.0" */
#if defined (EVE_VM800B43A) || defined (EVE_VM800B50A)
#define EVE_HSIZE	(480L)
#define EVE_VSIZE	(272L)

#define EVE_VSYNC0	(0L)
#define EVE_VSYNC1	(10L)
#define EVE_VOFFSET	(12L)
#define EVE_VCYCLE	(292L)
#define EVE_HSYNC0	(0L)
#define EVE_HSYNC1	(41L)
#define EVE_HOFFSET	(43L)
#define EVE_HCYCLE 	(548L)
#define EVE_PCLKPOL	(1L)
#define EVE_SWIZZLE	(0L)
#define EVE_PCLK	(5L)
#define EVE_CSPREAD	(1L)
#define EVE_TOUCH_RZTHRESH (1200L)
#define EVE_HAS_CRYSTAL
#endif


/* untested */
/* FTDI/BRT EVE2 modules VM810C50A-D, ME812A-WH50R and ME813A-WH50C, 800x480 5.0" */
#if defined (EVE_VM810C) || defined (EVE_ME812A) || defined (EVE_ME813A)
#define EVE_HSIZE	(800L)
#define EVE_VSIZE	(480L)

#define EVE_VSYNC0	(0L)
#define EVE_VSYNC1	(3L)
#define EVE_VOFFSET	(32L)
#define EVE_VCYCLE	(525L)
#define EVE_HSYNC0	(0L)
#define EVE_HSYNC1	(48L)
#define EVE_HOFFSET	(88L)
#define EVE_HCYCLE 	(928L)
#define EVE_PCLKPOL	(1L)
#define EVE_SWIZZLE	(0L)
#define EVE_PCLK	(2L)
#define EVE_CSPREAD	(0L)
#define EVE_TOUCH_RZTHRESH (1200L)
#define EVE_HAS_CRYSTAL
#define FT81X_ENABLE
#endif


/* FT810CB-HY50HD: FT810 800x480 5.0" HAOYU */
#if defined (EVE_FT810CB_HY50HD)
#define EVE_HSIZE	(800L)
#define EVE_VSIZE	(480L)

#define EVE_VSYNC0	(0L)
#define EVE_VSYNC1	(2L)
#define EVE_VOFFSET	(13L)
#define EVE_VCYCLE	(525L)
#define EVE_HSYNC0	(0L)
#define EVE_HSYNC1	(20L)
#define EVE_HOFFSET	(64L)
#define EVE_HCYCLE 	(952L)
#define EVE_PCLKPOL	(1L)
#define EVE_SWIZZLE	(0L)
#define EVE_PCLK	(2L)
#define EVE_CSPREAD	(1L)
#define EVE_TOUCH_RZTHRESH (2000L)	/* touch-sensitivity */
#define EVE_HAS_CRYSTAL
#define FT81X_ENABLE
#endif


/* FT811CB-HY50HD: FT811 800x480 5.0" HAOYU */
#if defined (EVE_FT811CB_HY50HD)
#define EVE_HSIZE	(800L)
#define EVE_VSIZE	(480L)

#define EVE_VSYNC0	(0L)
#define EVE_VSYNC1	(2L)
#define EVE_VOFFSET	(13L)
#define EVE_VCYCLE	(525L)
#define EVE_HSYNC0	(0L)
#define EVE_HSYNC1	(20L)
#define EVE_HOFFSET	(64L)
#define EVE_HCYCLE 	(952L)
#define EVE_PCLKPOL	(1L)
#define EVE_SWIZZLE	(0L)
#define EVE_PCLK	(2L)
#define EVE_CSPREAD	(1L)
#define EVE_TOUCH_RZTHRESH (1200L)	/* touch-sensitivity */
#define EVE_HAS_CRYSTAL
#define FT81X_ENABLE
#endif


/* untested */
/* G-ET0700G0DM6 800x480 7.0" Glyn */
#if defined (EVE_ET07)
#define EVE_HSIZE	(800L)
#define EVE_VSIZE	(480L)

#define EVE_VSYNC0	(0L)
#define EVE_VSYNC1	(2L)
#define EVE_VOFFSET	(35L)
#define EVE_VCYCLE	(525L)
#define EVE_HSYNC0	(0L)
#define EVE_HSYNC1	(128L)
#define EVE_HOFFSET (203L)
#define EVE_HCYCLE 	(1056L)
#define EVE_PCLKPOL (1L)
#define EVE_SWIZZLE (0L)
#define EVE_PCLK	(2L)
#define EVE_CSPREAD	(1L)
#define EVE_TOUCH_RZTHRESH (1200L)
#define FT81X_ENABLE
#endif


/* untested */
/* RVT28 240x320 2.8" Riverdi, various options, FT800/FT801 */
#if defined (EVE_RVT28)
#define EVE_HSIZE	(320L)
#define EVE_VSIZE	(240L)

#define EVE_VSYNC0	(0L)
#define EVE_VSYNC1	(2L)
#define EVE_VOFFSET	(2L)
#define EVE_VCYCLE	(326L)
#define EVE_HSYNC0	(0L)
#define EVE_HSYNC1	(10L)
#define EVE_HOFFSET	(20L)
#define EVE_HCYCLE 	(270L)
#define EVE_PCLKPOL	(0L)
#define EVE_SWIZZLE	(4L)
#define EVE_PCLK	(5L)
#define EVE_CSPREAD	(1L)
#define EVE_TOUCH_RZTHRESH (1200L)
#endif


/* untested */
/* RVT3.5 320x240 3.5" Riverdi, various options, FT800/FT801 */
#if defined (EVE_RVT35)
#define EVE_HSIZE	(320L)
#define EVE_VSIZE	(240L)

#define EVE_VSYNC0	(0L)
#define EVE_VSYNC1	(2L)
#define EVE_VOFFSET	(13L)
#define EVE_VCYCLE	(263L)
#define EVE_HSYNC0	(0L)
#define EVE_HSYNC1	(10L)
#define EVE_HOFFSET	(70L)
#define EVE_HCYCLE 	(408L)
#define EVE_PCLKPOL	(1L)
#define EVE_SWIZZLE	(2L)
#define EVE_PCLK	(6L)
#define EVE_CSPREAD	(1L)
#define EVE_TOUCH_RZTHRESH (1200L)
#endif


/* untested */
/* RVT43 / RVT4.3 480x272 4.3" Riverdi, various options, FT800/FT801 */
#if defined (EVE_RVT43)
#define EVE_HSIZE	(480L)
#define EVE_VSIZE	(272L)

#define EVE_VSYNC0	(0L)
#define EVE_VSYNC1	(10L)
#define EVE_VOFFSET	(12L)
#define EVE_VCYCLE	(292L)
#define EVE_HSYNC0	(0L)
#define EVE_HSYNC1	(41L)
#define EVE_HOFFSET	(43L)
#define EVE_HCYCLE 	(548L)
#define EVE_PCLKPOL	(1L)
#define EVE_SWIZZLE	(0L)
#define EVE_PCLK	(5L)
#define EVE_CSPREAD	(1L)
#define EVE_TOUCH_RZTHRESH (1200L)
#endif


/* untested */
/* RVT50xQFxxxxx 800x480 5.0" Riverdi, various options, FT812/FT813 */
#if defined (EVE_RVT50)
#define EVE_HSIZE	(800L)
#define EVE_VSIZE	(480L)

#define EVE_VSYNC0	(0L)
#define EVE_VSYNC1	(3L)
#define EVE_VOFFSET	(32L)
#define EVE_VCYCLE	(525L)
#define EVE_HSYNC0	(0L)
#define EVE_HSYNC1	(48L)
#define EVE_HOFFSET (88L)
#define EVE_HCYCLE 	(928L)
#define EVE_PCLKPOL (1L)
#define EVE_SWIZZLE (0L)
#define EVE_PCLK	(2L)
#define EVE_CSPREAD	(1L)
#define EVE_TOUCH_RZTHRESH (1200L)
#define FT81X_ENABLE
#endif


/* RVT70xQFxxxxx 800x480 7.0" Riverdi, various options, FT812/FT813, tested with RVT70UQFNWC0x */
#if defined (EVE_RVT70)
#define EVE_HSIZE	(800L)	/* Thd Length of visible part of line (in PCLKs) - display width */
#define EVE_VSIZE	(480L)	/* Tvd Number of visible lines (in lines) - display height */

#define EVE_VSYNC0	(0L)	/* Tvf Vertical Front Porch */
#define EVE_VSYNC1	(10L)	/* Tvf + Tvp Vertical Front Porch plus Vsync Pulse width */
#define EVE_VOFFSET	(23L)	/* Tvf + Tvp + Tvb Number of non-visible lines (in lines) */
#define EVE_VCYCLE	(525L)	/* Tv Total number of lines (visible and non-visible) (in lines) */
#define EVE_HSYNC0	(0L)	/* Thf Horizontal Front Porch */
#define EVE_HSYNC1	(10L)	/* Thf + Thp Horizontal Front Porch plus Hsync Pulse width */
#define EVE_HOFFSET (46L)	/* Thf + Thp + Thb Length of non-visible part of line (in PCLK cycles) */
#define EVE_HCYCLE 	(1056L)	/* Th Total length of line (visible and non-visible) (in PCLKs) */
#define EVE_PCLKPOL (1L)	/* PCLK polarity (0 = rising edge, 1 = falling edge) */
#define EVE_SWIZZLE (0L)	/* Defines the arrangement of the RGB pins of the FT800 */
#define EVE_PCLK	(2L)	/* 60MHz / REG_PCLK = PCLK frequency 30 MHz */
#define EVE_CSPREAD	(1L)	/* helps with noise, when set to 1 fewer signals are changed simultaneously, reset-default: 1 */
#define EVE_TOUCH_RZTHRESH (1800L)	/* touch-sensitivity */
#define FT81X_ENABLE
#endif


/* untested */
/* EVE2-29A 320x102 2.9" 1U Matrix Orbital, non-touch, FT812 */
#if defined (EVE_EVE2_29)
#define EVE_HSIZE	(320L)
#define EVE_VSIZE	(102L)

#define EVE_VSYNC0	(0L)
#define EVE_VSYNC1	(2L)
#define EVE_VOFFSET	(156L)
#define EVE_VCYCLE	(262L)
#define EVE_HSYNC0	(0L)
#define EVE_HSYNC1	(10L)
#define EVE_HOFFSET	(70L)
#define EVE_HCYCLE 	(408L)
#define EVE_PCLKPOL	(0L)
#define EVE_SWIZZLE	(0L)
#define EVE_PCLK	(8L)
#define EVE_CSPREAD	(1L)
#define EVE_TOUCH_RZTHRESH (1200L)
#define FT81X_ENABLE
#endif


/* EVE2-35A 320x240 3.5" Matrix Orbital, resistive, or non-touch, FT812 */
#if defined (EVE_EVE2_35)
#define EVE_HSIZE	(320L)
#define EVE_VSIZE	(240L)

#define EVE_VSYNC0	(0L)
#define EVE_VSYNC1	(2L)
#define EVE_VOFFSET	(18L)
#define EVE_VCYCLE	(262L)
#define EVE_HSYNC0	(0L)
#define EVE_HSYNC1	(10L)
#define EVE_HOFFSET	(70L)
#define EVE_HCYCLE 	(408L)
#define EVE_PCLKPOL	(0L)
#define EVE_SWIZZLE	(0L)
#define EVE_PCLK	(8L)
#define EVE_CSPREAD	(1L)
#define EVE_TOUCH_RZTHRESH (1200L)
#define FT81X_ENABLE
#endif


/* EVE2-35G 320x240 3.5" Matrix Orbital, capacitive touch, FT813 */
#if defined (EVE_EVE2_35G)
#define EVE_HSIZE	(320L)
#define EVE_VSIZE	(240L)

#define EVE_VSYNC0	(0L)
#define EVE_VSYNC1	(2L)
#define EVE_VOFFSET	(18L)
#define EVE_VCYCLE	(262L)
#define EVE_HSYNC0	(0L)
#define EVE_HSYNC1	(10L)
#define EVE_HOFFSET	(70L)
#define EVE_HCYCLE 	(408L)
#define EVE_PCLKPOL	(0L)
#define EVE_SWIZZLE	(0L)
#define EVE_PCLK	(8L)
#define EVE_CSPREAD	(1L)
#define EVE_TOUCH_RZTHRESH (1200L)
#define EVE_HAS_GT911	/* special treatment required for out-of-spec touch-controller */
#define FT81X_ENABLE
#endif


/* EVE2-38A 480x116 3.8" 1U Matrix Orbital, resistive touch, FT812 */
#if defined (EVE_EVE2_38)
#define EVE_HSIZE	(480L)
#define EVE_VSIZE	(272L)

#define EVE_VSYNC0	(152L)
#define EVE_VSYNC1	(10L)
#define EVE_VOFFSET	(12L)
#define EVE_VCYCLE	(292L)
#define EVE_HSYNC0	(0L)
#define EVE_HSYNC1	(41L)
#define EVE_HOFFSET	(43L)
#define EVE_HCYCLE 	(548L)
#define EVE_PCLKPOL	(1L)
#define EVE_SWIZZLE	(0L)
#define EVE_PCLK	(5L)
#define EVE_CSPREAD	(1L)
#define EVE_TOUCH_RZTHRESH (1200L)
#define FT81X_ENABLE
#endif


/* EVE2-38G 480x116 3.8" 1U Matrix Orbital, capacitive touch, FT813 */
#if defined (EVE_EVE2_38G)
#define EVE_HSIZE	(480L)
#define EVE_VSIZE	(272L)

#define EVE_VSYNC0	(152L)
#define EVE_VSYNC1	(10L)
#define EVE_VOFFSET	(12L)
#define EVE_VCYCLE	(292L)
#define EVE_HSYNC0	(0L)
#define EVE_HSYNC1	(41L)
#define EVE_HOFFSET	(43L)
#define EVE_HCYCLE 	(548L)
#define EVE_PCLKPOL	(1L)
#define EVE_SWIZZLE	(0L)
#define EVE_PCLK	(5L)
#define EVE_CSPREAD	(1L)
#define EVE_TOUCH_RZTHRESH (1200L)
#define EVE_HAS_GT911	/* special treatment required for out-of-spec touch-controller */
#define FT81X_ENABLE
#endif


/* untested */
/* EVE2-43A 480x272 4.3" Matrix Orbital, resistive or no touch, FT812 */
#if defined (EVE_EVE2_43)
#define EVE_HSIZE	(480L)
#define EVE_VSIZE	(272L)

#define EVE_VSYNC0	(0L)
#define EVE_VSYNC1	(10L)
#define EVE_VOFFSET	(12L)
#define EVE_VCYCLE	(292L)
#define EVE_HSYNC0	(0L)
#define EVE_HSYNC1	(41L)
#define EVE_HOFFSET	(43L)
#define EVE_HCYCLE 	(548L)
#define EVE_PCLKPOL	(1L)
#define EVE_SWIZZLE	(0L)
#define EVE_PCLK	(5L)
#define EVE_CSPREAD	(1L)
#define EVE_TOUCH_RZTHRESH (1200L)
#define FT81X_ENABLE
#endif


/* EVE2-43G 480x272 4.3" Matrix Orbital, capacitive touch, FT813 */
#if defined (EVE_EVE2_43G)
#define EVE_HSIZE	(480L)
#define EVE_VSIZE	(272L)

#define EVE_VSYNC0	(0L)
#define EVE_VSYNC1	(10L)
#define EVE_VOFFSET	(12L)
#define EVE_VCYCLE	(292L)
#define EVE_HSYNC0	(0L)
#define EVE_HSYNC1	(41L)
#define EVE_HOFFSET	(43L)
#define EVE_HCYCLE 	(548L)
#define EVE_PCLKPOL	(1L)
#define EVE_SWIZZLE	(0L)
#define EVE_PCLK	(5L)
#define EVE_CSPREAD	(1L)
#define EVE_TOUCH_RZTHRESH (1200L)
#define EVE_HAS_GT911	/* special treatment required for out-of-spec touch-controller */
#define FT81X_ENABLE
#endif


/* untested */
/* Matrix Orbital EVE2 modules EVE2-50A, EVE2-70A : 800x480 5.0" and 7.0" resistive, or no touch, FT812 */
#if defined (EVE_EVE2_50) || defined (EVE_EVE2_70)
#define EVE_HSIZE	(800L)
#define EVE_VSIZE	(480L)

#define EVE_VSYNC0	(0L)
#define EVE_VSYNC1	(3L)
#define EVE_VOFFSET	(32L)
#define EVE_VCYCLE	(525L)
#define EVE_HSYNC0	(0L)
#define EVE_HSYNC1	(48L)
#define EVE_HOFFSET	(88L)
#define EVE_HCYCLE 	(928L)
#define EVE_PCLKPOL	(1L)
#define EVE_SWIZZLE	(0L)
#define EVE_PCLK	(2L)
#define EVE_CSPREAD	(0L)
#define EVE_TOUCH_RZTHRESH (1200L)
#define FT81X_ENABLE
#endif


/* Matrix Orbital EVE2 modules EVE2-50G, EVE2-70G : 800x480 5.0" and 7.0" capacitive touch, FT813 */
#if defined (EVE_EVE2_50G) || defined (EVE_EVE2_70G)
#define EVE_HSIZE	(800L)
#define EVE_VSIZE	(480L)

#define EVE_VSYNC0	(0L)
#define EVE_VSYNC1	(3L)
#define EVE_VOFFSET	(32L)
#define EVE_VCYCLE	(525L)
#define EVE_HSYNC0	(0L)
#define EVE_HSYNC1	(48L)
#define EVE_HOFFSET	(88L)
#define EVE_HCYCLE 	(928L)
#define EVE_PCLKPOL	(1L)
#define EVE_SWIZZLE	(0L)
#define EVE_PCLK	(2L)
#define EVE_CSPREAD	(0L)
#define EVE_TOUCH_RZTHRESH (1200L)
#define EVE_HAS_GT911	/* special treatment required for out-of-spec touch-controller */
#define FT81X_ENABLE
#endif


/* NHD-3.5-320240FT-CxXx-xxx 320x240 3.5" Newhaven, resistive or capacitive, FT81x */
#if defined (EVE_NHD_35)
#define EVE_HSIZE	(320L)
#define EVE_VSIZE	(240L)

#define EVE_VSYNC0	(0L)
#define EVE_VSYNC1	(2L)
#define EVE_VOFFSET	(13L)
#define EVE_VCYCLE	(263L)
#define EVE_HSYNC0	(0L)
#define EVE_HSYNC1	(10L)
#define EVE_HOFFSET	(70L)
#define EVE_HCYCLE 	(408L)
#define EVE_PCLKPOL	(1L)
#define EVE_SWIZZLE	(2L)
#define EVE_PCLK	(6L)
#define EVE_CSPREAD	(0L)
#define EVE_TOUCH_RZTHRESH (1200L)
#define EVE_HAS_CRYSTAL
#define FT81X_ENABLE
#endif


/* untested */
/* NHD-4.3-480272FT-CxXx-xxx 480x272 4.3" Newhaven, resistive or capacitive, FT81x */
#if defined (EVE_NHD_43)
#define EVE_HSIZE	(480L)
#define EVE_VSIZE	(272L)

#define EVE_VSYNC0	(0L)
#define EVE_VSYNC1	(10L)
#define EVE_VOFFSET	(12L)
#define EVE_VCYCLE	(292L)
#define EVE_HSYNC0	(0L)
#define EVE_HSYNC1	(41L)
#define EVE_HOFFSET	(43L)
#define EVE_HCYCLE 	(548L)
#define EVE_PCLKPOL	(1L)
#define EVE_SWIZZLE	(0L)
#define EVE_PCLK	(5L)
#define EVE_CSPREAD	(1L)
#define EVE_TOUCH_RZTHRESH (1200L)
#define EVE_HAS_CRYSTAL
#define FT81X_ENABLE
#endif


/* untested */
/* NHD-5.0-800480FT-CxXx-xxx 800x480 5.0" Newhaven, resistive or capacitive, FT81x */
#if defined (EVE_NHD_50)
#define EVE_HSIZE	(800L)
#define EVE_VSIZE	(480L)

#define EVE_VSYNC0	(0L)
#define EVE_VSYNC1	(3L)
#define EVE_VOFFSET	(32L)
#define EVE_VCYCLE	(525L)
#define EVE_HSYNC0	(0L)
#define EVE_HSYNC1	(48L)
#define EVE_HOFFSET	(88L)
#define EVE_HCYCLE 	(928L)
#define EVE_PCLKPOL	(0L)
#define EVE_SWIZZLE	(0L)
#define EVE_PCLK	(2L)
#define EVE_CSPREAD	(1L)
#define EVE_TOUCH_RZTHRESH (1200L)
#define EVE_HAS_CRYSTAL
#define FT81X_ENABLE
#endif


/* untested */
/* NHD-7.0-800480FT-CxXx-xxx 800x480 7.0" Newhaven, resistive or capacitive, FT81x */
#if defined (EVE_NHD_70)
#define EVE_HSIZE	(800L)
#define EVE_VSIZE	(480L)

#define EVE_VSYNC0	(0L)
#define EVE_VSYNC1	(3L)
#define EVE_VOFFSET	(32L)
#define EVE_VCYCLE	(525L)
#define EVE_HSYNC0	(0L)
#define EVE_HSYNC1	(48L)
#define EVE_HOFFSET	(88L)
#define EVE_HCYCLE 	(928L)
#define EVE_PCLKPOL	(1L)
#define EVE_SWIZZLE	(0L)
#define EVE_PCLK	(2L)
#define EVE_CSPREAD	(1L)
#define EVE_TOUCH_RZTHRESH (1200L)
#define EVE_HAS_CRYSTAL
#define FT81X_ENABLE
#endif


/* ADAM101-LCP-SWVGA-NEW 1024x600 10.1" Glyn, capacitive, FT813 */
#if defined (EVE_ADAM101)
#define EVE_HSIZE	(1024L)
#define EVE_VSIZE	(600L)

#define EVE_VSYNC0	(0L)
#define EVE_VSYNC1	(1L)
#define EVE_VOFFSET	(1L)
#define EVE_VCYCLE	(720L)
#define EVE_HSYNC0	(0L)
#define EVE_HSYNC1	(1L)
#define EVE_HOFFSET	(1L)
#define EVE_HCYCLE 	(1100L)
#define EVE_PCLKPOL	(1L)
#define EVE_SWIZZLE	(0L)
#define EVE_PCLK	(2L)
#define EVE_CSPREAD	(1L)
#define EVE_TOUCH_RZTHRESH (1200L)
#define EVE_HAS_CRYSTAL
#define FT81X_ENABLE
#endif


/* Crystalfonts CFAF240400C1-030SC 240x400 3.0" , FT811 capacitive touch */
#if defined (EVE_CFAF240400C1_030SC)
#define EVE_HSIZE	(240L)
#define EVE_VSIZE	(400L)

#define EVE_VSYNC0	(4L)
#define EVE_VSYNC1	(6L)
#define EVE_VOFFSET	(8L)
#define EVE_VCYCLE	(409L)
#define EVE_HSYNC0	(10L)
#define EVE_HSYNC1	(20L)
#define EVE_HOFFSET	(40L)
#define EVE_HCYCLE 	(489L)
#define EVE_PCLKPOL	(0L)
#define EVE_SWIZZLE	(2L)
#define EVE_PCLK	(5L)
#define EVE_CSPREAD	(0L)
#define EVE_TOUCH_RZTHRESH (1200L)
#define FT81X_ENABLE
#endif


/* Crystalfonts CFAF320240F-035T 320x240 3.5" , FT810 resistive touch */
#if defined (EVE_CFAF320240F_035T)
#define EVE_HSIZE	(320L)
#define EVE_VSIZE	(240L)

#define EVE_VSYNC0	(1L)
#define EVE_VSYNC1	(4L)
#define EVE_VOFFSET	(4L)
#define EVE_VCYCLE	(245L)
#define EVE_HSYNC0	(10L)
#define EVE_HSYNC1	(20L)
#define EVE_HOFFSET	(40L)
#define EVE_HCYCLE 	(510L)
#define EVE_PCLKPOL	(0L)
#define EVE_SWIZZLE	(2L)
#define EVE_PCLK	(8L)
#define EVE_CSPREAD	(0L)
#define EVE_TOUCH_RZTHRESH (1200L)
#define FT81X_ENABLE
#endif


/* Crystalfonts CFAF480128A0-039TC 480x128 3.9" , FT811 capacitive touch */
#if defined (EVE_CFAF480128A0_039TC)
#define EVE_HSIZE	(480L)
#define EVE_VSIZE	(128L)

#define EVE_VSYNC0	(4L)
#define EVE_VSYNC1	(5L)
#define EVE_VOFFSET	(8L)
#define EVE_VCYCLE	(137L)
#define EVE_HSYNC0	(24L)
#define EVE_HSYNC1	(35L)
#define EVE_HOFFSET	(41L)
#define EVE_HCYCLE 	(1042L)
#define EVE_PCLKPOL	(1L)
#define EVE_SWIZZLE	(0L)
#define EVE_PCLK	(7L)
#define EVE_CSPREAD	(0L)
#define EVE_TOUCH_RZTHRESH (1200L)
#define FT81X_ENABLE
#endif


/* Crystalfonts CFAF800480E0-050SC 800x480 5.0" , FT813 capacitive touch */
#if defined (EVE_CFAF800480E0_050SC)
#define EVE_HSIZE	(800L)
#define EVE_VSIZE	(480L)

#define EVE_VSYNC0	(7L)
#define EVE_VSYNC1	(8L)
#define EVE_VOFFSET	(30L)
#define EVE_VCYCLE	(511L)
#define EVE_HSYNC0	(16L)
#define EVE_HSYNC1	(17L)
#define EVE_HOFFSET	(62L)
#define EVE_HCYCLE 	(978L)
#define EVE_PCLKPOL	(1L)
#define EVE_SWIZZLE	(0L)
#define EVE_PCLK	(2L)
#define EVE_CSPREAD	(0L)
#define EVE_TOUCH_RZTHRESH (1200L)
#define EVE_HAS_GT911
#define FT81X_ENABLE
#endif


/* PAF90B5WFNWC01 800x480 9.0" Panasys, BT815 */
#if defined (EVE_PAF90)
#define EVE_HSIZE	(800L)	/* Thd Length of visible part of line (in PCLKs) - display width */
#define EVE_VSIZE	(480L)	/* Tvd Number of visible lines (in lines) - display height */

#define EVE_VSYNC0	(0L)	/* Tvf Vertical Front Porch */
#define EVE_VSYNC1	(10L)	/* Tvf + Tvp Vertical Front Porch plus Vsync Pulse width */
#define EVE_VOFFSET	(23L)	/* Tvf + Tvp + Tvb Number of non-visible lines (in lines) */
#define EVE_VCYCLE	(525L)	/* Tv Total number of lines (visible and non-visible) (in lines) */
#define EVE_HSYNC0	(0L)	/* Thf Horizontal Front Porch */
#define EVE_HSYNC1	(10L)	/* Thf + Thp Horizontal Front Porch plus Hsync Pulse width */
#define EVE_HOFFSET (46L)	/* Thf + Thp + Thb Length of non-visible part of line (in PCLK cycles) */
#define EVE_HCYCLE 	(1056L)	/* Th Total length of line (visible and non-visible) (in PCLKs) */
#define EVE_PCLKPOL (1L)	/* PCLK polarity (0 = rising edge, 1 = falling edge) */
#define EVE_SWIZZLE (0L)	/* Defines the arrangement of the RGB pins of the FT800 */
#define EVE_PCLK	(2L)	/* 60MHz / REG_PCLK = PCLK frequency 30 MHz */
#define EVE_CSPREAD	(1L)	/* helps with noise, when set to 1 fewer signals are changed simultaneously, reset-default: 1 */
#define EVE_TOUCH_RZTHRESH (1200L)	/* touch-sensitivity */
#define EVE_HAS_CRYSTAL
#define FT81X_ENABLE
#define BT81X_ENABLE
#endif


/* untested */
/* Sunflower Arduino Shield, 320x240 3.5" from Cowfish, FT813, https://github.com/Cowfish-Studios/Cowfish_Sunflower_Shield_PCB */
/* set EVE_CS to 6 and EVE_PDN to 5 in the Arduino block in EVE_target.h */
#if defined (EVE_SUNFLOWER)
#define EVE_HSIZE	(320L)
#define EVE_VSIZE	(240L)

#define EVE_VSYNC0	(0L)
#define EVE_VSYNC1	(2L)
#define EVE_VOFFSET	(13L)
#define EVE_VCYCLE	(263L)
#define EVE_HSYNC0	(0L)
#define EVE_HSYNC1	(10L)
#define EVE_HOFFSET	(70L)
#define EVE_HCYCLE 	(408L)
#define EVE_PCLKPOL	(1L)
#define EVE_SWIZZLE	(2L)
#define EVE_PCLK	(6L)
#define EVE_CSPREAD	(0L)
#define EVE_TOUCH_RZTHRESH (1200L)
#define EVE_HAS_CRYSTAL
#define FT81X_ENABLE
#endif

/* untested */
/* MikroElektronika ConnectEVE, FT800 480x272 4.3" */
#if defined (EVE_CONNECTEVE)
#define EVE_HSIZE	(480L)
#define EVE_VSIZE	(272L)

#define EVE_VSYNC0	(0L)
#define EVE_VSYNC1	(10L)
#define EVE_VOFFSET	(12L)
#define EVE_VCYCLE	(286L)
#define EVE_HSYNC0	(0L)
#define EVE_HSYNC1	(41L)
#define EVE_HOFFSET	(43L)
#define EVE_HCYCLE 	(525L)
#define EVE_PCLKPOL	(1L)
#define EVE_SWIZZLE	(0L)
#define EVE_PCLK	(5L)
#define EVE_CSPREAD	(0L)
#define EVE_TOUCH_RZTHRESH (2000L)
#define EVE_HAS_CRYSTAL
#endif

#endif /* EVE_CONFIG_H */
