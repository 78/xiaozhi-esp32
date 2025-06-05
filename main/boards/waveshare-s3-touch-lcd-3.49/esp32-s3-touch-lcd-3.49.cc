#include "wifi_board.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "iot/thing_manager.h"
#include "audio_codecs/box_audio_codec.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <wifi_station.h>
#include "esp_lcd_sh8601.h"
#include "display/lcd_display.h"
#include "lvgl.h"

#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_io_expander_tca9554.h"

#include "waveshare_3.49_lcd_display.h"

#define TAG "waveshare_lcd_3_49"

LV_FONT_DECLARE(font_puhui_16_4);
LV_FONT_DECLARE(font_awesome_16_4);


static esp_io_expander_handle_t io_expander = NULL;

static lv_color_t *flush_dma_buf = NULL;
static uint32_t dma_buf_len = 0;
static SemaphoreHandle_t dma_biy = NULL;
static QueueHandle_t dma_buf_queue = NULL;



static const sh8601_lcd_init_cmd_t lcd_init_cmds[] = 
{
  {0xBB, (uint8_t []){0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5A, 0xA5}, 8, 0},
  {0xCA, (uint8_t []){0x21, 0x36, 0x00, 0x22}, 4, 0},
  {0xA0, (uint8_t []){0x00, 0x30, 0x00, 0x02, 0x00, 0x00, 0x04, 0x3F, 0x20, 0x05, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00}, 16, 0},
  {0xA2, (uint8_t []){0x30, 0x19, 0x60, 0x64, 0x9B, 0x22, 0x50, 0x80, 0xAC, 0x28, 0x7F, 0x7F, 0x7F, 0x20, 0xF8, 0x10, 0x02, 0xFF, 0xFF, 0xF0, 0x90, 0x01, 0x32, 0xA0, 0x91, 0xC0, 0x20, 0x7F, 0xFF, 0x00, 0x04}, 32, 0},
  {0xD0, (uint8_t []){0x80, 0xAC, 0x21, 0x24, 0x08, 0x09, 0x10, 0x01, 0x80, 0x12, 0xC2, 0x00, 0x22, 0x22, 0xAA, 0x03, 0x10, 0x12, 0x40, 0x14, 0x1E, 0x51, 0x15, 0x00, 0x40, 0x10, 0x00, 0x03, 0x7D, 0x12}, 30, 0},
  {0xA3, (uint8_t []){0xA0, 0x06, 0xA9, 0x00, 0x08, 0x02, 0x0A, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x55, 0x55}, 24, 0},
  {0xC1, (uint8_t []){0x33, 0x04, 0x02, 0x02, 0x71, 0x05, 0x24, 0x55, 0x02, 0x00, 0x41, 0x01, 0x53, 0xFF, 0xFF, 0xFF, 0x4F, 0x52, 0x00, 0x4F, 0x52, 0x00, 0x45, 0x3B, 0x0B, 0x02, 0x0D, 0x00, 0xFF, 0x40}, 32, 0},
  {0xC3, (uint8_t []){0x00, 0x00, 0x00, 0x50, 0x03, 0x00, 0x00, 0x00, 0x01, 0x80, 0x01}, 11, 0},
  {0xC4, (uint8_t []){0x00, 0x24, 0x33, 0x80, 0x11, 0xEA, 0x64, 0x32, 0xC8, 0x64, 0xC8, 0x32, 0x90, 0x90, 0x11, 0x06, 0xDC, 0xFA, 0x00, 0x00, 0x80, 0xFE, 0x10, 0x10, 0x00, 0x0A, 0x0A, 0x44, 0x50}, 29, 0},
  {0xC5, (uint8_t []){0x18, 0x00, 0x00, 0x03, 0xFE, 0x08, 0x68, 0x30, 0x10, 0x10, 0x88, 0xDE, 0x0D, 0x08, 0x0F, 0x0F, 0x01, 0x08, 0x68, 0x30, 0x10, 0x10, 0x00}, 22, 0},
  {0xC6, (uint8_t []){0x05, 0x0A, 0x05, 0x0A, 0x00, 0xE0, 0x2E, 0x0B, 0x12, 0x22, 0x12, 0x22, 0x01, 0x00, 0x00, 0x02, 0x6A, 0x18, 0xC8, 0x22}, 19, 0},
  {0xC7, (uint8_t []){0x50, 0x36, 0x28, 0x00, 0xA2, 0x80, 0x8F, 0x00, 0x80, 0xFF, 0x07, 0x11, 0x9C, 0x6F, 0xFF, 0x24, 0x0C, 0x0D, 0x0E, 0x0F}, 20, 0},
  {0xC9, (uint8_t []){0x33, 0x44, 0x44, 0x01}, 4, 0},
  {0xCF, (uint8_t []){0x2C, 0x1E, 0x88, 0x58, 0x13, 0x18, 0x56, 0x18, 0x1E, 0x68, 0xF8, 0x00, 0x66, 0x0D, 0x22, 0xC4, 0x0C, 0x77, 0x22, 0x44, 0xAA, 0x55, 0x04, 0x04, 0x12, 0xA0, 0x08}, 28, 0},
  {0xD5, (uint8_t []){0x50, 0x60, 0x8A, 0x00, 0x35, 0x04, 0x71, 0x02, 0x03, 0x03, 0x03, 0x00, 0x04, 0x02, 0x13, 0x46, 0x03, 0x03, 0x03, 0x03, 0x86, 0x00, 0x00, 0x00, 0x80, 0x52, 0x7C, 0x00, 0x00, 0x00}, 30, 0},
  {0xD6, (uint8_t []){0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE, 0x00, 0x00, 0x01, 0x83, 0x03, 0x03, 0x33, 0x03, 0x03, 0x33, 0x3F, 0x03, 0x03, 0x03, 0x20, 0x20, 0x00, 0x24, 0x51, 0x23, 0x01, 0x00}, 31, 0},
  {0xD7, (uint8_t []){0x18, 0x1A, 0x1B, 0x1F, 0x0A, 0x08, 0x0E, 0x0C, 0x00, 0x1F, 0x1D, 0x1F, 0x50, 0x60, 0x04, 0x00, 0x1F, 0x1F, 0x1F}, 19, 0},
  {0xD8, (uint8_t []){0x18, 0x1A, 0x1B, 0x1F, 0x0B, 0x09, 0x0F, 0x0D, 0x01, 0x1F, 0x1D, 0x1F}, 12, 0},
  {0xD9, (uint8_t []){0x0F, 0x09, 0x0B, 0x1F, 0x18, 0x19, 0x1F, 0x01, 0x1E, 0x1D, 0x1F}, 11, 0},
  {0xDD, (uint8_t []){0x0E, 0x08, 0x0A, 0x1F, 0x18, 0x19, 0x1F, 0x00, 0x1E, 0x1A, 0x1F}, 11, 0},
  {0xDF, (uint8_t []){0x44, 0x33, 0x4B, 0x69, 0x00, 0x0A, 0x02, 0x90}, 8, 0},
  {0xE0, (uint8_t []){0x35, 0x08, 0x19, 0x1C, 0x0C, 0x09, 0x13, 0x2A, 0x54, 0x21, 0x0B, 0x15, 0x13, 0x25, 0x27, 0x08, 0x00}, 17, 0},
  {0xE1, (uint8_t []){0x3E, 0x08, 0x19, 0x1C, 0x0C, 0x08, 0x13, 0x2A, 0x54, 0x21, 0x0B, 0x14, 0x13, 0x26, 0x27, 0x08, 0x0F}, 17, 0},
  {0xE2, (uint8_t []){0x19, 0x20, 0x0A, 0x11, 0x09, 0x06, 0x11, 0x25, 0xD4, 0x22, 0x0B, 0x13, 0x12, 0x2D, 0x32, 0x2F, 0x03}, 17, 0},
  {0xE3, (uint8_t []){0x38, 0x20, 0x0A, 0x11, 0x09, 0x06, 0x11, 0x25, 0xC4, 0x21, 0x0A, 0x12, 0x11, 0x2C, 0x32, 0x2F, 0x27}, 17, 0},
  {0xE4, (uint8_t []){0x19, 0x20, 0x0D, 0x14, 0x0D, 0x08, 0x12, 0x2A, 0xD4, 0x26, 0x0E, 0x15, 0x13, 0x34, 0x39, 0x2F, 0x03}, 17, 0},
  {0xE5, (uint8_t []){0x38, 0x20, 0x0D, 0x13, 0x0D, 0x07, 0x12, 0x29, 0xC4, 0x25, 0x0D, 0x15, 0x12, 0x33, 0x39, 0x2F, 0x27}, 17, 0},
  {0xBB, (uint8_t []){0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 8, 0},
  {0x11, (uint8_t []){}, 0, 150},
  {0x29, (uint8_t []){}, 0, 150},
};

static void example_dma_task(void *arg)
{
  uint8_t Countup = 0;
  uint16_t offsety1 = 0;
  uint16_t offsety2 = 64;
  esp_lcd_panel_handle_t disp_ctx = (esp_lcd_panel_handle_t )arg;
  for(;;)
  {
    xQueueReceive(dma_buf_queue,flush_dma_buf,portMAX_DELAY);
    esp_lcd_panel_draw_bitmap(disp_ctx, 0, offsety1, 180, offsety2, flush_dma_buf);
    offsety1 += 64;
    offsety2 += 64;
    Countup++;
    if(Countup == 10)
    {
      Countup = 0;
      offsety1 = 0;
      offsety2 = 64;
    }
  }
}
static bool lvgl_port_flush_io_ready_callback(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xSemaphoreGiveFromISR(dma_biy,&xHigherPriorityTaskWoken);
  return false;
}

class CustomLcdDisplay : public userSpiLcdDisplay {
  public:
  uint32_t color_bytes;
  static void my_draw_event_cb(lv_event_t *e)
  {
    lv_area_t *area = (lv_area_t *)lv_event_get_param(e);

    uint16_t x1 = area->x1;
    uint16_t x2 = area->x2;

    uint16_t y1 = area->y1;
    uint16_t y2 = area->y2;

    // round the start of coordinate down to the nearest 2M number
    area->x1 = (x1 >> 1) << 1;
    area->y1 = (y1 >> 1) << 1;
    // round the end of coordinate up to the nearest 2N+1 number
    area->x2 = ((x2 >> 1) << 1) + 1;
    area->y2 = ((y2 >> 1) << 1) + 1;
  }
  static void lvgl_port_flush_callback(lv_display_t *drv, const lv_area_t *area, uint8_t *color_map)
  {
    size_t len = lv_area_get_size(area);
    lv_draw_sw_rgb565_swap(color_map, len);
    uint8_t *full = color_map;
    for(uint8_t i = 0; i<10; i++)
    {
      xQueueSend(dma_buf_queue,full,portMAX_DELAY);
      xSemaphoreTake(dma_biy,portMAX_DELAY);
      full += dma_buf_len;
    }
    lv_disp_flush_ready(drv);
  }
  CustomLcdDisplay(esp_lcd_panel_io_handle_t io_handle,
                  esp_lcd_panel_handle_t panel_handle,
                  int width,
                  int height,
                  int offset_x,
                  int offset_y,
                  bool mirror_x,
                  bool mirror_y,
                  bool swap_xy)
      : userSpiLcdDisplay(io_handle, panel_handle,
                  width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy,
                  {
                    .text_font = &font_puhui_16_4,
                    .icon_font = &font_awesome_16_4,
                    .emoji_font = font_emoji_32_init(),
                  }) {
    DisplayLockGuard lock(this);
    color_bytes = lv_color_format_get_size(LV_COLOR_FORMAT_RGB565);
    flush_dma_buf = (lv_color_t *)heap_caps_malloc(width *(height / 10) * color_bytes , MALLOC_CAP_DMA);
    dma_buf_len = width *(height / 10) * color_bytes;
    dma_biy = xSemaphoreCreateBinary();
    dma_buf_queue = xQueueCreate(1,dma_buf_len);
    lv_display_add_event_cb(display_, my_draw_event_cb, LV_EVENT_INVALIDATE_AREA, NULL);
    lv_display_set_flush_cb(display_, lvgl_port_flush_callback);
    const esp_lcd_panel_io_callbacks_t cbs = 
    {
      .on_color_trans_done = lvgl_port_flush_io_ready_callback,
    };
    /* Register done callback */
    ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(panel_io_, &cbs, NULL));
    xTaskCreatePinnedToCore(example_dma_task, "example_dma_task", 4000, panel_, 5, NULL,1); //运行于内核_1   //转储SPIRAM数据到DMA
    SetupUI();
  }
};

class waveshare_s3_lcd_3_49 : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    Button boot_button_;
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_io_handle_t io_handle = NULL;
    CustomLcdDisplay* display_;
    void InitializeI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }
    void tsa9554_init(){
      esp_io_expander_new_i2c_tca9554(i2c_bus_, ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000, &io_expander);
    }
    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
        });
        boot_button_.OnPressDown([this]() {
            Application::GetInstance().StartListening();
        });
        boot_button_.OnPressUp([this]() {
            Application::GetInstance().StopListening();
        });
    }
    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        //thing_manager.AddThing(iot::CreateThing("Screen"));
    }
    void InitializeSpi() 
    {
      spi_bus_config_t buscfg =
      {            
          .data0_io_num = LCD_D0,             
          .data1_io_num = LCD_D1, 
          .sclk_io_num = LCD_PCLK,            
          .data2_io_num = LCD_D2,             
          .data3_io_num = LCD_D3,             
          .max_transfer_sz = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * sizeof(uint16_t),
      };
      ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }
    /*LCD-3.49*/
    void user_lcd_init()
    {
      gpio_config_t gpio_conf = {};
      gpio_conf.intr_type = GPIO_INTR_DISABLE;
      gpio_conf.mode = GPIO_MODE_OUTPUT;
      gpio_conf.pin_bit_mask = ((uint64_t)0X01<<LCD_RST);
      gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
      gpio_conf.pull_up_en = GPIO_PULLUP_ENABLE;

      ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&gpio_conf));

      const esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = LCD_CS,          
        .dc_gpio_num = -1,          
        .spi_mode = 0,              
        .pclk_hz = 40 * 1000 * 1000,
        .trans_queue_depth = 2,     
        .on_color_trans_done = NULL,  
        .user_ctx = NULL,         
        .lcd_cmd_bits = 32,         
        .lcd_param_bits = 8,        
        .flags = {                  
            .quad_mode = true,      
        },                          
      };
      ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &io_handle));
      sh8601_vendor_config_t vendor_config = 
      {
        .init_cmds = lcd_init_cmds,             // Uncomment these line if use custom initialization commands
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]), // sizeof(axs15231b_lcd_init_cmd_t),
        .flags = 
        {
          .use_qspi_interface = 1,
        },
      };
      const esp_lcd_panel_dev_config_t panel_config = 
      {
        .reset_gpio_num = LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,     // Implemented by LCD command `36h`
        .bits_per_pixel = 16,                           // Implemented by LCD command `3Ah` (16/18)
        .vendor_config = &vendor_config,
      };
      ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(io_handle, &panel_config, &panel_handle));
      
      ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
      ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
      ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
      ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
      #if 1
      display_ = new CustomLcdDisplay(io_handle, panel_handle,
      EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
      #endif
    }
    /*LCD-3.49*/
public:
    waveshare_s3_lcd_3_49() : boot_button_(BOOT_BUTTON_GPIO)
    {
      InitializeI2c();
      InitializeSpi();
      tsa9554_init();
      esp_io_expander_set_dir(io_expander, IO_EXPANDER_PIN_NUM_7, IO_EXPANDER_OUTPUT);
      esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_7, 1);
      InitializeButtons();
      user_lcd_init();
      InitializeIot();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static BoxAudioCodec audio_codec(
            i2c_bus_, 
            AUDIO_INPUT_SAMPLE_RATE, 
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, 
            AUDIO_I2S_GPIO_BCLK, 
            AUDIO_I2S_GPIO_WS, 
            AUDIO_I2S_GPIO_DOUT, 
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, 
            AUDIO_CODEC_ES8311_ADDR, 
            AUDIO_CODEC_ES7210_ADDR, 
            AUDIO_INPUT_REFERENCE);
        return &audio_codec;
    }
    virtual Display* GetDisplay() override 
    {
      return display_;
    }
};

DECLARE_BOARD(waveshare_s3_lcd_3_49);