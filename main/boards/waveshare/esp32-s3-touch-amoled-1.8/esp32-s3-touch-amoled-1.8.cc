#include <driver/i2c_master.h>
#include <driver/spi_master.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <cstdint>
#include "esp_lcd_sh8601.h"
#include "lcd_display.h"
#include "wifi_board.h"

#include "application.h"
#include "axp2101.h"
#include "button.h"
#include "codecs/es8311_audio_codec.h"
#include "config.h"
#include "led/single_led.h"

#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include "esp_io_expander_tca9554.h"
#include "mcp_server.h"
#include "power_save_timer.h"
#include "settings.h"

#include <esp_lcd_touch_ft5x06.h>
#include <esp_lvgl_port.h>
#include <lvgl.h>

#define TAG "WaveshareEsp32s3TouchAMOLED1inch8"

// 电源管理芯片类，继承自 Axp2101
class Pmic : public Axp2101 {
public:
    Pmic(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : Axp2101(i2c_bus, addr) {
        WriteReg(0x22, 0b110);  // 设置 PWRON > OFFLEVEL 作为关机源使能
        WriteReg(0x27, 0x10);   // 设置长按 4s 关机

        // 除了 DC1 外禁用所有 DC 电源
        WriteReg(0x80, 0x01);
        // 禁用所有 LDO
        WriteReg(0x90, 0x00);
        WriteReg(0x91, 0x00);

        // 设置 DC1 电位为 3.3V
        WriteReg(0x82, (3300 - 1500) / 100);

        // 设置 ALDO1 电位为 3.3V
        WriteReg(0x92, (3300 - 500) / 100);

        // 使能 ALDO1 (用于麦克风电源)
        WriteReg(0x90, 0x01);

        WriteReg(0x64, 0x02);  // 恒压充电电压设置为 4.1V

        WriteReg(0x61, 0x02);  // 设置主电池预充电电流为 50mA
        WriteReg(0x62, 0x08);  // 设置主电池充电电流为 400mA (0x08-200mA, 0x09-300mA, 0x0A-400mA)
        WriteReg(0x63, 0x01);  // 设置主电池截止充电电流为 25mA
    }
};

// LCD操作码定义
#define LCD_OPCODE_WRITE_CMD (0x02ULL)    // 写命令
#define LCD_OPCODE_READ_CMD (0x03ULL)     // 读命令
#define LCD_OPCODE_WRITE_COLOR (0x32ULL)  // 写颜色数据

static const sh8601_lcd_init_cmd_t vendor_specific_init[] = {
    {0x11, (uint8_t[]){0x00}, 0, 120},
    {0x44, (uint8_t[]){0x01, 0xD1}, 2, 0},
    {0x35, (uint8_t[]){0x00}, 1, 0},
    {0x53, (uint8_t[]){0x20}, 1, 10},
    {0x2A, (uint8_t[]){0x00, 0x00, 0x01, 0x6F}, 4, 0},
    {0x2B, (uint8_t[]){0x00, 0x00, 0x01, 0xBF}, 4, 0},
    {0x51, (uint8_t[]){0x00}, 1, 10},
    {0x29, (uint8_t[]){0x00}, 0, 10}};

// 在waveshare_amoled_1_8类之前添加新的显示类
// 自定义 LCD 显示类，继承自 SpiLcdDisplay
class CustomLcdDisplay : public SpiLcdDisplay {
public:
    CustomLcdDisplay(esp_lcd_panel_io_handle_t io_handle, esp_lcd_panel_handle_t panel_handle,
                     int width, int height, int offset_x, int offset_y, bool mirror_x,
                     bool mirror_y, bool swap_xy)
        : SpiLcdDisplay(io_handle, panel_handle, width, height, offset_x, offset_y, mirror_x,
                        mirror_y, swap_xy) {
        // 注意：UI 自定义应该在 SetupUI() 中进行，而不是构造函数中，
        // 以确保在访问 LVGL 对象之前它们已被正确创建。
    }

    virtual void SetupUI() override {
        // 先调用父类的 SetupUI() 来创建所有 LVGL 对象
        SpiLcdDisplay::SetupUI();

        DisplayLockGuard lock(this);
        // 自定义状态栏的内边距，使其在圆形或异形屏上居中更好看
        lv_obj_set_style_pad_left(status_bar_, LV_HOR_RES * 0.1, 0);
        lv_obj_set_style_pad_right(status_bar_, LV_HOR_RES * 0.1, 0);
    }
};

// 自定义背光控制类
class CustomBacklight : public Backlight {
public:
    CustomBacklight(esp_lcd_panel_io_handle_t panel_io, Display* display)
        : Backlight(), panel_io_(panel_io), display_(display) {}

protected:
    esp_lcd_panel_io_handle_t panel_io_;
    Display* display_;

    // 实现设置亮度的具体方法
    virtual void SetBrightnessImpl(uint8_t brightness) override {
        DisplayLockGuard lock(display_);
        uint8_t val = (uint8_t)((255 * brightness) / 100);
        uint8_t data[1] = {val};
        int lcd_cmd = 0x51;
        lcd_cmd &= 0xff;
        lcd_cmd <<= 8;
        lcd_cmd |= LCD_OPCODE_WRITE_CMD << 24;
        esp_lcd_panel_io_tx_param(panel_io_, lcd_cmd, &data, sizeof(data));
        ESP_LOGD("CustomBacklight", "SPI 发送亮度指令: 0x%02X, 值: %d (RAW: %d)", 0x51, brightness,
                 val);
    }

public:
    void ToggleBacklight() {
        // 如果当前是黑的（无论是 0 还是刚启动的初始 0），都执行亮屏
        if (brightness_ == 0) {
            ESP_LOGI("CustomBacklight", "检测到屏幕当前为关闭状态，正在淡入...");
            RestoreBrightness();
        } else {
            ESP_LOGI("CustomBacklight", "检测到屏幕当前为开启状态 (%d)，正在淡出...", brightness_);
            SetBrightness(0);
        }
    }
};

// Waveshare ESP32-S3 Touch AMOLED 1.8寸 开发板类
class WaveshareEsp32s3TouchAMOLED1inch8 : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;       // 用于音频编解码器和外设的 I2C 总线
    Pmic* pmic_ = nullptr;                        // 电源管理 IC
    Button boot_button_;                          // Boot 按钮
    CustomLcdDisplay* display_;                   // 液晶显示器
    CustomBacklight* backlight_;                  // 背光控制
    esp_io_expander_handle_t io_expander = NULL;  // IO 扩展芯片句柄
    PowerSaveTimer* power_save_timer_;            // 节能定时器

    // 初始化节能定时器
    void InitializePowerSaveTimer() {
        // 设置闲置时间，超时后进入睡眠模式或关机
        power_save_timer_ = new PowerSaveTimer(-1, 60, 300);
        power_save_timer_->OnEnterSleepMode([this]() {
            GetDisplay()->SetPowerSaveMode(true);
            GetBacklight()->SetBrightness(20);  // 睡眠时调低亮度
        });
        power_save_timer_->OnExitSleepMode([this]() {
            GetDisplay()->SetPowerSaveMode(false);
            GetBacklight()->RestoreBrightness();  // 唤醒时恢复亮度
        });
        power_save_timer_->OnShutdownRequest([this]() {
            pmic_->PowerOff();  // 关机请求时调用 PMIC 关机
        });
        power_save_timer_->SetEnabled(true);
    }

    // 初始化 I2C 总线
    void InitializeCodecI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags =
                {
                    .enable_internal_pullup = 1,
                },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
    }

    // 初始化 TCA9554 IO 扩展芯片
    void InitializeTca9554(void) {
        esp_err_t ret = esp_io_expander_new_i2c_tca9554(codec_i2c_bus_, I2C_ADDRESS, &io_expander);
        if (ret != ESP_OK)
            ESP_LOGE(TAG, "TCA9554 创建失败");
        // 设置方向
        ret = esp_io_expander_set_dir(
            io_expander, IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1 | IO_EXPANDER_PIN_NUM_2,
            IO_EXPANDER_OUTPUT);
        ret |= esp_io_expander_set_dir(io_expander, IO_EXPANDER_PIN_NUM_4, IO_EXPANDER_INPUT);
        ESP_ERROR_CHECK(ret);
        // 复位/控制电平抖动
        ret = esp_io_expander_set_level(
            io_expander, IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1 | IO_EXPANDER_PIN_NUM_2, 1);
        ESP_ERROR_CHECK(ret);
        vTaskDelay(pdMS_TO_TICKS(100));
        ret = esp_io_expander_set_level(
            io_expander, IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1 | IO_EXPANDER_PIN_NUM_2, 0);
        ESP_ERROR_CHECK(ret);
        vTaskDelay(pdMS_TO_TICKS(300));
        ret = esp_io_expander_set_level(
            io_expander, IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1 | IO_EXPANDER_PIN_NUM_2, 1);
        ESP_ERROR_CHECK(ret);
    }

    // 初始化 AXP2101 电源芯片
    void InitializeAxp2101() {
        ESP_LOGI(TAG, "初始化 AXP2101");
        pmic_ = new Pmic(codec_i2c_bus_, 0x34);
    }

    // 初始化 SPI 总线（用于 AMOLED 屏）
    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.sclk_io_num = GPIO_NUM_11;
        buscfg.data0_io_num = GPIO_NUM_4;
        buscfg.data1_io_num = GPIO_NUM_5;
        buscfg.data2_io_num = GPIO_NUM_6;
        buscfg.data3_io_num = GPIO_NUM_7;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        buscfg.flags = SPICOMMON_BUSFLAG_QUAD;  // 使用 QSPI 模式
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    // 初始化按钮
    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();  // 启动时按下进入 WiFi 配置模式
                return;
            }
            app.ToggleChatState();  // 运行时按下切换聊天状态
        });

        // 创建一个任务专门用于监测 TCA9554 的 EXIO4 按键状态（低频轮询）
        xTaskCreate(
            [](void* arg) {
                WaveshareEsp32s3TouchAMOLED1inch8* board =
                    static_cast<WaveshareEsp32s3TouchAMOLED1inch8*>(arg);

                vTaskDelay(pdMS_TO_TICKS(1000));  // 等待 1 秒确保系统和 IO 扩展芯片稳定

                uint32_t level = 1;
                if (board->io_expander) {
                    esp_io_expander_get_level(board->io_expander, IO_EXPANDER_PIN_NUM_4, &level);
                }
                bool last_pressed = (level == 0);
                ESP_LOGI(TAG, "EXIO4 按键监测任务就绪，初始电平: %ld, 初始按下: %d", level,
                         last_pressed);

                while (true) {
                    if (board->io_expander) {
                        esp_io_expander_get_level(board->io_expander, IO_EXPANDER_PIN_NUM_4,
                                                  &level);
                        bool pressed = (level == 0);
                        if (pressed && !last_pressed) {
                            ESP_LOGI(TAG, "检测到物理按键 EXIO4 (PWR) 下降沿触发");
                            if (board->backlight_) {
                                board->backlight_->ToggleBacklight();
                            }
                        }
                        last_pressed = pressed;
                    }
                    vTaskDelay(pdMS_TO_TICKS(50));
                }
            },
            "pek_monitor_task", 4096, this, 2, nullptr);
    }

    // 初始化 SH8601 液晶控制器
    void InitializeSH8601Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        // 液晶屏控制 IO 初始化 (QSPI)
        ESP_LOGD(TAG, "安装面板 IO");
        esp_lcd_panel_io_spi_config_t io_config =
            SH8601_PANEL_IO_QSPI_CONFIG(EXAMPLE_PIN_NUM_LCD_CS, nullptr, nullptr);
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片
        ESP_LOGD(TAG, "安装 LCD 驱动");
        const sh8601_vendor_config_t vendor_config = {
            .init_cmds = &vendor_specific_init[0],
            .init_cmds_size = sizeof(vendor_specific_init) / sizeof(sh8601_lcd_init_cmd_t),
            .flags = {
                .use_qspi_interface = 1,
            }};

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.flags.reset_active_high = 1,
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        panel_config.vendor_config = (void*)&vendor_config;
        ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(panel_io, &panel_config, &panel));

        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, false);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        esp_lcd_panel_disp_on_off(panel, true);
        display_ = new CustomLcdDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                        DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X,
                                        DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
        backlight_ = new CustomBacklight(panel_io, display_);
        backlight_->RestoreBrightness();
    }

    // 初始化触摸屏
    void InitializeTouch() {
        esp_lcd_touch_handle_t tp;
        esp_lcd_touch_config_t tp_cfg = {
            .x_max = DISPLAY_WIDTH,
            .y_max = DISPLAY_HEIGHT,
            .rst_gpio_num = GPIO_NUM_NC,
            .int_gpio_num = GPIO_NUM_21,
            .levels =
                {
                    .reset = 0,
                    .interrupt = 0,
                },
            .flags =
                {
                    .swap_xy = 0,
                    .mirror_x = 0,
                    .mirror_y = 0,
                },
        };
        esp_lcd_panel_io_handle_t tp_io_handle = NULL;
        esp_lcd_panel_io_i2c_config_t tp_io_config = {
            .dev_addr = ESP_LCD_TOUCH_IO_I2C_FT5x06_ADDRESS,
            .control_phase_bytes = 1,
            .dc_bit_offset = 0,
            .lcd_cmd_bits = 8,
            .flags =
            {
                .disable_control_phase = 1,
            }
        };
        tp_io_config.scl_speed_hz = 400 * 1000;
        // 触摸屏使用与编解码器相同的 I2C 总线
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(codec_i2c_bus_, &tp_io_config, &tp_io_handle));
        ESP_LOGI(TAG, "初始化触摸控制器");
        ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_ft5x06(tp_io_handle, &tp_cfg, &tp));
        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = lv_display_get_default(),
            .handle = tp,
        };
        lvgl_port_add_touch(&touch_cfg);  // 将触摸屏集成到 LVGL
        ESP_LOGI(TAG, "触摸面板初始化成功");
    }

    // 初始化 MCP 相关的控制工具
    void InitializeTools() {
        auto& mcp_server = McpServer::GetInstance();
        mcp_server.AddTool("self.system.reconfigure_wifi",
                           "End this conversation and enter WiFi configuration mode.\n"
                           "**CAUTION** You must ask the user to confirm this action.",
                           PropertyList(), [this](const PropertyList& properties) {
                               EnterWifiConfigMode();
                               return true;
                           });
    }

public:
    WaveshareEsp32s3TouchAMOLED1inch8() : boot_button_(BOOT_BUTTON_GPIO) {
        ESP_LOGI(TAG, "--- 正在引导 Waveshare AMOLED 板载驱动 ---");
        InitializePowerSaveTimer();
        InitializeCodecI2c();
        InitializeTca9554();
        InitializeAxp2101();
        InitializeSpi();
        InitializeSH8601Display();
        InitializeTouch();
        InitializeButtons();
        InitializeTools();
        ESP_LOGI(TAG, "--- 开发板自适应逻辑引导完成 ---");
    }

    // 获取音频编解码器实例
    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(
            codec_i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN, AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override { return display_; }

    virtual Backlight* GetBacklight() override { return backlight_; }

    // 获取电池状态信息
    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        static bool last_discharging = false;
        charging = pmic_->IsCharging();
        discharging = pmic_->IsDischarging();
        // 如果放电状态发生变化，更新节能定时器状态
        if (discharging != last_discharging) {
            power_save_timer_->SetEnabled(discharging);
            last_discharging = discharging;
        }

        level = pmic_->GetBatteryLevel();
        return true;
    }

    // 设置节能级别
    virtual void SetPowerSaveLevel(PowerSaveLevel level) override {
        if (level != PowerSaveLevel::LOW_POWER) {
            power_save_timer_->WakeUp();  // 如果不是低功耗模式，则唤醒系统
        }
        WifiBoard::SetPowerSaveLevel(level);
    }
};

DECLARE_BOARD(WaveshareEsp32s3TouchAMOLED1inch8);
