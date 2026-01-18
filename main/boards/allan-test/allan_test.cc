#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "display/lcd_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "lamp_controller.h"
#include "led/single_led.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <driver/spi_common.h>

#define TAG "CompactBoardTest"

class CompactBoardTest : public WifiBoard {
private:
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    Button boot_button_;
    LcdDisplay* display_;

    void InitializeSpi() {
        ESP_LOGI(TAG, "初始化SPI总线");
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_CLK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
        ESP_LOGI(TAG, "SPI总线初始化成功");
    }

    void InitializeSt7789Display() {
        ESP_LOGI(TAG, "开始初始化ST7789显示屏");
        ESP_LOGI(TAG, "引脚配置: MOSI=%d, CLK=%d, DC=%d, RST=%d, BLK=%d", 
            DISPLAY_MOSI_PIN, DISPLAY_CLK_PIN, DISPLAY_DC_PIN, DISPLAY_RST_PIN, DISPLAY_BACKLIGHT_PIN);
        ESP_LOGI(TAG, "SPI模式: %d, 时钟频率: %d Hz", DISPLAY_SPI_MODE, DISPLAY_SPI_FREQUENCY);
        ESP_LOGI(TAG, "显示配置: 反转颜色=%s, 镜像X=%s, 镜像Y=%s, 交换XY=%s", 
            DISPLAY_INVERT_COLOR ? "是" : "否",
            DISPLAY_MIRROR_X ? "是" : "否", 
            DISPLAY_MIRROR_Y ? "是" : "否",
            DISPLAY_SWAP_XY ? "是" : "否");
        ESP_LOGI(TAG, "背光极性: %s", DISPLAY_BACKLIGHT_OUTPUT_INVERT ? "反向" : "正常");
        
        ESP_LOGI(TAG, "安装面板IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = DISPLAY_SPI_FREQUENCY;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        
        esp_err_t ret = esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &panel_io_);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "面板IO初始化失败: %s", esp_err_to_name(ret));
            return;
        }
        ESP_LOGI(TAG, "面板IO初始化成功");
        
        ESP_LOGI(TAG, "安装LCD驱动");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        
        ret = esp_lcd_new_panel_st7789(panel_io_, &panel_config, &panel_);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "LCD驱动安装失败: %s", esp_err_to_name(ret));
            return;
        }
        ESP_LOGI(TAG, "LCD驱动安装成功");
        
        // 执行硬件复位
        ESP_LOGI(TAG, "执行硬件复位");
        ret = esp_lcd_panel_reset(panel_);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "硬件复位失败: %s", esp_err_to_name(ret));
            return;
        }
        
        // 初始化面板
        ESP_LOGI(TAG, "初始化面板");
        ret = esp_lcd_panel_init(panel_);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "面板初始化失败: %s", esp_err_to_name(ret));
            return;
        }
        ESP_LOGI(TAG, "面板初始化成功");
        
        // 设置显示参数
        ESP_LOGI(TAG, "设置显示参数");
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_, DISPLAY_INVERT_COLOR));
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_, DISPLAY_SWAP_XY));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));
        
        // 开启显示
        ESP_LOGI(TAG, "开启显示");
        esp_lcd_panel_disp_on_off(panel_, true);
        
        // 创建显示对象
        display_ = new SpiLcdDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, 
            DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
        ESP_LOGI(TAG, "ST7789显示屏初始化完成");
        
        // 移除所有测试代码，只保留必要的初始化
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState();
        });
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeTools() {
        static LampController lamp(LAMP_GPIO);
    }

public:
    CompactBoardTest() :
        boot_button_(BOOT_BUTTON_GPIO) {        
        ESP_LOGI(TAG, "开始CompactBoardTest初始化");
        
        // 初始化SPI和显示
        InitializeSpi();
        InitializeSt7789Display();
        InitializeButtons();
        InitializeTools();
        
        // 初始化背光PWM控制
        if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            ESP_LOGI(TAG, "初始化背光PWM控制");
            GetBacklight()->RestoreBrightness();
        }
        
        ESP_LOGI(TAG, "CompactBoardTest初始化完成");
    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
            return &backlight;
        }
        return nullptr;
    }
};

DECLARE_BOARD(CompactBoardTest);