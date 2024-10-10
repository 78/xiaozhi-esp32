/**
 * @file lvgl_spi_conf.h
 *
 */

#ifndef LVGL_SPI_CONF_H
#define LVGL_SPI_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

/*********************
 *      DEFINES
 *********************/
// DISPLAY PINS
#define DISP_SPI_MOSI CONFIG_LV_DISP_SPI_MOSI
#if defined (CONFIG_LV_DISPLAY_USE_SPI_MISO)
    #define DISP_SPI_MISO CONFIG_LV_DISP_SPI_MISO
    #define DISP_SPI_INPUT_DELAY_NS CONFIG_LV_DISP_SPI_INPUT_DELAY_NS
#else
    #define DISP_SPI_MISO (-1)
    #define DISP_SPI_INPUT_DELAY_NS (0)
#endif
#if defined(CONFIG_LV_DISP_SPI_IO2)
#define DISP_SPI_IO2 CONFIG_LV_DISP_SPI_IO2
#else
#define DISP_SPI_IO2 (-1)
#endif
#if defined(CONFIG_LV_DISP_SPI_IO3)
#define DISP_SPI_IO3 CONFIG_LV_DISP_SPI_IO3
#else
#define DISP_SPI_IO3 (-1)
#endif
#define DISP_SPI_CLK CONFIG_LV_DISP_SPI_CLK
#if defined (CONFIG_LV_DISPLAY_USE_SPI_CS)
    #define DISP_SPI_CS CONFIG_LV_DISP_SPI_CS
#else
    #define DISP_SPI_CS (46)
#endif

/* Define TOUCHPAD PINS when selecting a touch controller */
#if !defined (CONFIG_LV_TOUCH_CONTROLLER_NONE)

/* Handle FT81X special case */
#if defined (CONFIG_LV_TFT_DISPLAY_CONTROLLER_FT81X) && \
    defined (CONFIG_LV_TOUCH_CONTROLLER_FT81X)

    #define TP_SPI_MOSI CONFIG_LV_DISP_SPI_MOSI
    #define TP_SPI_MISO CONFIG_LV_DISP_SPI_MISO
    #define TP_SPI_CLK  CONFIG_LV_DISP_SPI_CLK
    #define TP_SPI_CS   CONFIG_LV_DISP_SPI_CS
#else
    #define TP_SPI_MOSI CONFIG_LV_TOUCH_SPI_MOSI
    #define TP_SPI_MISO CONFIG_LV_TOUCH_SPI_MISO
    #define TP_SPI_CLK  CONFIG_LV_TOUCH_SPI_CLK
    #define TP_SPI_CS   CONFIG_LV_DISP_SPI_CS
#endif
#endif

#define ENABLE_TOUCH_INPUT  CONFIG_LV_ENABLE_TOUCH

#if defined (CONFIG_LV_TFT_DISPLAY_SPI_HSPI)
#if defined (CONFIG_IDF_TARGET_ESP32C3)
#define TFT_SPI_HOST    SPI2_HOST
#else
#define TFT_SPI_HOST    SPI2_HOST
#endif
#elif defined (CONFIG_LV_TFT_DISPLAY_SPI_VSPI)
#define TFT_SPI_HOST    SPI2_HOST
#elif defined (CONFIG_LV_TFT_DISPLAY_SPI_FSPI)
#define TFT_SPI_HOST    SPI2_HOST
#endif

#if defined (CONFIG_LV_TFT_DISPLAY_SPI_HALF_DUPLEX)
#define DISP_SPI_HALF_DUPLEX
#else
#define DISP_SPI_FULL_DUPLEX
#endif

#if defined (CONFIG_LV_TFT_DISPLAY_SPI_TRANS_MODE_DIO)
#define DISP_SPI_TRANS_MODE_DIO
#elif defined (CONFIG_LV_TFT_DISPLAY_SPI_TRANS_MODE_QIO)
#define DISP_SPI_TRANS_MODE_QIO
#else
#define DISP_SPI_TRANS_MODE_SIO
#endif

#if defined (CONFIG_LV_TOUCH_CONTROLLER_SPI_HSPI)
#define TOUCH_SPI_HOST SPI2_HOST
#elif defined (CONFIG_LV_TOUCH_CONTROLLER_SPI_VSPI)
#define TOUCH_SPI_HOST SPI2_HOST
#elif defined (CONFIG_LV_TOUCH_CONTROLLER_SPI_FSPI)
#define TOUCH_SPI_HOST SPI2_HOST
#endif

/* Handle the FT81X Special case */
#if defined (CONFIG_LV_TFT_DISPLAY_CONTROLLER_FT81X)

#if defined (CONFIG_LV_TOUCH_CONTROLLER_FT81X)
#define SHARED_SPI_BUS
#else
/* Empty */
#endif

#else
// Detect the use of a shared SPI Bus and verify the user specified the same SPI bus for both touch and tft
#if defined (CONFIG_LV_TOUCH_DRIVER_PROTOCOL_SPI) && TP_SPI_MOSI == DISP_SPI_MOSI && TP_SPI_CLK == DISP_SPI_CLK
#if TFT_SPI_HOST != TOUCH_SPI_HOST
#error You must specify the same SPI host (HSPI, VSPI or FSPI) for both display and touch driver
#endif

#define SHARED_SPI_BUS
#endif

#endif

/**********************
 *      TYPEDEFS
 **********************/
#if defined (CONFIG_LV_TFT_DISPLAY_CONTROLLER_ILI9481) || \
    defined (CONFIG_LV_TFT_DISPLAY_CONTROLLER_ILI9488)

#define SPI_BUS_MAX_TRANSFER_SZ (DISP_BUF_SIZE * 3)

#elif defined (CONFIG_LV_TFT_DISPLAY_CONTROLLER_ILI9341)  || \
      defined (CONFIG_LV_TFT_DISPLAY_CONTROLLER_ST7789)   || \
      defined (CONFIG_LV_TFT_DISPLAY_CONTROLLER_ST7735S)  || \
      defined (CONFIG_LV_TFT_DISPLAY_CONTROLLER_HX8357)   || \
      defined (CONFIG_LV_TFT_DISPLAY_CONTROLLER_SH1107)   || \
      defined (CONFIG_LV_TFT_DISPLAY_CONTROLLER_FT81X)    || \
      defined (CONFIG_LV_TFT_DISPLAY_CONTROLLER_IL3820)   || \
      defined (CONFIG_LV_TFT_DISPLAY_CONTROLLER_JD79653A) || \
      defined (CONFIG_LV_TFT_DISPLAY_CONTROLLER_ILI9163C)

#define SPI_BUS_MAX_TRANSFER_SZ (DISP_BUF_SIZE * 2)

#else
#define SPI_BUS_MAX_TRANSFER_SZ (DISP_BUF_SIZE * 2)
#endif

#if defined (CONFIG_LV_TFT_USE_CUSTOM_SPI_CLK_DIVIDER)
#define SPI_TFT_CLOCK_SPEED_HZ ((60 * 1000 * 1000) / CONFIG_LV_TFT_CUSTOM_SPI_CLK_DIVIDER)
#else
#if defined (CONFIG_LV_TFT_DISPLAY_CONTROLLER_ST7789)
#define SPI_TFT_CLOCK_SPEED_HZ  (60*1000*1000)
#elif defined (CONFIG_LV_TFT_DISPLAY_CONTROLLER_ST7735S)
#define SPI_TFT_CLOCK_SPEED_HZ  (40*1000*1000)
#elif defined (CONFIG_LV_TFT_DISPLAY_CONTROLLER_HX8357)
#define SPI_TFT_CLOCK_SPEED_HZ  (26*1000*1000)
#elif defined (CONFIG_LV_TFT_DISPLAY_CONTROLLER_SH1107)
#define SPI_TFT_CLOCK_SPEED_HZ  (8*1000*1000)
#elif defined (CONFIG_LV_TFT_DISPLAY_CONTROLLER_ILI9481)
#define SPI_TFT_CLOCK_SPEED_HZ  (16*1000*1000)
#elif defined (CONFIG_LV_TFT_DISPLAY_CONTROLLER_ILI9486)
#define SPI_TFT_CLOCK_SPEED_HZ  (20*1000*1000)
#elif defined (CONFIG_LV_TFT_DISPLAY_CONTROLLER_ILI9488)
#define SPI_TFT_CLOCK_SPEED_HZ  (40*1000*1000)
#elif defined (CONFIG_LV_TFT_DISPLAY_CONTROLLER_ILI9341)
#define SPI_TFT_CLOCK_SPEED_HZ  (40*1000*1000)
#elif defined(CONFIG_LV_TFT_DISPLAY_CONTROLLER_ILI9163C)
#define SPI_TFT_CLOCK_SPEED_HZ (40 * 1000 * 1000)
#elif defined(CONFIG_LV_TFT_DISPLAY_CONTROLLER_FT81X)
#define SPI_TFT_CLOCK_SPEED_HZ  (32*1000*1000)
#else
#define SPI_TFT_CLOCK_SPEED_HZ  (60*1000*1000)
#endif

#endif


#if defined (CONFIG_LV_TFT_DISPLAY_CONTROLLER_ST7789)
#define SPI_TFT_SPI_MODE    (2)
#else
#define SPI_TFT_SPI_MODE    (2)
#endif

/* Touch driver */
#if (CONFIG_LV_TOUCH_CONTROLLER == TOUCH_CONTROLLER_STMPE610)
#define SPI_TOUCH_CLOCK_SPEED_HZ    (1*1000*1000)
#define SPI_TOUCH_SPI_MODE          (1)
#else
#define SPI_TOUCH_CLOCK_SPEED_HZ    (2*1000*1000)
#define SPI_TOUCH_SPI_MODE          (0)
#endif

/**********************
 * GLOBAL PROTOTYPES
 **********************/


/**********************
 *      MACROS
 **********************/


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*LVGL_SPI_CONF_H*/
