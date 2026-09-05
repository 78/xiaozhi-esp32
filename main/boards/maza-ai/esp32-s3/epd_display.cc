#include <stdio.h>
#include <esp_lcd_panel_io.h>
#include <freertos/FreeRTOS.h>
#include <vector>
#include <esp_log.h>
#include <esp_timer.h>
#include "epd_display.h"
#include "board.h"
#include "config.h"
#include "esp_lvgl_port.h"
#include "settings.h"

#define TAG "EpdDisplay"

#define BYTES_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565))
#define BUFF_SIZE (DISPLAY_WIDTH * DISPLAY_HEIGHT * BYTES_PER_PIXEL)

const uint8_t WF_Full[30] =
{		
    //C221 25C Full update waveform									
    0x50,0xAA,0x55,0xAA,0x11,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0xFF,0xFF,0x1F,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

const uint8_t WF_PARTIAL[30] =
{
    //C221 25C partial update waveform
    0x10,0x18,0x18,0x08,0x18,0x18,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x13,0x14,0x44,0x12,0x00,0x00,0x00,0x00,0x00,0x00
};

void EpdDisplay::lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *color_p) {
    assert(disp != NULL);
    EpdDisplay *driver = (EpdDisplay *) lv_display_get_user_data(disp);
    const uint16_t *pixels = (const uint16_t *) color_p;
    if (area->x1 == 0 && area->y1 == 0 && area->x2 == driver->Height - 1 && area->y2 == driver->Width - 1) {
        const int panelStride = driver->Width >> 3;
        for (int x = 0; x < driver->Height; x++) {
            uint8_t *dest = driver->buffer + (driver->Height - 1 - x) * panelStride;
            for (int y = 0; y < driver->Width; y += 8) {
                const uint16_t *pixel = pixels + y * driver->Height + x;
                dest[y >> 3] = ((pixel[0 * driver->Height] >= 0x7fff) << 7) |
                               ((pixel[1 * driver->Height] >= 0x7fff) << 6) |
                               ((pixel[2 * driver->Height] >= 0x7fff) << 5) |
                               ((pixel[3 * driver->Height] >= 0x7fff) << 4) |
                               ((pixel[4 * driver->Height] >= 0x7fff) << 3) |
                               ((pixel[5 * driver->Height] >= 0x7fff) << 2) |
                               ((pixel[6 * driver->Height] >= 0x7fff) << 1) |
                               (pixel[7 * driver->Height] >= 0x7fff);
            }
        }
    } else {
        for (int y = area->y1; y <= area->y2; y++) {
            for (int x = area->x1; x <= area->x2; x++) {
                uint8_t color = (*pixels < 0x7fff) ? DRIVER_COLOR_BLACK : DRIVER_COLOR_WHITE;
                driver->EPD_DrawColorPixel(x, y, color);
                pixels++;
            }
        }
    }
    driver->EPD_DisplayPart();
    lv_disp_flush_ready(disp);
}

EpdDisplay::EpdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, 
    int width, int height, custom_lcd_spi_t _lcd_spi_data) : 
    LcdDisplay(panel_io, panel, height, width), 
    lcd_spi_data(_lcd_spi_data), 
    Width(width), Height(height) {

    ESP_LOGI(TAG, "Initialize SPI");
    spi_port_init();
    spi_gpio_init();

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority   = 2;
    port_cfg.timer_period_ms = 50;
    lvgl_port_init(&port_cfg);
    lvgl_port_lock(0);

    buffer = (uint8_t *) heap_caps_malloc(lcd_spi_data.buffer_len, MALLOC_CAP_SPIRAM);
    assert(buffer);
    display_ = lv_display_create(height, width); /* Landscape LVGL surface over portrait panel RAM */
    lv_display_set_flush_cb(display_, lvgl_flush_cb);
    lv_display_set_user_data(display_, this);

    uint8_t *buffer_1 = NULL;
    buffer_1 = (uint8_t *) heap_caps_malloc(BUFF_SIZE, MALLOC_CAP_SPIRAM);
    assert(buffer_1);
    lv_display_set_buffers(display_, buffer_1, NULL, BUFF_SIZE, LV_DISPLAY_RENDER_MODE_FULL);

    ESP_LOGI(TAG, "EPD init");
    EPD_Init();
    EPD_Clear();
    EPD_Display();
    EPD_DisplayPartBaseImage();
    EPD_Init_Partial(); // Initialize partial refresh

    lvgl_port_unlock();
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add display");
        return;
    }

    // Note: SetupUI() should be called by Application::Initialize(), not in constructor
    // to ensure lvgl objects are created after the display is fully initialized.
}

EpdDisplay::~EpdDisplay() {
    
}

void EpdDisplay::spi_gpio_init() {
    int rst  = lcd_spi_data.rst;
    int cs   = lcd_spi_data.cs;
    int dc   = lcd_spi_data.dc;
    int busy = lcd_spi_data.busy;

    gpio_config_t gpio_conf = {};
    gpio_conf.intr_type     = GPIO_INTR_DISABLE;
    gpio_conf.mode          = GPIO_MODE_OUTPUT;
    gpio_conf.pin_bit_mask  = (0x1ULL << rst) | (0x1ULL << dc) | (0x1ULL << cs);
    gpio_conf.pull_down_en  = GPIO_PULLDOWN_DISABLE;
    gpio_conf.pull_up_en    = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&gpio_conf));

    gpio_conf.mode         = GPIO_MODE_INPUT;
    gpio_conf.pin_bit_mask = (0x1ULL << busy);
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&gpio_conf));

    set_rst_1();
}

void EpdDisplay::spi_port_init() {
    int              mosi     = lcd_spi_data.mosi;
    int              scl      = lcd_spi_data.scl;
    int              spi_host = lcd_spi_data.spi_host;
    esp_err_t        ret;
    spi_bus_config_t buscfg = {};
    buscfg.miso_io_num      = -1;
    buscfg.mosi_io_num      = mosi;
    buscfg.sclk_io_num      = scl;
    buscfg.quadwp_io_num    = -1;
    buscfg.quadhd_io_num    = -1;
    buscfg.max_transfer_sz  = Width * Height;

    spi_device_interface_config_t devcfg = {};
    devcfg.spics_io_num                  = -1;
    devcfg.clock_speed_hz                = 40 * 1000 * 1000; // Clock out at 10 MHz
    devcfg.mode                          = 0;                // SPI mode 0
    devcfg.queue_size                    = 7;                // We want to be able to queue 7 transactions at a time

    ret = spi_bus_initialize((spi_host_device_t) spi_host, &buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);
    ret = spi_bus_add_device((spi_host_device_t) spi_host, &devcfg, &spi);
    ESP_ERROR_CHECK(ret);
}

void EpdDisplay::read_busy() {
    int busy = lcd_spi_data.busy;
    while (gpio_get_level((gpio_num_t) busy) == 1) {
        vTaskDelay(pdMS_TO_TICKS(5)); // LOW: idle, HIGH: busy
    }
}

void EpdDisplay::SPI_SendByte(uint8_t data) {
    esp_err_t         ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length    = 8;
    t.tx_buffer = &data;
    ret         = spi_device_polling_transmit(spi, &t); // Transmit!
    assert(ret == ESP_OK);                              // Should have had no issues.
}

void EpdDisplay::EPD_SendData(uint8_t data) {
    set_cs_0();
    set_dc_1();
    SPI_SendByte(data);
    set_cs_1();
}

void EpdDisplay::EPD_SendCommand(uint8_t command) {
    set_cs_0();
    set_dc_0();
    SPI_SendByte(command);
    set_cs_1();
}

void EpdDisplay::writeBytes(const uint8_t *buffer, int len) {
    set_cs_0();
    set_dc_1();
    esp_err_t         ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length    = 8 * len;
    t.tx_buffer = buffer;
    ret         = spi_device_polling_transmit(spi, &t); // Transmit!
    assert(ret == ESP_OK);
    set_cs_1();
}

void EpdDisplay::EPD_SetWindows(uint16_t Xstart, uint16_t Ystart, uint16_t Xend, uint16_t Yend) {
    EPD_SendCommand(0x44); // SET_RAM_X_ADDRESS_START_END_POSITION
    EPD_SendData((Xstart >> 3) & 0xFF);
    EPD_SendData((Xend >> 3) & 0xFF);

    EPD_SendCommand(0x45); // SET_RAM_Y_ADDRESS_START_END_POSITION
    EPD_SendData(Ystart & 0xFF);
    EPD_SendData((Ystart >> 8) & 0xFF);
    EPD_SendData(Yend & 0xFF);
    EPD_SendData((Yend >> 8) & 0xFF);
}

void EpdDisplay::EPD_SetCursor(uint16_t Xstart, uint16_t Ystart) {
    EPD_SendCommand(0x4E); // SET_RAM_X_ADDRESS_COUNTER
    EPD_SendData(Xstart & 0xFF);

    EPD_SendCommand(0x4F); // SET_RAM_Y_ADDRESS_COUNTER
    EPD_SendData(Ystart & 0xFF);
    EPD_SendData((Ystart >> 8) & 0xFF);
}

void EpdDisplay::EPD_SetLut(const uint8_t *lut) {
    EPD_SendCommand(0x32);
    writeBytes(lut, 30);
}

void EpdDisplay::EPD_TurnOnDisplay() {
    EPD_SendCommand(0x22);
    EPD_SendData(0xC4);
    EPD_SendCommand(0x20);
    read_busy();
}

void EpdDisplay::EPD_TurnOnDisplayPart() {
    EPD_SendCommand(0x22);
    EPD_SendData(0x04);
    EPD_SendCommand(0x20);
    read_busy();
}

void EpdDisplay::EPD_Init() {
    set_rst_0();
    vTaskDelay(pdMS_TO_TICKS(10));
    set_rst_1();
    vTaskDelay(pdMS_TO_TICKS(10));

    EPD_SendCommand(0x01); // Driver output control
    EPD_SendData((Height - 1) & 0xFF);
    EPD_SendData(((Height - 1) >> 8) & 0xFF);
    EPD_SendData(0x00);

    EPD_SendCommand(0x0C);  // Softstart  
    EPD_SendData(0xD7);
    EPD_SendData(0xD6);
    EPD_SendData(0x9D);

    EPD_SendCommand(0x2C);  // VCOM Voltage
    EPD_SendData(0x9A);

    EPD_SendCommand(0x3A);  // Dummy Line   
    EPD_SendData(0x1A);
    EPD_SendCommand(0x3B);  // Gate time 
    EPD_SendData(0X08);

    EPD_SendCommand(0x11);  // Data entry mode
    EPD_SendData(0x03);

    EPD_SendCommand(0x3C);  // BorderWaveform
    EPD_SendData(0x33);

    EPD_SetWindows(0, 0, Width - 1, Height - 1);

    EPD_SetCursor(0, 0);

    read_busy();

    EPD_SetLut(WF_Full);
}

void EpdDisplay::EPD_Clear() {
    int buffer_len = lcd_spi_data.buffer_len;
    memset(buffer, 0x0, buffer_len);
}

void EpdDisplay::EPD_Display() {
    int buffer_len = lcd_spi_data.buffer_len;
    EPD_SendCommand(0x24);
    assert(buffer);
    writeBytes(buffer, buffer_len);
    EPD_TurnOnDisplay();
}

void EpdDisplay::EPD_DisplayPartBaseImage() {
    int buffer_len = lcd_spi_data.buffer_len;
    EPD_SendCommand(0x24);
    assert(buffer);
    writeBytes(buffer, buffer_len);
    EPD_TurnOnDisplay();
}

void EpdDisplay::EPD_Init_Partial() {
    EPD_Init();
    EPD_SetLut(WF_PARTIAL);

    EPD_SendCommand(0x22); // Display update control
    EPD_SendData(0xC0);
    EPD_SendCommand(0x20);
    read_busy();

    EPD_SendCommand(0x3C); // BorderWaveform
    EPD_SendData(0x80);
}

void EpdDisplay::EPD_DisplayPart() {
    EPD_SendCommand(0x24);
    assert(buffer);
    writeBytes(buffer, Width * Height >> 3);
    EPD_TurnOnDisplayPart();
}

void EpdDisplay::EPD_DrawColorPixel(uint16_t x, uint16_t y, uint8_t color) {
    if (x >= Height || y >= Width) {
        ESP_LOGE("EPD", "Out of bounds pixel: (%d,%d)", x, y);
        return;
    }

    uint16_t physicalX = y;
    uint16_t physicalY = Height - 1 - x;
    uint16_t index = physicalY * (Width >> 3) + (physicalX >> 3);
    uint8_t bit = 7 - (physicalX & 0x07);
    if (color == DRIVER_COLOR_WHITE) {
        buffer[index] |= (0x01 << bit);
    } else {
        buffer[index] &= ~(0x01 << bit);
    }
}
