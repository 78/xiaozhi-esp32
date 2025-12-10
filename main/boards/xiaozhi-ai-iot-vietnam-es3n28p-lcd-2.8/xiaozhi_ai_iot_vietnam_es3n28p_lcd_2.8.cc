#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#ifdef CONFIG_TOUCH_PANEL_ENABLE
#include <esp_lcd_touch.h>
#include <esp_lcd_touch_ft5x06.h>
#include <esp_lcd_touch_cst816s.h>
#endif
#include <lvgl.h>
#include <esp_lvgl_port.h>
#include <wifi_station.h>
#include "application.h"
#include "codecs/no_audio_codec.h"
#include "codecs/es8311_audio_codec.h"
#include "button.h"
#include "display/lcd_display.h"
#include "led/single_led.h"
#include "system_reset.h"
#include "wifi_board.h"
#include "mcp_server.h"
#include "lamp_controller.h"
#include "config.h"
#include "esp_lcd_ili9341.h"
#ifdef CONFIG_SD_CARD_MMC_INTERFACE
#include "sdmmc.h"
#elif defined(CONFIG_SD_CARD_SPI_INTERFACE)
#include "sdspi.h"
#endif

#define TAG "XiaozhiAIIoTEs3n28p"

// Global variables for touch callback
static i2c_master_bus_handle_t g_touch_i2c_bus = NULL;
static lv_display_t* g_lvgl_display = NULL;

class XiaozhiAIIoTEs3n28p : public WifiBoard {
 private:
  Button boot_button_;
  LcdDisplay *display_;
  i2c_master_bus_handle_t codec_i2c_bus_;
  esp_lcd_touch_handle_t tp_ = NULL;

  void InitializeSpi() {
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
    buscfg.miso_io_num = DISPLAY_MIS0_PIN;
    buscfg.sclk_io_num = DISPLAY_SCK_PIN;
    buscfg.quadwp_io_num = GPIO_NUM_NC;
    buscfg.quadhd_io_num = GPIO_NUM_NC;
    buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
  }

  void InitializeLcdDisplay() {
    esp_lcd_panel_io_handle_t panel_io = nullptr;
    esp_lcd_panel_handle_t panel = nullptr;
    // Initialize LCD control IO
    ESP_LOGD(TAG, "Install panel IO");
    esp_lcd_panel_io_spi_config_t io_config = {};
    io_config.cs_gpio_num = DISPLAY_CS_PIN;
    io_config.dc_gpio_num = DISPLAY_DC_PIN;
    io_config.spi_mode = DISPLAY_SPI_MODE;
    io_config.pclk_hz = DISPLAY_SPI_SCLK_HZ;
    io_config.trans_queue_depth = 10;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(LCD_SPI_HOST, &io_config, &panel_io));
    
    // Initialize the LCD driver chip
    ESP_LOGD(TAG, "Install LCD driver");
    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = DISPLAY_RST_PIN;
    panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
    panel_config.bits_per_pixel = 16;
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_config, &panel));
    ESP_LOGI(TAG, "Install LCD driver ILI9341");
    esp_lcd_panel_reset(panel);

    esp_lcd_panel_init(panel);
    esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
    esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
    esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
    display_ = new SpiLcdDisplay(panel_io, panel, 
        DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, 
        DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
  }

  void InitializeI2c() {
      i2c_master_bus_config_t i2c_bus_cfg = {
          .i2c_port = AUDIO_CODEC_I2C_NUM,
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
      ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
  }

  void CheckI2CDevice(uint8_t addr, const char* name) {
    i2c_master_dev_handle_t dev_handle;
    i2c_device_config_t dev_cfg = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = addr,
      .scl_speed_hz = 100000,
    };
    
    if (i2c_master_bus_add_device(codec_i2c_bus_, &dev_cfg, &dev_handle) == ESP_OK) {
      uint8_t data;
      if (i2c_master_receive(dev_handle, &data, 1, 100) == ESP_OK) {
        ESP_LOGI(TAG, "✓ Found %s at I2C address 0x%02X", name, addr);
      } else {
        ESP_LOGW(TAG, "✗ Device at 0x%02X (%s) not responding", addr, name);
      }
      i2c_master_bus_rm_device(dev_handle);
    } else {
      ESP_LOGE(TAG, "✗ Failed to add device at 0x%02X (%s)", addr, name);
    }
  }

#ifdef CONFIG_TOUCH_PANEL_ENABLE
  static void touch_event_callback(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    ESP_LOGI(TAG, "📱 Touch event: %d", code);  // Debug all events
    
    if (code == LV_EVENT_PRESSED) {
      lv_point_t point;
      lv_indev_get_point(lv_indev_get_act(), &point);
      ESP_LOGI(TAG, "🖐️ Touch PRESSED at (%d, %d)", point.x, point.y);
    } else if (code == LV_EVENT_RELEASED) {
      ESP_LOGI(TAG, "🖐️ Touch RELEASED");
    } else if (code == LV_EVENT_CLICKED) {
      lv_point_t point;
      lv_indev_get_point(lv_indev_get_act(), &point);
      ESP_LOGI(TAG, "🖐️ Touch CLICKED at (%d, %d) - Toggling chat!", point.x, point.y);
      
      // Toggle chat state like boot button
      auto &app = Application::GetInstance();
      app.ToggleChatState();
    } else if (code == LV_EVENT_PRESSING) {
      // Ignore pressing events (too many)
    } else {
      ESP_LOGI(TAG, "📱 Other touch event: %d", code);
    }
  }

  void InitializeTouch() {
    ESP_LOGI(TAG, "Initialize touch controller FT6236G");
    ESP_LOGI(TAG, "Touch I2C: SDA=%d, SCL=%d, ADDR=0x%02X", TOUCH_I2C_SDA_PIN, TOUCH_I2C_SCL_PIN, TOUCH_I2C_ADDR);
    ESP_LOGI(TAG, "Touch pins: RST=%d, INT=%d", TOUCH_RST_PIN, TOUCH_INT_PIN);
    
    // Manual reset of touch controller
    ESP_LOGI(TAG, "Resetting touch controller...");
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << TOUCH_RST_PIN);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    
    gpio_set_level(TOUCH_RST_PIN, 0);  // Reset low
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(TOUCH_RST_PIN, 1);  // Reset high
    vTaskDelay(pdMS_TO_TICKS(200));     // Wait for touch controller to boot
    ESP_LOGI(TAG, "Touch controller reset complete");
    
    // Check I2C devices
    CheckI2CDevice(0x18, "ES8311 Audio Codec");
    CheckI2CDevice(0x38, "FT6236G Touch");
    
    // Touch configuration - disable interrupt, use polling mode
    esp_lcd_touch_config_t tp_cfg = {
      .x_max = DISPLAY_WIDTH - 1,
      .y_max = DISPLAY_HEIGHT - 1,
      .rst_gpio_num = TOUCH_RST_PIN,
      .int_gpio_num = GPIO_NUM_NC,  // Disable interrupt, use polling
      .levels = {
        .reset = 0,
        .interrupt = 0,
      },
      .flags = {
        .swap_xy = DISPLAY_SWAP_XY ? 1U : 0U,
        .mirror_x = DISPLAY_MIRROR_X ? 1U : 0U,
        .mirror_y = DISPLAY_MIRROR_Y ? 1U : 0U,
      },
    };
    
    ESP_LOGI(TAG, "Using polling mode (interrupt disabled) for touch detection");
    
    ESP_LOGI(TAG, "Touch config: x_max=%d, y_max=%d, swap_xy=%d, mirror_x=%d, mirror_y=%d",
             tp_cfg.x_max, tp_cfg.y_max, tp_cfg.flags.swap_xy, 
             tp_cfg.flags.mirror_x, tp_cfg.flags.mirror_y);
    
    // Read FT6336G chip ID to verify communication
    ESP_LOGI(TAG, "Reading FT6336G chip ID...");
    i2c_master_dev_handle_t touch_dev;
    i2c_device_config_t touch_dev_cfg = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = TOUCH_I2C_ADDR,
      .scl_speed_hz = 400000,
    };
    
    esp_err_t ret = i2c_master_bus_add_device(codec_i2c_bus_, &touch_dev_cfg, &touch_dev);
    if (ret == ESP_OK) {
      uint8_t chip_id_reg = 0xA3;  // FT6336 Chip ID register
      uint8_t chip_id = 0;
      uint8_t tx_buf[1] = {chip_id_reg};
      
      ret = i2c_master_transmit_receive(touch_dev, tx_buf, 1, &chip_id, 1, 1000);
      if (ret == ESP_OK) {
        ESP_LOGI(TAG, "FT6336G Chip ID: 0x%02X (expected 0x64 or 0x36)", chip_id);
      } else {
        ESP_LOGW(TAG, "Failed to read chip ID: %s", esp_err_to_name(ret));
      }
      
      // Read firmware version
      uint8_t fw_ver_reg = 0xA6;
      uint8_t fw_ver = 0;
      tx_buf[0] = fw_ver_reg;
      ret = i2c_master_transmit_receive(touch_dev, tx_buf, 1, &fw_ver, 1, 1000);
      if (ret == ESP_OK) {
        ESP_LOGI(TAG, "FT6336G Firmware version: 0x%02X", fw_ver);
      }
      
      // Configure FT6336G registers
      ESP_LOGI(TAG, "Configuring FT6336G registers...");
      
      // Set to normal operating mode
      uint8_t mode_reg[] = {0x00, 0x00};  // Device Mode = Normal
      i2c_master_transmit(touch_dev, mode_reg, 2, 1000);
      
      // Set touch threshold
      uint8_t threshold_reg[] = {0x80, 0x40};  // Threshold = 64
      i2c_master_transmit(touch_dev, threshold_reg, 2, 1000);
      
      // Set report rate
      uint8_t rate_reg[] = {0x88, 0x0A};  // Report rate = 10Hz
      i2c_master_transmit(touch_dev, rate_reg, 2, 1000);
      
      // Enable interrupt mode
      uint8_t int_mode_reg[] = {0xA4, 0x01};  // Interrupt mode = trigger
      i2c_master_transmit(touch_dev, int_mode_reg, 2, 1000);
      
      ESP_LOGI(TAG, "FT6336G configuration complete");
      
      i2c_master_bus_rm_device(touch_dev);
    }
    
    // Use FT5x06 driver (compatible with FT6336)
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
    tp_io_config.dev_addr = TOUCH_I2C_ADDR;
    tp_io_config.scl_speed_hz = 400 * 1000;  // 400kHz
    
    ESP_LOGI(TAG, "Creating touch I2C panel IO...");
    ret = esp_lcd_new_panel_io_i2c(codec_i2c_bus_, &tp_io_config, &tp_io_handle);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to create touch I2C panel IO: %s", esp_err_to_name(ret));
      return;
    }
    
    ESP_LOGI(TAG, "Creating FT5x06 touch controller (compatible with FT6336)...");
    ret = esp_lcd_touch_new_i2c_ft5x06(tp_io_handle, &tp_cfg, &tp_);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to create FT5x06 touch controller: %s", esp_err_to_name(ret));
      return;
    }
    
    // Store I2C bus handle for touch callback
    g_touch_i2c_bus = codec_i2c_bus_;
    
    // Use custom touch driver instead of FT5x06
    ESP_LOGI(TAG, "Adding custom touch driver to LVGL...");
    lv_indev_t *touch_indev = lv_indev_create();
    lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch_indev, custom_touch_read_cb);
    
    ESP_LOGI(TAG, "Touch input device created, LVGL will start polling...");
    
    // Store LVGL display handle for rotation
    g_lvgl_display = lv_display_get_default();
    if (g_lvgl_display != NULL) {
      ESP_LOGI(TAG, "LVGL display handle stored for rotation control");
    }
    
    // Add touch event callback to screen
    lv_obj_t *screen = lv_scr_act();
    lv_obj_add_event_cb(screen, touch_event_callback, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(screen, touch_event_callback, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(screen, touch_event_callback, LV_EVENT_CLICKED, NULL);
    
    ESP_LOGI(TAG, "✅ Touch panel FT6236G initialized successfully with custom driver!");
    ESP_LOGI(TAG, "Touch screen is ready - try touching now...");
  }
  
  // Custom touch read callback for LVGL
  static void custom_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    static i2c_master_dev_handle_t touch_dev = NULL;
    static bool init_done = false;
    static bool was_pressed = false;
    static int callback_counter = 0;
    
    // Swipe detection variables
    static int16_t touch_start_x = 0;
    static int16_t touch_start_y = 0;
    static int64_t touch_start_time = 0;
    static bool is_swiping = false;
    static int current_brightness = 100;  // Track brightness (initialized to 100)
    
    // Double-tap detection variables
    static int64_t last_tap_time = 0;
    static const int64_t DOUBLE_TAP_WINDOW = 500000;  // 500ms window for double-tap
    static int rotation_state = 0;  // 0=0°, 1=90°, 2=180°, 3=270°
    
    // Two-finger detection variables
    static bool two_finger_detected = false;
    static int64_t two_finger_start_time = 0;
    
    // Callback counter (silent - only for debugging if needed)
    callback_counter++;
    
    // Initialize on first call
    if (!init_done && g_touch_i2c_bus != NULL) {
      i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = TOUCH_I2C_ADDR,
        .scl_speed_hz = 400000,
      };
      if (i2c_master_bus_add_device(g_touch_i2c_bus, &dev_cfg, &touch_dev) == ESP_OK) {
        ESP_LOGI(TAG, "Touch callback initialized");
        init_done = true;
      } else {
        ESP_LOGE(TAG, "Failed to init touch in callback");
      }
    }
    
    // Default to released
    data->state = LV_INDEV_STATE_RELEASED;
    
    if (touch_dev == NULL) {
      return;
    }
    
    // Read touch data
    uint8_t touch_data[16];
    uint8_t reg_addr = 0x02;
    bool is_pressed = false;
    int16_t current_x = 0;
    int16_t current_y = 0;
    
    if (i2c_master_transmit_receive(touch_dev, &reg_addr, 1, touch_data, 16, 100) == ESP_OK) {
      uint8_t touch_points = touch_data[0] & 0x0F;
      
      // Detect two-finger touch
      if (touch_points == 2) {
        if (!two_finger_detected) {
          two_finger_detected = true;
          two_finger_start_time = esp_timer_get_time();
          is_swiping = true;  // Prevent other gestures
          ESP_LOGI(TAG, "✌️ Two-finger touch detected!");
        }
        
        // Use first touch point for LVGL
        uint16_t x = ((touch_data[1] & 0x0F) << 8) | touch_data[2];
        uint16_t y = ((touch_data[3] & 0x0F) << 8) | touch_data[4];
        
        if (DISPLAY_SWAP_XY) {
          uint16_t temp = x;
          x = y;
          y = temp;
        }
        
        current_x = x;
        current_y = y;
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
        is_pressed = true;
      }
      else if (touch_points == 1) {
        uint16_t x = ((touch_data[1] & 0x0F) << 8) | touch_data[2];
        uint16_t y = ((touch_data[3] & 0x0F) << 8) | touch_data[4];
        
        // Apply swap_xy if needed (display is 320x240 with swap_xy=1)
        if (DISPLAY_SWAP_XY) {
          uint16_t temp = x;
          x = y;
          y = temp;
        }
        
        current_x = x;
        current_y = y;
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
        is_pressed = true;
        
        // Record touch start position
        if (!was_pressed) {
          touch_start_x = x;
          touch_start_y = y;
          touch_start_time = esp_timer_get_time();
          is_swiping = false;
        }
        
        // Touch coordinates detected (silent mode)
      }
    }
    
    // Detect swipe gesture while touching (only for single finger)
    if (was_pressed && is_pressed && !is_swiping && !two_finger_detected) {
      int16_t dx = current_x - touch_start_x;
      int16_t dy = current_y - touch_start_y;
      int64_t touch_duration = esp_timer_get_time() - touch_start_time;
      
      auto& board = Board::GetInstance();
      
      // Detect horizontal swipe for volume (threshold: 50 pixels, within 1 second)
      if (abs(dx) > 50 && abs(dx) > abs(dy) * 1.5 && touch_duration < 1000000) {
        is_swiping = true;
        auto codec = board.GetAudioCodec();
        int current_volume = codec->output_volume();
        int new_volume = current_volume;
        
        if (dx > 0) {
          // Swipe right - increase volume
          new_volume = current_volume + 10;
          if (new_volume > 100) new_volume = 100;
          ESP_LOGI(TAG, "👉 Swipe RIGHT - Volume: %d → %d", current_volume, new_volume);
        } else {
          // Swipe left - decrease volume
          new_volume = current_volume - 10;
          if (new_volume < 0) new_volume = 0;
          ESP_LOGI(TAG, "👈 Swipe LEFT - Volume: %d → %d", current_volume, new_volume);
        }
        
        codec->SetOutputVolume(new_volume);
        auto display = board.GetDisplay();
        display->ShowNotification("Volume: " + std::to_string(new_volume));
      }
      // Detect vertical swipe for brightness (threshold: 50 pixels, within 1 second)
      else if (abs(dy) > 50 && abs(dy) > abs(dx) * 1.5 && touch_duration < 1000000) {
        is_swiping = true;
        auto backlight = board.GetBacklight();
        int new_brightness = current_brightness;
        
        if (dy < 0) {
          // Swipe up - increase brightness
          new_brightness = current_brightness + 10;
          if (new_brightness > 100) new_brightness = 100;
          ESP_LOGI(TAG, "👆 Swipe UP - Brightness: %d → %d", current_brightness, new_brightness);
        } else {
          // Swipe down - decrease brightness
          new_brightness = current_brightness - 10;
          if (new_brightness < 10) new_brightness = 10;  // Min 10% to keep visible
          ESP_LOGI(TAG, "👇 Swipe DOWN - Brightness: %d → %d", current_brightness, new_brightness);
        }
        
        backlight->SetBrightness(new_brightness);
        current_brightness = new_brightness;  // Update tracked value
        auto display = board.GetDisplay();
        display->ShowNotification("Brightness: " + std::to_string(new_brightness));
      }
    }
    
    // Detect touch release (click complete)
    if (was_pressed && !is_pressed) {
      // Check if two-finger gesture was completed
      if (two_finger_detected) {
        int64_t two_finger_duration = esp_timer_get_time() - two_finger_start_time;
        
        // If two fingers held for at least 200ms, rotate display
        if (two_finger_duration > 200000) {
          ESP_LOGI(TAG, "✌️ Two-finger tap completed - Rotating display!");
          
          if (g_lvgl_display != NULL) {
            rotation_state = (rotation_state + 1) % 4;
            
            switch (rotation_state) {
              case 0:
                lv_display_set_rotation(g_lvgl_display, LV_DISPLAY_ROTATION_0);
                ESP_LOGI(TAG, "🔄 Display rotation: 0° (320x240)");
                break;
              case 1:
                lv_display_set_rotation(g_lvgl_display, LV_DISPLAY_ROTATION_90);
                ESP_LOGI(TAG, "🔄 Display rotation: 90° (240x320)");
                break;
              case 2:
                lv_display_set_rotation(g_lvgl_display, LV_DISPLAY_ROTATION_180);
                ESP_LOGI(TAG, "🔄 Display rotation: 180° (320x240)");
                break;
              case 3:
                lv_display_set_rotation(g_lvgl_display, LV_DISPLAY_ROTATION_270);
                ESP_LOGI(TAG, "🔄 Display rotation: 270° (240x320)");
                break;
            }
            
            // Force LVGL to refresh and recalculate layout
            lv_obj_invalidate(lv_scr_act());
            lv_refr_now(g_lvgl_display);
            
            auto& board = Board::GetInstance();
            auto display = board.GetDisplay();
            display->ShowNotification("Rotation: " + std::to_string(rotation_state * 90) + "°");
          }
        }
        
        two_finger_detected = false;
        is_swiping = false;
      }
      // Only trigger click if no swipe was detected
      else if (!is_swiping) {
        int64_t current_time = esp_timer_get_time();
        int64_t time_since_last_tap = current_time - last_tap_time;
        
        // Check for double-tap
        if (time_since_last_tap < DOUBLE_TAP_WINDOW && last_tap_time > 0) {
          // Double-tap detected - rotate display 90°
          ESP_LOGI(TAG, "👆👆 Double-TAP detected - Rotating display 90°!");
#if (0)
          if (g_lvgl_display != NULL) {
            rotation_state = (rotation_state + 1) % 4;  // Cycle 0->1->2->3->0
            
            switch (rotation_state) {
              case 0:
                lv_display_set_rotation(g_lvgl_display, LV_DISPLAY_ROTATION_0);
                ESP_LOGI(TAG, "🔄 Display rotation: 0° (320x240)");
                break;
              case 1:
                lv_display_set_rotation(g_lvgl_display, LV_DISPLAY_ROTATION_90);
                ESP_LOGI(TAG, "🔄 Display rotation: 90° (240x320)");
                break;
              case 2:
                lv_display_set_rotation(g_lvgl_display, LV_DISPLAY_ROTATION_180);
                ESP_LOGI(TAG, "🔄 Display rotation: 180° (320x240)");
                break;
              case 3:
                lv_display_set_rotation(g_lvgl_display, LV_DISPLAY_ROTATION_270);
                ESP_LOGI(TAG, "🔄 Display rotation: 270° (240x320)");
                break;
            }
            
            // Force LVGL to refresh and recalculate layout
            lv_obj_invalidate(lv_scr_act());
            lv_refr_now(g_lvgl_display);
            
            auto& board = Board::GetInstance();
            auto display = board.GetDisplay();
            display->ShowNotification("Rotation: " + std::to_string(rotation_state * 90) + "°");
          }
#endif
          last_tap_time = 0;  // Reset to prevent triple-tap
        } else {
          // Single tap - toggle chat state
          ESP_LOGI(TAG, "🖐️ Touch TAP detected at (%d, %d) - Toggling chat!", touch_start_x, touch_start_y);
          auto &app = Application::GetInstance();
          app.ToggleChatState();
          
          last_tap_time = current_time;  // Record for double-tap detection
        }
      } 
      else {
        ESP_LOGI(TAG, "Swipe completed, no tap action");
      }
      is_swiping = false;
    }
    
    was_pressed = is_pressed;
  }
#endif

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

  void InitializeTools() {
    static LampController lamp(BUILTIN_LED_GPIO);
  }

 public:
  XiaozhiAIIoTEs3n28p(): boot_button_(BOOT_BUTTON_GPIO)
  {
    InitializeI2c();
    InitializeSpi();
    InitializeLcdDisplay();
#ifdef CONFIG_TOUCH_PANEL_ENABLE
    InitializeTouch();
#endif
    InitializeButtons();
    InitializeTools();
    if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
      GetBacklight()->RestoreBrightness();
    }
  }

  virtual Led *GetLed() override {
    static SingleLed led(BUILTIN_LED_GPIO);
    return &led;
  }

  virtual AudioCodec* GetAudioCodec() override {
    static Es8311AudioCodec audio_codec(codec_i2c_bus_, AUDIO_CODEC_I2C_NUM,
      AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE, AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK,
      AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN, AUDIO_CODEC_PA_PIN,
      AUDIO_CODEC_ES8311_ADDR, true, true);
    return &audio_codec;
  }

  virtual Display *GetDisplay() override { return display_; }

  virtual Backlight *GetBacklight() override {
    static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN,
                                  DISPLAY_BACKLIGHT_OUTPUT_INVERT);
    return &backlight;
  }

#ifdef CONFIG_SD_CARD_MMC_INTERFACE
    virtual SdCard* GetSdCard() override {
#ifdef CARD_SDMMC_BUS_WIDTH_4BIT
        static SdMMC sdmmc(CARD_SDMMC_CLK_GPIO,
                           CARD_SDMMC_CMD_GPIO,
                           CARD_SDMMC_D0_GPIO,
                           CARD_SDMMC_D1_GPIO,
                           CARD_SDMMC_D2_GPIO,
                           CARD_SDMMC_D3_GPIO);
#else
        static SdMMC sdmmc(CARD_SDMMC_CLK_GPIO,
                           CARD_SDMMC_CMD_GPIO,
                           CARD_SDMMC_D0_GPIO);
#endif
        return &sdmmc;
    }
#endif
#ifdef CONFIG_SD_CARD_SPI_INTERFACE
    virtual SdCard* GetSdCard() override {
        static SdSPI sdspi(CARD_SPI_MISO_GPIO,
                           CARD_SPI_MOSI_GPIO,
                           CARD_SPI_SCLK_GPIO,
                           CARD_SPI_CS_GPIO);
        return &sdspi;
    }
#endif

};

DECLARE_BOARD(XiaozhiAIIoTEs3n28p);