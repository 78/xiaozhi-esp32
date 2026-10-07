#ifndef __EPD_DISPLAY_H__
#define __EPD_DISPLAY_H__

#include <stddef.h>
#include <driver/gpio.h>
#include "lcd_display.h"

/* Display color */
enum COLOR_IMAGE {
    DRIVER_COLOR_WHITE  = 0xff,
    DRIVER_COLOR_BLACK  = 0x00,
    FONT_BACKGROUND = DRIVER_COLOR_WHITE,
};

struct custom_lcd_spi_t {
    uint8_t cs;
    uint8_t dc;
    uint8_t rst;
    uint8_t busy;
    uint8_t mosi;
    uint8_t scl;
    int spi_host;
    int buffer_len;
};

/* E-Paper Display by EPDiy */
class EpdDisplay : public LcdDisplay {
public:
    EpdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                  int width, int height, custom_lcd_spi_t _lcd_spi_data);
    ~EpdDisplay() override;

    // Call once after the most-derived object is fully constructed, before SetupUI().
    void Initialize();

    virtual void EPD_Init();    /* Initialize the E-Paper display */
    virtual void EPD_Clear();   /* Clear the screen */
    virtual void EPD_Display(); /* Refresh the buffer to the E-Paper display */
    
    /* Fast refresh */
    virtual void EPD_DisplayPartBaseImage();
    virtual void EPD_Init_Partial();
    virtual void EPD_DisplayPart();
    void EPD_DrawColorPixel(uint16_t x, uint16_t y,uint8_t color);
    
protected:
    struct Waveform {
        const uint8_t* data;
        size_t size;
    };

    // Return static or member-owned data, never a local array.
    virtual Waveform GetFullWaveform() const = 0;
    virtual Waveform GetPartialWaveform() const = 0;

    const custom_lcd_spi_t lcd_spi_data;
    const int Width;
    const int Height;
    spi_device_handle_t spi;
    uint8_t *buffer = NULL;
    
    void read_busy();

    void set_cs_1() { gpio_set_level((gpio_num_t)lcd_spi_data.cs,1); }
    void set_cs_0() { gpio_set_level((gpio_num_t)lcd_spi_data.cs,0); }
    void set_dc_1() { gpio_set_level((gpio_num_t)lcd_spi_data.dc,1); }
    void set_dc_0() { gpio_set_level((gpio_num_t)lcd_spi_data.dc,0); }
    void set_rst_1() { gpio_set_level((gpio_num_t)lcd_spi_data.rst,1); }
    void set_rst_0() { gpio_set_level((gpio_num_t)lcd_spi_data.rst,0); }

    void SPI_SendByte(uint8_t data);
    void EPD_SendData(uint8_t data);
    void EPD_SendCommand(uint8_t command);
    void writeBytes(const uint8_t *buffer, size_t len);
    void EPD_SetWindows(uint16_t Xstart, uint16_t Ystart, uint16_t Xend, uint16_t Yend);
    void EPD_SetCursor(uint16_t Xstart, uint16_t Ystart);
    void EPD_SetLut(Waveform lut);
    void EPD_TurnOnDisplay();
    void EPD_TurnOnDisplayPart();

private:
    static void lvgl_flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * color_p);
    void spi_gpio_init();
    void spi_port_init();
};

#endif // __EPD_DISPLAY_H__