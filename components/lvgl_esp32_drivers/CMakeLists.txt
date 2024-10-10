if(ESP_PLATFORM)

file(GLOB SOURCES *.c)
set(LVGL_INCLUDE_DIRS . lvgl_tft)
list(APPEND SOURCES "lvgl_tft/disp_driver.c")

#@todo add SimleInclude macro here

# Include only the source file of the selected
# display controller.
if(CONFIG_LV_TFT_DISPLAY_CONTROLLER_ILI9341)
    list(APPEND SOURCES "lvgl_tft/ili9341.c")
elseif(CONFIG_LV_TFT_DISPLAY_CONTROLLER_ILI9481)
    list(APPEND SOURCES "lvgl_tft/ili9481.c")
elseif(CONFIG_LV_TFT_DISPLAY_CONTROLLER_ILI9486)
    list(APPEND SOURCES "lvgl_tft/ili9486.c")
elseif(CONFIG_LV_TFT_DISPLAY_CONTROLLER_ILI9488)
    list(APPEND SOURCES "lvgl_tft/ili9488.c")
elseif(CONFIG_LV_TFT_DISPLAY_CONTROLLER_ST7789)
    list(APPEND SOURCES "lvgl_tft/st7789.c")
elseif(CONFIG_LV_TFT_DISPLAY_CONTROLLER_ST7735S)
    list(APPEND SOURCES "lvgl_tft/st7735s.c")
elseif(CONFIG_LV_TFT_DISPLAY_CONTROLLER_ST7796S)
    list(APPEND SOURCES "lvgl_tft/st7796s.c")
elseif(CONFIG_LV_TFT_DISPLAY_CONTROLLER_HX8357)
    list(APPEND SOURCES "lvgl_tft/hx8357.c")
elseif(CONFIG_LV_TFT_DISPLAY_CONTROLLER_SH1107)
    list(APPEND SOURCES "lvgl_tft/sh1107.c")
elseif(CONFIG_LV_TFT_DISPLAY_CONTROLLER_SSD1306)
    list(APPEND SOURCES "lvgl_tft/ssd1306.c")
elseif(CONFIG_LV_TFT_DISPLAY_CONTROLLER_FT81X)
    list(APPEND SOURCES "lvgl_tft/EVE_commands.c")
    list(APPEND SOURCES "lvgl_tft/FT81x.c")
elseif(CONFIG_LV_TFT_DISPLAY_CONTROLLER_IL3820)
    list(APPEND SOURCES "lvgl_tft/il3820.c")
elseif(CONFIG_LV_TFT_DISPLAY_CONTROLLER_JD79653A)
    list(APPEND SOURCES "lvgl_tft/jd79653a.c")
elseif(CONFIG_LV_TFT_DISPLAY_CONTROLLER_UC8151D)
    list(APPEND SOURCES "lvgl_tft/uc8151d.c")
elseif(CONFIG_LV_TFT_DISPLAY_CONTROLLER_RA8875)
    list(APPEND SOURCES "lvgl_tft/ra8875.c")
elseif(CONFIG_LV_TFT_DISPLAY_CONTROLLER_GC9A01)
    list(APPEND SOURCES "lvgl_tft/GC9A01.c")
elseif(CONFIG_LV_TFT_DISPLAY_CONTROLLER_ILI9163C)
    list(APPEND SOURCES "lvgl_tft/ili9163c.c")
else()
    message(WARNING "LVGL ESP32 drivers: Display controller not defined.")
endif()

if(CONFIG_LV_TFT_DISPLAY_PROTOCOL_SPI)
    list(APPEND SOURCES "lvgl_tft/disp_spi.c")
endif()

# Add touch driver to compilation only if it is selected in menuconfig
if(CONFIG_LV_TOUCH_CONTROLLER)
    list(APPEND SOURCES "lvgl_touch/touch_driver.c")
    list(APPEND LVGL_INCLUDE_DIRS lvgl_touch)

    # Include only the source file of the selected
    # touch controller.
    if(CONFIG_LV_TOUCH_CONTROLLER_XPT2046)
        list(APPEND SOURCES "lvgl_touch/xpt2046.c")
    elseif(CONFIG_LV_TOUCH_CONTROLLER_FT6X06)
        list(APPEND SOURCES "lvgl_touch/ft6x36.c")
    elseif(CONFIG_LV_TOUCH_CONTROLLER_CST816)
        list(APPEND SOURCES "lvgl_touch/cst816.c")
    elseif(CONFIG_LV_TOUCH_CONTROLLER_STMPE610)
        list(APPEND SOURCES "lvgl_touch/stmpe610.c")
    elseif(CONFIG_LV_TOUCH_CONTROLLER_ADCRAW)
        list(APPEND SOURCES "lvgl_touch/adcraw.c")
    elseif(CONFIG_LV_TOUCH_CONTROLLER_FT81X)
        list(APPEND SOURCES "lvgl_touch/FT81x.c")
    elseif(CONFIG_LV_TOUCH_CONTROLLER_RA8875)
        list(APPEND SOURCES "lvgl_touch/ra8875_touch.c")
    elseif(CONFIG_LV_TOUCH_CONTROLLER_GT911)
        list(APPEND SOURCES "lvgl_touch/gt911.c")
    endif()

    if(CONFIG_LV_TOUCH_DRIVER_PROTOCOL_SPI)
        list(APPEND SOURCES "lvgl_touch/tp_spi.c")
    endif()
endif()


if(CONFIG_LV_I2C)
    list(APPEND SOURCES "lvgl_i2c/i2c_manager.c")
endif()

idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS ${LVGL_INCLUDE_DIRS}
                       REQUIRES lvgl vfs sdmmc fatfs driver)

target_compile_definitions(${COMPONENT_LIB} PUBLIC "-DLV_LVGL_H_INCLUDE_SIMPLE")

else()
    message(FATAL_ERROR "LVGL ESP32 drivers: ESP_PLATFORM is not defined. Try reinstalling ESP-IDF.")
endif()
