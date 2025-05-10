#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include <wifi_station.h>

#include <numeric>

#include "application.h"
#include "assets/lang_config.h"
#include "button.h"
#include "config.h"
#include "display/lcd_display.h"
#include "iot/thing_manager.h"
#include "led/single_led.h"
#include "sph0645_audio_codec.h"
#include "system_reset.h"
#include "wifi_board.h"
#define TAG "NulllabAIVox"

#ifndef CONFIG_IDF_TARGET_ESP32S3
#error "This board is only supported on ESP32-S3, please use ESP32-S3 target by using 'idf.py esp32s3' and 'idf.py menuconfig' to configure the project."
#endif

LV_FONT_DECLARE(font_puhui_16_4);
LV_FONT_DECLARE(font_awesome_16_4);

class NulllabAIVox : public WifiBoard {
 private:
  Button boot_button_;
  Button volume_up_button_;
  Button volume_down_button_;
  uint32_t current_band_ = UINT32_MAX;
  LcdDisplay* display_;
  adc_oneshot_unit_handle_t battery_adc_handle_ = nullptr;
  std::vector<int32_t> battery_adc_samples_;

  void InitializeSpi() {
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
    buscfg.miso_io_num = GPIO_NUM_NC;
    buscfg.sclk_io_num = DISPLAY_CLK_PIN;
    buscfg.quadwp_io_num = GPIO_NUM_NC;
    buscfg.quadhd_io_num = GPIO_NUM_NC;
    buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
  }

  void InitializeLcdDisplay() {
    esp_lcd_panel_io_handle_t panel_io = nullptr;
    esp_lcd_panel_handle_t panel = nullptr;
    // 液晶屏控制IO初始化
    ESP_LOGD(TAG, "Install panel IO");
    esp_lcd_panel_io_spi_config_t io_config = {};
    io_config.cs_gpio_num = DISPLAY_CS_PIN;
    io_config.dc_gpio_num = DISPLAY_DC_PIN;
    io_config.spi_mode = DISPLAY_SPI_MODE;
    io_config.pclk_hz = 40 * 1000 * 1000;
    io_config.trans_queue_depth = 10;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

    // 初始化液晶屏驱动芯片
    ESP_LOGD(TAG, "Install LCD driver");
    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = DISPLAY_RST_PIN;
    panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
    panel_config.bits_per_pixel = 16;
#if defined(LCD_TYPE_ILI9341_SERIAL)
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_config, &panel));
#elif defined(LCD_TYPE_GC9A01_SERIAL)
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(panel_io, &panel_config, &panel));
    gc9a01_vendor_config_t gc9107_vendor_config = {
        .init_cmds = gc9107_lcd_init_cmds,
        .init_cmds_size =
            sizeof(gc9107_lcd_init_cmds) / sizeof(gc9a01_lcd_init_cmd_t),
    };
#else
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
#endif

    esp_lcd_panel_reset(panel);

    esp_lcd_panel_init(panel);
    esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
    esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
    esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
#ifdef LCD_TYPE_GC9A01_SERIAL
    panel_config.vendor_config = &gc9107_vendor_config;
#endif
    display_ = new SpiLcdDisplay(
        panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X,
        DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
        {
            .text_font = &font_puhui_16_4,
            .icon_font = &font_awesome_16_4,
#if CONFIG_USE_WECHAT_MESSAGE_STYLE
            .emoji_font = font_emoji_32_init(),
#else
            .emoji_font = DISPLAY_HEIGHT >= 240 ? font_emoji_64_init()
                                                : font_emoji_32_init(),
#endif
        });
  }

  void InitializeButtons() {
    boot_button_.OnClick([this]() {
      auto& app = Application::GetInstance();
      if (app.GetDeviceState() == kDeviceStateStarting &&
          !WifiStation::GetInstance().IsConnected()) {
        ResetWifiConfiguration();
      }
      app.ToggleChatState();
    });

    volume_up_button_.OnClick([this]() {
      auto codec = GetAudioCodec();
      auto volume = codec->output_volume() + 10;
      if (volume > 100) {
        volume = 100;
      }
      codec->SetOutputVolume(volume);
      GetDisplay()->ShowNotification(Lang::Strings::VOLUME +
                                     std::to_string(volume));
    });

    volume_up_button_.OnLongPress([this]() {
      GetAudioCodec()->SetOutputVolume(100);
      GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);
    });

    volume_down_button_.OnClick([this]() {
      auto codec = GetAudioCodec();
      auto volume = codec->output_volume() - 10;
      if (volume < 0) {
        volume = 0;
      }
      codec->SetOutputVolume(volume);
      GetDisplay()->ShowNotification(Lang::Strings::VOLUME +
                                     std::to_string(volume));
    });

    volume_down_button_.OnLongPress([this]() {
      GetAudioCodec()->SetOutputVolume(0);
      GetDisplay()->ShowNotification(Lang::Strings::MUTED);
    });
  }

  // 物联网初始化，添加对 AI 可见设备
  void InitializeIot() {
    auto& thing_manager = iot::ThingManager::GetInstance();
    thing_manager.AddThing(iot::CreateThing("Speaker"));
    thing_manager.AddThing(iot::CreateThing("Screen"));
    thing_manager.AddThing(iot::CreateThing("Lamp"));
  }

 public:
  NulllabAIVox()
      : boot_button_(BOOT_BUTTON_GPIO),
        volume_up_button_(VOLUME_UP_BUTTON_GPIO),
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO) {
    gpio_config_t io_config = {
        .pin_bit_mask = (1ULL << GPIO_NUM_9),
        .mode = GPIO_MODE_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&io_config));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_9, 1));

    adc_channel_t channel;
    adc_unit_t adc_unit;
    ESP_ERROR_CHECK(
        adc_oneshot_io_to_channel(GPIO_NUM_10, &adc_unit, &channel));

    adc_oneshot_unit_init_cfg_t adc_init_config = {
        .unit_id = adc_unit,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(
        adc_oneshot_new_unit(&adc_init_config, &battery_adc_handle_));

    adc_oneshot_chan_cfg_t adc_chan_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(battery_adc_handle_, channel,
                                               &adc_chan_config));

    InitializeSpi();
    InitializeLcdDisplay();
    InitializeButtons();
    InitializeIot();
    if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
      GetBacklight()->RestoreBrightness();
    }
  }

  virtual Led* GetLed() override {
    static SingleLed led(BUILTIN_LED_GPIO);
    return &led;
  }

  virtual AudioCodec* GetAudioCodec() override {
    static Sph0645AudioCodec audio_codec(
        AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
        AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK,
        AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS,
        AUDIO_I2S_MIC_GPIO_DIN);
    return &audio_codec;
  }

  virtual Display* GetDisplay() override { return display_; }

  virtual Backlight* GetBacklight() override {
    if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
      static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN,
                                    DISPLAY_BACKLIGHT_OUTPUT_INVERT);
      return &backlight;
    }
    return nullptr;
  }

  bool GetBatteryLevel(int& level, bool& charging, bool& discharging) {
    constexpr uint32_t kMinAdcValue = 2048;
    constexpr uint32_t kMaxAdcValue = 2330;
    constexpr uint32_t kTotalBands = 10;
    constexpr uint32_t kAdcRangePerBand =
        (kMaxAdcValue - kMinAdcValue) / kTotalBands;
    constexpr uint32_t kHysteresisOffset = kAdcRangePerBand / 2;

    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_9, 0));
    int adc_value = 0;
    adc_channel_t channel;
    adc_unit_t adc_unit;
    ESP_ERROR_CHECK(
        adc_oneshot_io_to_channel(GPIO_NUM_10, &adc_unit, &channel));
    ESP_ERROR_CHECK(adc_oneshot_read(battery_adc_handle_, channel, &adc_value));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_9, 1));

    adc_value = std::clamp<int>(adc_value, kMinAdcValue, kMaxAdcValue);

    battery_adc_samples_.push_back(adc_value);
    if (battery_adc_samples_.size() > 10) {
      battery_adc_samples_.erase(battery_adc_samples_.begin());
    }

    int32_t sum = std::accumulate(battery_adc_samples_.begin(),
                                  battery_adc_samples_.end(), 0);
    adc_value = sum / battery_adc_samples_.size();

    if (current_band_ == UINT32_MAX) {
      // Initialize the current band based on the initial ADC value
      current_band_ = (adc_value - kMinAdcValue) / kAdcRangePerBand;
      if (current_band_ >= kTotalBands) {
        current_band_ = kTotalBands - 1;
      }
    } else {
      const int32_t lower_threshold =
          kMinAdcValue + current_band_ * kAdcRangePerBand - kHysteresisOffset;
      const int32_t upper_threshold = kMinAdcValue +
                                      (current_band_ + 1) * kAdcRangePerBand +
                                      kHysteresisOffset;

      if (adc_value < lower_threshold && current_band_ > 0) {
        --current_band_;
      } else if (adc_value > upper_threshold &&
                 current_band_ < kTotalBands - 1) {
        ++current_band_;
      }
    }

    level = current_band_ * 100 / (kTotalBands - 1);
    charging = false;
    discharging = true;

    return true;
  }
};

DECLARE_BOARD(NulllabAIVox);
