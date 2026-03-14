#include "custom_lcd_display.h"
#include <esp_lcd_panel_io.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <stdio.h>
#include <vector>
#include "board.h"
#include "config.h"
#include "esp_lvgl_port.h"
#include "settings.h"

#define TAG "CustomEpdDisplay"

#define BYTES_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565))
#define BUFF_SIZE (EXAMPLE_LCD_WIDTH * EXAMPLE_LCD_HEIGHT * BYTES_PER_PIXEL)

void CustomEpdDisplay::lvgl_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* color_p) {
    assert(disp != NULL);
    CustomEpdDisplay* driver = (CustomEpdDisplay*)lv_display_get_user_data(disp);
    uint16_t* buffer = (uint16_t*)color_p;
    driver->EPD_Clear();
    for (int y = area->y1; y <= area->y2; y++) {
        for (int x = area->x1; x <= area->x2; x++) {
            uint8_t color = (*buffer < 0x7fff) ? DRIVER_COLOR_BLACK : DRIVER_COLOR_WHITE;
            driver->EPD_DrawColorPixel(x, y, color);
            buffer++;
        }
    }
    driver->EPD_DisplayPart();
    lv_disp_flush_ready(disp);
}

CustomEpdDisplay::CustomEpdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                                   int width, int height, int offset_x, int offset_y, bool mirror_x,
                                   bool mirror_y, bool swap_xy, custom_epd_spi_t _epd_spi_data)
    : LcdDisplay(panel_io, panel, width, height),
      epd_spi_data(_epd_spi_data),
      Width(width),
      Height(height) {
    ESP_LOGI(TAG, "Initialize SPI");
    spi_port_init();
    spi_gpio_init();

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 2;
    port_cfg.timer_period_ms = 50;
    lvgl_port_init(&port_cfg);
    lvgl_port_lock(0);

    buffer = (uint8_t*)heap_caps_malloc(epd_spi_data.buffer_len, MALLOC_CAP_SPIRAM);
    assert(buffer);
    display_ = lv_display_create(width, height); /* 以水平和垂直分辨率（像素）进行基本初始化 */
    lv_display_set_flush_cb(display_, lvgl_flush_cb);
    lv_display_set_user_data(display_, this);

    uint8_t* buffer_1 = NULL;
    buffer_1 = (uint8_t*)heap_caps_malloc(BUFF_SIZE, MALLOC_CAP_SPIRAM);
    assert(buffer_1);
    lv_display_set_buffers(display_, buffer_1, NULL, BUFF_SIZE, LV_DISPLAY_RENDER_MODE_FULL);

    ESP_LOGI(TAG, "EPD init");
    EPD_Init();
    ESP_LOGI(TAG, "EPD Clear");
    EPD_Clear();
    EPD_Display();

    lvgl_port_unlock();
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add display");
        return;
    }

    ESP_LOGI(TAG, "ui start");

    SetupUI();
}

CustomEpdDisplay::~CustomEpdDisplay() {}

void CustomEpdDisplay::spi_gpio_init() {
    int rst = epd_spi_data.rst;
    int cs = epd_spi_data.cs;
    int dc = epd_spi_data.dc;
    int busy = epd_spi_data.busy;

    gpio_config_t gpio_conf = {};
    gpio_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_conf.mode = GPIO_MODE_OUTPUT;
    gpio_conf.pin_bit_mask = (0x1ULL << rst) | (0x1ULL << dc) | (0x1ULL << cs);
    gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&gpio_conf));

    gpio_conf.mode = GPIO_MODE_INPUT;
    gpio_conf.pin_bit_mask = (0x1ULL << busy);
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&gpio_conf));

    set_rst_1();
}

void CustomEpdDisplay::spi_port_init() {
    int mosi = epd_spi_data.mosi;
    int scl = epd_spi_data.scl;
    int spi_host = epd_spi_data.spi_host;
    esp_err_t ret;
    spi_bus_config_t buscfg = {};
    buscfg.miso_io_num = -1;
    buscfg.mosi_io_num = mosi;
    buscfg.sclk_io_num = scl;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = 4096;

    spi_device_interface_config_t devcfg = {};
    devcfg.spics_io_num = -1;
    devcfg.clock_speed_hz = 20 * 1000 * 1000;  // Clock out at 10 MHz
    devcfg.mode = 0;                           // SPI mode 0
    devcfg.queue_size = 7;  // We want to be able to queue 7 transactions at a time

    ret = spi_bus_initialize((spi_host_device_t)spi_host, &buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);
    ret = spi_bus_add_device((spi_host_device_t)spi_host, &devcfg, &spi);
    ESP_ERROR_CHECK(ret);
}

void CustomEpdDisplay::read_busy() {
    int busy = epd_spi_data.busy;
    while (gpio_get_level((gpio_num_t)busy) == 1) {
        vTaskDelay(pdMS_TO_TICKS(5));  // LOW: idle, HIGH: busy
    }
}

void CustomEpdDisplay::SPI_SendByte(uint8_t data) {
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8;
    t.tx_buffer = &data;
    ret = spi_device_polling_transmit(spi, &t);  // Transmit!
    assert(ret == ESP_OK);                       // Should have had no issues.
}

void CustomEpdDisplay::EPD_SendData(uint8_t data) {
    set_dc_1();
    set_cs_0();
    SPI_SendByte(data);
    set_cs_1();
}

void CustomEpdDisplay::EPD_SendCommand(uint8_t command) {
    set_dc_0();
    set_cs_0();
    SPI_SendByte(command);
    set_cs_1();
}

void CustomEpdDisplay::writeBytes(uint8_t* buffer, int len) {
    set_dc_1();
    set_cs_0();

    const int MAX_SPI_TRANSFER = 4096;

    int remaining = len;
    int offset = 0;

    while (remaining > 0) {
        int chunk_size = (remaining > MAX_SPI_TRANSFER) ? MAX_SPI_TRANSFER : remaining;

        esp_err_t ret;
        spi_transaction_t t;
        memset(&t, 0, sizeof(t));
        t.length = 8 * chunk_size;
        t.tx_buffer = buffer + offset;
        ret = spi_device_polling_transmit(spi, &t);  // Transmit!

        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SPI transmit failed at offset %d, chunk %d: %s", offset, chunk_size,
                     esp_err_to_name(ret));
            break;
        }

        remaining -= chunk_size;
        offset += chunk_size;
    }

    set_cs_1();
}

void CustomEpdDisplay::writeBytes(const uint8_t* buffer, int len) {
    set_dc_1();
    set_cs_0();

    const int MAX_SPI_TRANSFER = 4096;

    int remaining = len;
    int offset = 0;

    while (remaining > 0) {
        int chunk_size = (remaining > MAX_SPI_TRANSFER) ? MAX_SPI_TRANSFER : remaining;

        esp_err_t ret;
        spi_transaction_t t;
        memset(&t, 0, sizeof(t));
        t.length = 8 * chunk_size;
        t.tx_buffer = buffer + offset;
        ret = spi_device_polling_transmit(spi, &t);  // Transmit!

        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SPI transmit failed at offset %d, chunk %d: %s", offset, chunk_size,
                     esp_err_to_name(ret));
            break;
        }

        remaining -= chunk_size;
        offset += chunk_size;
    }

    set_cs_1();
}

void CustomEpdDisplay::EPD_SetWindows(uint16_t Xstart, uint16_t Ystart, uint16_t Xend,
                                      uint16_t Yend) {
    EPD_SendCommand(0x44);  // SET_RAM_X_ADDRESS_START_END_POSITION
    EPD_SendData((Xstart * 8) & 0xFF);
    EPD_SendData(((Xstart * 8) >> 8) & 0xFF);
    EPD_SendData((Xend * 8) & 0xFF);
    EPD_SendData(((Xend * 8) >> 8) & 0xFF);

    EPD_SendCommand(0x45);  // SET_RAM_Y_ADDRESS_START_END_POSITION
    EPD_SendData(Yend & 0xFF);
    EPD_SendData((Yend >> 8) & 0xFF);
    EPD_SendData(Ystart & 0xFF);
    EPD_SendData((Ystart >> 8) & 0xFF);
}

void CustomEpdDisplay::EPD_SetCursor(uint16_t Xstart, uint16_t Ystart) {
    EPD_SendCommand(0x4E);
    EPD_SendData((Xstart * 8) & 0xFF);
    EPD_SendData(((Xstart * 8) >> 8) & 0xFF);

    EPD_SendCommand(0x4F);
    EPD_SendData(Ystart & 0xFF);
    EPD_SendData((Ystart >> 8) & 0xFF);
}

void CustomEpdDisplay::EPD_TurnOnDisplay() {
    EPD_SendCommand(0x22);
    EPD_SendData(0xF7);
    EPD_SendCommand(0x20);
    read_busy();
}

void CustomEpdDisplay::EPD_TurnOnDisplayPart() {
    EPD_SendCommand(0x22);
    EPD_SendData(0xFF);
    EPD_SendCommand(0x20);
    read_busy();
}

void CustomEpdDisplay::EPD_Init() {
    set_rst_1();
    vTaskDelay(pdMS_TO_TICKS(50));
    set_rst_0();
    vTaskDelay(pdMS_TO_TICKS(2));
    set_rst_1();
    vTaskDelay(pdMS_TO_TICKS(50));

    read_busy();
    EPD_SendCommand(0x12);  // SWRESET
    read_busy();

    EPD_SendCommand(0x18);
    EPD_SendData(0x80);

    EPD_SendCommand(0x0C);  // Driver output control
    EPD_SendData(0xAE);
    EPD_SendData(0xC7);
    EPD_SendData(0xC3);
    EPD_SendData(0xC0);
    EPD_SendData(0x80);

    EPD_SendCommand(0x01);  // Driver output control
    EPD_SendData((Height - 1) % 256);
    EPD_SendData((Height - 1) / 256);
    EPD_SendData(0x02);

    EPD_SendCommand(0x3C);  // BorderWavefrom
    EPD_SendData(0x01);

    EPD_SendCommand(0x11);  // data entry mode
    EPD_SendData(0x01);

    EPD_SendCommand(0x44);  // set Ram-X address start/end position
    EPD_SendData(0x00);
    EPD_SendData(0x00);
    EPD_SendData((Width - 1) % 256);
    EPD_SendData((Width - 1) / 256);

    EPD_SendCommand(0x45);  // set Ram-Y address start/end position
    EPD_SendData((Height - 1) % 256);
    EPD_SendData((Height - 1) / 256);
    EPD_SendData(0x00);
    EPD_SendData(0x00);

    EPD_SendCommand(0x4E);  // set RAM x address count to 0;
    EPD_SendData(0x00);
    EPD_SendData(0x00);
    EPD_SendCommand(0x4F);  // set RAM y address count to 0X199;
    EPD_SendData(0x00);
    EPD_SendData(0x00);

    read_busy();
}

void CustomEpdDisplay::EPD_Clear() {
    int buffer_len = epd_spi_data.buffer_len;
    memset(buffer, 0xff, buffer_len);
}

void CustomEpdDisplay::EPD_Display() {
    int buffer_len = epd_spi_data.buffer_len;
    EPD_SendCommand(0x24);
    assert(buffer);
    writeBytes(buffer, buffer_len);
    EPD_TurnOnDisplay();
}

void CustomEpdDisplay::EPD_DisplayPartBaseImage() {
    int buffer_len = epd_spi_data.buffer_len;
    EPD_SendCommand(0x24);
    assert(buffer);
    writeBytes(buffer, buffer_len);
    EPD_SendCommand(0x26);
    writeBytes(buffer, buffer_len);
    EPD_TurnOnDisplay();
}

void CustomEpdDisplay::EPD_Init_Partial() {
    set_rst_1();
    vTaskDelay(pdMS_TO_TICKS(50));
    set_rst_0();
    vTaskDelay(pdMS_TO_TICKS(2));
    set_rst_1();
    vTaskDelay(pdMS_TO_TICKS(50));

    read_busy();

    EPD_SendCommand(0x18);
    EPD_SendData(0x80);

    EPD_SendCommand(0x3C);
    EPD_SendData(0x80);

    EPD_SetWindows(0, Width - 1, Height - 1, 0);

    EPD_SetCursor(0, Height - 1);

    read_busy();
}

void CustomEpdDisplay::EPD_DisplayPart() {
    EPD_SendCommand(0x24);
    assert(buffer);
    writeBytes(buffer, 48000);
    EPD_TurnOnDisplayPart();
}

void CustomEpdDisplay::EPD_Sleep() {
    EPD_SendCommand(0x10);  // enter deep sleep
    EPD_SendData(0x01);
    vTaskDelay(pdMS_TO_TICKS(10));
    set_rst_0();
    set_cs_0();
    set_dc_0();
}

void CustomEpdDisplay::EPD_DrawColorPixel(uint16_t x, uint16_t y, uint8_t color) {
    if (x >= Width || y >= Height) {
        ESP_LOGE("EPD", "Out of bounds pixel: (%d,%d)", x, y);
        return;
    }

    uint16_t index = y * Width / 8 + (x >> 3);
    uint8_t bit = 7 - (x & 0x07);
    if (color == DRIVER_COLOR_WHITE) {
        buffer[index] |= (0x01 << bit);
    } else {
        buffer[index] &= ~(0x01 << bit);
    }
}
