# LVGL ESP32 drivers

# Define sources and include dirs
COMPONENT_SRCDIRS := . lvgl_tft lvgl_touch lvgl_i2c
COMPONENT_ADD_INCLUDEDIRS := .

# LVGL is supposed to be used as a ESP-IDF component
# -> lvlg is already in the include path
# -> we use simple include
CFLAGS += -DLV_LVGL_H_INCLUDE_SIMPLE

# TFT display drivers
COMPONENT_ADD_INCLUDEDIRS += lvgl_tft

$(call compile_only_if,$(CONFIG_LV_TFT_DISPLAY_CONTROLLER_ILI9341),lvgl_tft/ili9341.o)
$(call compile_only_if,$(CONFIG_LV_TFT_DISPLAY_CONTROLLER_ILI9481),lvgl_tft/ili9481.o)
$(call compile_only_if,$(CONFIG_LV_TFT_DISPLAY_CONTROLLER_ILI9486),lvgl_tft/ili9486.o)
$(call compile_only_if,$(CONFIG_LV_TFT_DISPLAY_CONTROLLER_ILI9488),lvgl_tft/ili9488.o)
$(call compile_only_if,$(CONFIG_LV_TFT_DISPLAY_CONTROLLER_ST7789),lvgl_tft/st7789.o)
$(call compile_only_if,$(CONFIG_LV_TFT_DISPLAY_CONTROLLER_ST7735S),lvgl_tft/st7735s.o)
$(call compile_only_if,$(CONFIG_LV_TFT_DISPLAY_CONTROLLER_ST7796S),lvgl_tft/st7796s.o)
$(call compile_only_if,$(CONFIG_LV_TFT_DISPLAY_CONTROLLER_HX8357),lvgl_tft/hx8357.o)
$(call compile_only_if,$(CONFIG_LV_TFT_DISPLAY_CONTROLLER_SH1107),lvgl_tft/sh1107.o)
$(call compile_only_if,$(CONFIG_LV_TFT_DISPLAY_CONTROLLER_SSD1306),lvgl_tft/ssd1306.o)
$(call compile_only_if,$(CONFIG_LV_TFT_DISPLAY_CONTROLLER_FT81X),lvgl_tft/EVE_commands.o)
$(call compile_only_if,$(CONFIG_LV_TFT_DISPLAY_CONTROLLER_FT81X),lvgl_tft/FT81x.o)
$(call compile_only_if,$(CONFIG_LV_TFT_DISPLAY_CONTROLLER_IL3820),lvgl_tft/il3820.o)
$(call compile_only_if,$(CONFIG_LV_TFT_DISPLAY_CONTROLLER_JD79653A),lvgl_tft/jd79653a.o)
$(call compile_only_if,$(CONFIG_LV_TFT_DISPLAY_CONTROLLER_UC8151D),lvgl_tft/uc8151d.o)
$(call compile_only_if,$(CONFIG_LV_TFT_DISPLAY_CONTROLLER_RA8875),lvgl_tft/ra8875.o)
$(call compile_only_if,$(CONFIG_LV_TFT_DISPLAY_CONTROLLER_GC9A01),lvgl_tft/GC9A01.o)

$(call compile_only_if,$(CONFIG_LV_TFT_DISPLAY_PROTOCOL_SPI),lvgl_tft/disp_spi.o)

# Touch controller drivers
COMPONENT_ADD_INCLUDEDIRS += lvgl_touch

$(call compile_only_if,$(CONFIG_LV_TOUCH_CONTROLLER),lvgl_touch/touch_driver.o)
$(call compile_only_if,$(and $(CONFIG_LV_TOUCH_CONTROLLER),$(CONFIG_LV_TOUCH_CONTROLLER_XPT2046)), lvgl_touch/xpt2046.o)
$(call compile_only_if,$(and $(CONFIG_LV_TOUCH_CONTROLLER),$(CONFIG_LV_TOUCH_CONTROLLER_FT6X06)), lvgl_touch/ft6x36.o)
$(call compile_only_if,$(and $(CONFIG_LV_TOUCH_CONTROLLER),$(CONFIG_LV_TOUCH_CONTROLLER_STMPE610)), lvgl_touch/stmpe610.o)
$(call compile_only_if,$(and $(CONFIG_LV_TOUCH_CONTROLLER),$(CONFIG_LV_TOUCH_CONTROLLER_ADCRAW)), lvgl_touch/adcraw.o)
$(call compile_only_if,$(and $(CONFIG_LV_TOUCH_CONTROLLER),$(CONFIG_LV_TOUCH_CONTROLLER_FT81X)), lvgl_touch/FT81x.o)
$(call compile_only_if,$(and $(CONFIG_LV_TOUCH_CONTROLLER),$(CONFIG_LV_TOUCH_CONTROLLER_RA8875)), lvgl_touch/ra8875_touch.o)

$(call compile_only_if,$(and $(CONFIG_LV_TOUCH_CONTROLLER),$(CONFIG_LV_TOUCH_DRIVER_PROTOCOL_SPI)), lvgl_touch/tp_spi.o)

# I2C Manager
$(call compile_only_if,$(CONFIG_LV_I2C), lvgl_i2c/i2c_manager.o)
