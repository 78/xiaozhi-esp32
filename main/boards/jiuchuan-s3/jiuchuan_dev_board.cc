#include "wifi_board.h"
#include "codecs/box_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "i2c_device.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <wifi_station.h>
#include "led/single_led.h"
#include "assets/lang_config.h"
#include "esp_lcd_panel_jd9853.h"
#include "esp_lcd_panel_gc9301.h"

#include "power_save_timer.h"
#include "power_manager.h"
#include <driver/rtc_io.h>
#include <driver/gpio.h>
#include <esp_sleep.h>
#include <esp_timer.h>

#define BOARD_TAG "JiuchuanDevBoard"

// 九川版AudioCodec：手动控制PA引脚（参考立创版）
class JiuchuanAudioCodec : public BoxAudioCodec {
private:
    gpio_num_t pa_pin_;
    bool pa_initialized_;

public:
    JiuchuanAudioCodec(i2c_master_bus_handle_t i2c_bus, 
                       int input_sample_rate, int output_sample_rate,
                       gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, 
                       gpio_num_t dout, gpio_num_t din,
                       gpio_num_t pa_pin, 
                       uint8_t es8311_addr, uint8_t es7210_addr, 
                       bool input_reference)
        : BoxAudioCodec(i2c_bus, input_sample_rate, output_sample_rate,
                       mclk, bclk, ws, dout, din, 
                       GPIO_NUM_NC,  // 不让ES8311驱动控制PA引脚
                       es8311_addr, es7210_addr, input_reference),
          pa_pin_(pa_pin),
          pa_initialized_(false) {
        
        ESP_LOGI(BOARD_TAG, "JiuchuanAudioCodec initialized (ES8311+ES7210)");
    }

    virtual void EnableOutput(bool enable) override {
        // 延迟初始化PA引脚（第一次调用EnableOutput时才初始化）
        if (!pa_initialized_ && pa_pin_ != GPIO_NUM_NC) {
            gpio_reset_pin(pa_pin_);  // 先复位，清除任何之前的配置
            
            gpio_config_t io_conf = {};
            io_conf.intr_type = GPIO_INTR_DISABLE;
            io_conf.mode = GPIO_MODE_OUTPUT;
            io_conf.pin_bit_mask = (1ULL << pa_pin_);
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
            gpio_config(&io_conf);
            
            pa_initialized_ = true;
            ESP_LOGI(BOARD_TAG, "PA pin GPIO%d initialized (lazy init)", pa_pin_);
        }
        
        BoxAudioCodec::EnableOutput(enable);
        
        // 控制PA引脚
        if (pa_pin_ != GPIO_NUM_NC) {
            gpio_set_level(pa_pin_, enable ? 1 : 0);
            ESP_LOGI(BOARD_TAG, "PA pin GPIO%d set to %d", pa_pin_, enable ? 1 : 0);
        }
    }
};

// 自定义LCD显示器类，用于圆形屏幕适配
class CustomLcdDisplay : public SpiLcdDisplay
{
public:
    CustomLcdDisplay(esp_lcd_panel_io_handle_t io_handle,
                     esp_lcd_panel_handle_t panel_handle,
                     int width,
                     int height,
                     int offset_x,
                     int offset_y,
                     bool mirror_x,
                     bool mirror_y,
                     bool swap_xy)
        : SpiLcdDisplay(io_handle, panel_handle, width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy)
    {

        DisplayLockGuard lock(this);
        lv_obj_set_style_pad_left(status_bar_, LV_HOR_RES * 0.167, 0);
        lv_obj_set_style_pad_right(status_bar_, LV_HOR_RES * 0.167, 0);
    }
};

class JiuchuanDevBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;
    Button boot_button_;
    Button pwr_button_;
    Button wifi_button;
    Button cmd_button;
    LcdDisplay* display_;
    PowerSaveTimer* power_save_timer_;
    PowerManager* power_manager_;
    esp_lcd_panel_io_handle_t panel_io = NULL;
    esp_lcd_panel_handle_t panel = NULL;

    // 音量映射函数：将内部音量(0-80)映射为显示音量(0-100%)
    int MapVolumeForDisplay(int internal_volume) {
        // 确保输入在有效范围内
        if (internal_volume < 0) internal_volume = 0;
        if (internal_volume > 80) internal_volume = 80;
        
        // 将0-80映射到0-100
        // 公式: 显示音量 = (内部音量 / 80) * 100
        return (internal_volume * 100) / 80;
    }
    
    void InitializePowerManager() {
        power_manager_ = new PowerManager(VBUS_ADC_GPIO);  // 使用VBUS检测引脚
        power_manager_->OnChargingStatusChanged([this](bool is_charging) {
            if (is_charging) {
                power_save_timer_->SetEnabled(false);
            } else {
                power_save_timer_->SetEnabled(true);
            }
        });
    }

    void InitializePowerSaveTimer() {
        // 一分钟进入浅睡眠，5分钟进入深睡眠关机
        power_save_timer_ = new PowerSaveTimer(-1, (60*5), -1);
        // power_save_timer_ = new PowerSaveTimer(-1, 6, 10);//test
        power_save_timer_->OnEnterSleepMode([this]() {
            GetDisplay()->SetPowerSaveMode(true);
            GetBacklight()->SetBrightness(1);
        });
        power_save_timer_->OnExitSleepMode([this]() {
            GetDisplay()->SetPowerSaveMode(false);
            GetBacklight()->RestoreBrightness();
        });
        power_save_timer_->OnShutdownRequest([this]() {
            ESP_LOGI(TAG, "Auto shutdown triggered by inactivity timer");
            // 使用 PowerManager 的统一关机逻辑
            // 会自动判断充电状态并执行相应的关机流程
            power_manager_->Shutdown();
        });
        power_save_timer_->SetEnabled(true);
    }

    void InitializeI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)1,
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

    void InitializeButtons() {
        // 启动保护：记录开机时间，5秒内禁用关机功能
        // 这样可以避免开机时的长按被误判为关机操作
        static int64_t boot_time_ms = esp_timer_get_time() / 1000;
        static const int64_t BOOT_PROTECTION_MS = 3000;  // 5秒保护期
        
        // 配置电源按钮 GPIO（不使用内部上下拉，依赖外部电路）
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << GPIO_NUM_3),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&io_conf));

        ESP_LOGI(TAG, "Power button initialized with %lld ms boot protection", BOOT_PROTECTION_MS);

        boot_button_.OnClick([this]() {
            power_save_timer_->WakeUp();
        });
        
        pwr_button_.OnLongPress([this]() {
            // 检查是否在启动保护期内
            int64_t current_time_ms = esp_timer_get_time() / 1000;
            int64_t elapsed_ms = current_time_ms - boot_time_ms;
            
            if (elapsed_ms < BOOT_PROTECTION_MS) {
                ESP_LOGW(TAG, "Shutdown blocked: within boot protection period (%lld/%lld ms)", 
                         elapsed_ms, BOOT_PROTECTION_MS);
                return;
            }
            
            // 防抖确认：连续检测5次
            for (int i = 0; i < 5; i++) {
                if (gpio_get_level(PWR_BUTTON_GPIO) == 0) {
                    ESP_LOGW(TAG, "Button released during confirmation, shutdown cancelled");
                    return;
                }
                vTaskDelay(100 / portTICK_PERIOD_MS);
            }
            
            ESP_LOGI(TAG, "Shutting down...");
            
            GetDisplay()->ShowNotification("松开按键以关机");

            // 关闭显示输出并让 LCD 控制器进入睡眠，彻底清除残影
            if (panel) {
                esp_err_t err = ESP_OK;
                err = esp_lcd_panel_disp_on_off(panel, false);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to turn off panel display: %s", esp_err_to_name(err));
                }
                err = esp_lcd_panel_disp_sleep(panel, true);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to enter panel sleep: %s", esp_err_to_name(err));
                }
            }
            
            power_manager_->Shutdown();
        });

        // 单击：切换聊天状态或唤醒设备
        pwr_button_.OnClick([this]() {
            auto &app = Application::GetInstance();
            auto current_state = app.GetDeviceState();
            
            if (current_state == kDeviceStateIdle || 
                current_state == kDeviceStateListening || 
                current_state == kDeviceStateSpeaking) {
                app.ToggleChatState();
            } else {
                power_save_timer_->WakeUp();
            }
        });

        // 双击：切换 AEC 打断模式
        pwr_button_.OnDoubleClick([this]() {
            power_save_timer_->WakeUp();
            auto& app = Application::GetInstance();
            
            // 空闲状态下切换 AEC 模式
            if (app.GetDeviceState() == kDeviceStateIdle) {
#if CONFIG_USE_DEVICE_AEC
                AecMode current_mode = app.GetAecMode();
                AecMode new_mode = (current_mode == kAecOff) ? kAecOnDeviceSide : kAecOff;
                app.SetAecMode(new_mode);
                ESP_LOGI(BOARD_TAG, "AEC mode: %s", new_mode == kAecOnDeviceSide ? "ON" : "OFF");
#endif
            }
        });

        // 电源键三击：重置WiFi
        pwr_button_.OnMultipleClick([this]()
                                    {
            ESP_LOGI(TAG, "Power button triple click: 重置WiFi");
            power_save_timer_->WakeUp();
            ResetWifiConfiguration(); }, 3);

        wifi_button.OnPressDown([this]()
                            {
           ESP_LOGI(TAG, "Volume up button pressed");
            power_save_timer_->WakeUp();

            auto codec = GetAudioCodec();
            int current_vol = codec->output_volume(); // 获取实际当前音量
            current_vol = (current_vol + 8 > 80) ? 80 : current_vol + 8;
            
            codec->SetOutputVolume(current_vol);

            ESP_LOGI(TAG, "Current volume: %d", current_vol);
            int display_volume = MapVolumeForDisplay(current_vol);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(display_volume) + "%");});

        cmd_button.OnPressDown([this]()
                           {
           ESP_LOGI(TAG, "Volume down button pressed");
            power_save_timer_->WakeUp();

            auto codec = GetAudioCodec();
            int current_vol = codec->output_volume(); // 获取实际当前音量
            current_vol = (current_vol - 8 < 0) ? 0 : current_vol - 8;
            
            codec->SetOutputVolume(current_vol);

            ESP_LOGI(TAG, "Current volume: %d", current_vol);
            if (current_vol == 0) {
                GetDisplay()->ShowNotification(Lang::Strings::MUTED);
            } else {
                int display_volume = MapVolumeForDisplay(current_vol);
                GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(display_volume) + "%");
            }});
    }

        void InitializeDisplay()
    {
        // 液晶屏控制IO初始化
        ESP_LOGI(TAG, "Install panel IO");
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_SPI_MOSI_PIN;
        buscfg.sclk_io_num = DISPLAY_SPI_SCK_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));

        // 初始化SPI总线
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_SPI_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = 3;
        io_config.pclk_hz = 80 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io);

        // 根据配置选择LCD驱动
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.bits_per_pixel = 16;

#if LCD_DRIVER_TYPE == 1
        // 使用 GC9301/GC9309NA 驱动
        ESP_LOGI(TAG, "Install LCD driver - GC9301/GC9309NA");
        panel_config.rgb_ele_order = LCD_RGB_ENDIAN_RGB;  // GC9301 使用 RGB 顺序
        esp_lcd_new_panel_gc9309na(panel_io, &panel_config, &panel);
        
#elif LCD_DRIVER_TYPE == 2
        // 使用 JD9853 驱动
        ESP_LOGI(TAG, "Install LCD driver - JD9853");
        panel_config.rgb_ele_order = LCD_RGB_ENDIAN_BGR;  // JD9853 使用 BGR 顺序
        esp_lcd_new_panel_jd9853(panel_io, &panel_config, &panel);
        
#else
        #error "LCD_DRIVER_TYPE must be 1 (GC9301) or 2 (JD9853). Auto-detect (0) not implemented yet."
#endif

        ESP_LOGI(TAG, "LCD driver loaded successfully");

        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, false);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        
        display_ = new CustomLcdDisplay(panel_io, panel,
                                        DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, 
                                        DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

public:
    JiuchuanDevBoard() :
        boot_button_(BOOT_BUTTON_GPIO),
        pwr_button_(PWR_BUTTON_GPIO,true),
        wifi_button(WIFI_BUTTON_GPIO),
        cmd_button(CMD_BUTTON_GPIO) {

        InitializeI2c();
        InitializePowerManager();
        InitializePowerSaveTimer();
        InitializeButtons();
        InitializeDisplay();
        GetBacklight()->RestoreBrightness();

    #if CONFIG_USE_DEVICE_AEC
        auto& app = Application::GetInstance();
        if (app.GetAecMode() != kAecOff) {
            // Ensure Jiuchuan boots with AEC disabled; user can re-enable via button
            app.SetAecMode(kAecOff, false);
        }
    #endif

    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        // 使用BoxAudioCodec：ES8311(DAC输出) + ES7210(ADC输入，4麦克风)
        static JiuchuanAudioCodec audio_codec(
            codec_i2c_bus_, 
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

    virtual Display* GetDisplay() override {
        return display_;
    }
    
    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        static bool last_discharging = false;
        charging = power_manager_->IsCharging();
        discharging = power_manager_->IsDischarging();
        if (discharging != last_discharging) {
            power_save_timer_->SetEnabled(discharging);
            last_discharging = discharging;
        }
        level = power_manager_->GetBatteryLevel();
        return true;
    }

    virtual void SetPowerSaveMode(bool enabled) override {
        if (!enabled) {
            power_save_timer_->WakeUp();
        }
        WifiBoard::SetPowerSaveMode(enabled);
    }
};

DECLARE_BOARD(JiuchuanDevBoard);
