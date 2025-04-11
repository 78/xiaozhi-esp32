#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include <wifi_station.h>

#include "application.h"
#include "audio_codecs/no_audio_codec.h"
#include "button.h"
#include "config.h"
#include "display/lcd_display.h"
#include "iot/thing_manager.h"
#include "led/single_led.h"
#include "system_reset.h"
#include "wifi_board.h"

#define TAG "FreenoveMediaKit"

LV_FONT_DECLARE(font_puhui_16_4);
LV_FONT_DECLARE(font_awesome_16_4);

#define LCD_SPI_HOST SPI3_HOST
#define LCD_RST_PIN GPIO_NUM_20

button_adc_config_t adc_btn_config0 = {
    .unit_id = ADC_UNIT_2,
    .adc_channel = ADC_CHANNEL_8,
    .button_index = 0,
    .min = 0,
    .max = 660 * 0 + 100,
};
button_adc_config_t adc_btn_config1 = {
    .unit_id = ADC_UNIT_2,
    .adc_channel = ADC_CHANNEL_8,
    .button_index = 1,
    .min = 660 * 1 - 100,
    .max = 660 * 1 + 100,
};
button_adc_config_t adc_btn_config2 = {
    .unit_id = ADC_UNIT_2,
    .adc_channel = ADC_CHANNEL_8,
    .button_index = 2,
    .min = 660 * 2 - 100,
    .max = 660 * 2 + 100,
};
button_adc_config_t adc_btn_config3 = {
    .unit_id = ADC_UNIT_2,
    .adc_channel = ADC_CHANNEL_8,
    .button_index = 3,
    .min = 660 * 3 - 100,
    .max = 660 * 3 + 100,
};
button_adc_config_t adc_btn_config4 = {
    .unit_id = ADC_UNIT_2,
    .adc_channel = ADC_CHANNEL_8,
    .button_index = 4,
    .min = 660 * 4 - 100,
    .max = 660 * 4 + 100,
};

class FreenoveMediaKit : public WifiBoard {
 private:
  Button boot_button_;
  LcdDisplay *display_;
  Button adcButton0;
  Button adcButton1;
  Button adcButton2;
  Button adcButton3;
  Button adcButton4;

  void resetLcd() {
    // 配置引脚为输出模式，并设置默认电平为低
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << LCD_RST_PIN);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    gpio_set_level(LCD_RST_PIN, 0);  // 设置引脚为低电平
    vTaskDelay(pdMS_TO_TICKS(10));
    // vTaskDelay(50/portTICK_RATE_MS)
    gpio_set_level(LCD_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    // 配置RST引脚为开漏输入模式
    io_conf.mode = GPIO_MODE_INPUT_OUTPUT_OD;
    // io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);
  }

  void InitializeSpi() {
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
    buscfg.miso_io_num = GPIO_NUM_NC;
    buscfg.sclk_io_num = DISPLAY_CLK_PIN;
    buscfg.quadwp_io_num = GPIO_NUM_NC;
    buscfg.quadhd_io_num = GPIO_NUM_NC;
    buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
  }

  void InitializeLcdDisplay() {
    esp_lcd_panel_io_handle_t panel_io = nullptr;
    esp_lcd_panel_handle_t panel = nullptr;
    // 液晶屏控制IO初始化
    ESP_LOGD(TAG, "Install panel IO");
    esp_lcd_panel_io_spi_config_t io_config = {};
    io_config.cs_gpio_num = DISPLAY_CS_PIN;
    io_config.dc_gpio_num = DISPLAY_DC_PIN;
    io_config.spi_mode = 3;
    io_config.pclk_hz = 1 * 1000 * 1000;
    io_config.trans_queue_depth = 10;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;
    // io_config.on_color_trans_done.
    ESP_ERROR_CHECK(
        esp_lcd_new_panel_io_spi(LCD_SPI_HOST, &io_config, &panel_io));
    // 初始化液晶屏驱动芯片
    ESP_LOGD(TAG, "Install LCD driver");
    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = DISPLAY_RST_PIN;
    panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
    panel_config.bits_per_pixel = 16;
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
    ESP_LOGI(TAG, "Install LCD driver ST7789");
    esp_lcd_panel_reset(panel);
    resetLcd();
    esp_lcd_panel_init(panel);
    esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
    esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
    esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
    display_ = new SpiLcdDisplay(
        panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X,
        DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
        {
            .text_font = &font_puhui_16_4,
            .icon_font = &font_awesome_16_4,
            .emoji_font = DISPLAY_HEIGHT >= 240 ? font_emoji_64_init()
                                                : font_emoji_32_init(),
        });
  }

  void InitializeButtons() {
    boot_button_.OnClick([this]() {
      auto &app = Application::GetInstance();
      if (app.GetDeviceState() == kDeviceStateStarting &&
          !WifiStation::GetInstance().IsConnected()) {
        ResetWifiConfiguration();
      }
      app.ToggleChatState();
    });
  }

  void InitializeAdcButtons() {
    adcButton0.OnClick([this]() { ESP_LOGI(TAG, "adcButton0 Click"); });
    adcButton1.OnClick([this]() { ESP_LOGI(TAG, "adcButton1 Click"); });
    adcButton2.OnClick([this]() { ESP_LOGI(TAG, "adcButton2 Click"); });
    adcButton3.OnClick([this]() { ESP_LOGI(TAG, "adcButton3 Click"); });
    adcButton4.OnClick([this]() { ESP_LOGI(TAG, "adcButton4 Click"); });
  }

  // 物联网初始化，添加对 AI 可见设备
  void InitializeIot() {
    auto &thing_manager = iot::ThingManager::GetInstance();
    thing_manager.AddThing(iot::CreateThing("Speaker"));
    thing_manager.AddThing(iot::CreateThing("Screen"));
  }

 public:
  FreenoveMediaKit()
      : boot_button_(BOOT_BUTTON_GPIO),
        adcButton0(adc_btn_config0),
        adcButton1(adc_btn_config1),
        adcButton2(adc_btn_config2),
        adcButton3(adc_btn_config3),
        adcButton4(adc_btn_config4) {
    InitializeSpi();
    InitializeLcdDisplay();
    InitializeButtons();
    InitializeAdcButtons();
    InitializeIot();
    GetBacklight()->SetBrightness(100);
  }

  virtual Led *GetLed() override {
    static SingleLed led(BUILTIN_LED_GPIO);
    return &led;
  }

  virtual AudioCodec *GetAudioCodec() override {
    static NoAudioCodecSimplex audio_codec(
        AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
        AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK,
        AUDIO_I2S_SPK_GPIO_DOUT, I2S_STD_SLOT_RIGHT, AUDIO_I2S_MIC_GPIO_SCK,
        AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN, I2S_STD_SLOT_RIGHT);

    return &audio_codec;
  }

  virtual Display *GetDisplay() override { return display_; }

  virtual Backlight *GetBacklight() override {
    static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN,
                                  DISPLAY_BACKLIGHT_OUTPUT_INVERT);
    return &backlight;
  }
};

DECLARE_BOARD(FreenoveMediaKit);
