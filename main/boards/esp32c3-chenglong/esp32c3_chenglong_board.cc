#include "wifi_board.h"
#include "audio_codecs/es8311_audio_codec.h"
#include "application.h"
#include "button.h"
#include "led/single_led.h"
#include "config.h"
#include "iot/thing_manager.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <esp_efuse_table.h>
#include <driver/i2c_master.h>
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#include "iot/thing_manager.h"
#include "led/single_led.h"
#include "settings.h"

#include "display/lcd_display.h"
#include <esp_lcd_panel_vendor.h>
#include <cstring>  // 添加这个头文件用于memcpy
#define TAG "Esp32c3ChenglongBoard"
LV_FONT_DECLARE(font_puhui_20_4);
LV_FONT_DECLARE(font_awesome_20_4);





class Esp32c3ChenglongBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;
    Button boot_button_;
    TaskHandle_t uart_task_handle_;
    bool press_to_talk_enabled_ = false;

    LcdDisplay* display_;
    SingleLed* led_strip_ = nullptr;
    
    void InitializeCodecI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
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
// 添加串口监听任务
    static void UartListenTask(void* arg) {
        Esp32c3ChenglongBoard* board = static_cast<Esp32c3ChenglongBoard*>(arg);
        uint8_t data[128];
        
        ESP_LOGI(TAG, "UART listen task started");

        while (true) {
            int length = uart_read_bytes(UART_NUM_0, data, sizeof(data), pdMS_TO_TICKS(100));
            if (length > 0) {
                // 检查是否收到握手请求 A5 FA 00 82 01 00 20 FB
                if (length == 8 && data[0] == 0xA5 && data[1] == 0xFA && 
                    data[2] == 0x00 && data[3] == 0x82 && 
                    data[4] == 0x01 && data[5] == 0x00 && 
                    data[6] == 0x20 && data[7] == 0xFB) {
                    
                    // 发送握手响应
                    uint8_t response[] = {0xA5, 0xFA, 0x00, 0x82, 0x01, 0x00, 0x21, 0xFB};
                    board->SendUartResponse(response, sizeof(response));
                    // ESP_LOGI(TAG, "Sent handshake response");   注意期间不能任何有打印，否则会送到串口到CI1302了，UART这个引脚会默认吧日志也往这里送，会干扰，一定要小心注意，不应该使用这个引脚来通讯
                }
                // 检查是否收到唤醒请求 A5 FA 00 81 01 00 21 FB
                if (length == 8 && data[0] == 0xA5 && data[1] == 0xFA && 
                    data[2] == 0x00 && data[3] == 0x81 && 
                    data[4] == 0x01 && data[5] == 0x00 && 
                    data[6] == 0x21 && data[7] == 0xFB) {
                    
                    // 发送唤醒响应A5 FA 00 82 01 00 22 FB
                    uint8_t response[] = {0xA5, 0xFA, 0x00, 0x82, 0x01, 0x00, 0x22, 0xFB};
                    board->SendUartResponse(response, sizeof(response));
                    // ESP_LOGI(TAG, "收到唤醒词，开始对话"); 注意这期间能任何有打印，否则会送到串口到CI1302了，因为用了urtx0发的，需要配置禁止往串口写日志才能不会被影响。

                    Application::GetInstance().WakeWordInvoke("你好");
                }
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
     // 添加串口初始化函数
    void InitializeUart() {
          // 1. 禁用这个UART的日志输出
    esp_log_level_set("uart", ESP_LOG_NONE);

        // 2. 然后配置UART
        uart_config_t uart_config = {
            .baud_rate = 9600,
            .data_bits = UART_DATA_8_BITS,
            .parity    = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .source_clk = UART_SCLK_APB,
        };
    
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 256, 256, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &uart_config));
    
    // 3. 最后设置UART引脚
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_0, GPIO_NUM_21, GPIO_NUM_20, 
        UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

        
        ESP_LOGI(TAG, "UART initialized successfully");  // 添加初始化成功日志

        // 创建串口监听任务
        xTaskCreate(UartListenTask,          // 任务函数
                   "uart_task",              // 任务名称
                   2048,                     // 任务堆栈大小
                   this,                     // 传递给任务的参数
                   5,                        // 任务优先级
                   &uart_task_handle_);      // 任务句柄

    }



void SendUartResponse(const uint8_t* response, size_t length) {

    uint8_t buffer[length + 1];
    buffer[0] = length;  // 第一个字节是数据长度
    memcpy(buffer + 1, response, length);  // 复制实际数据
    
    // 2. 发送完整数据包
    uart_write_bytes(UART_NUM_0, buffer, length + 1);
    uart_wait_tx_done(UART_NUM_0, pdMS_TO_TICKS(100));

}

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            if (!press_to_talk_enabled_) {
                app.ToggleChatState();
            }
        });
        boot_button_.OnPressDown([this]() {
            if (press_to_talk_enabled_) {
                Application::GetInstance().StartListening();
            }
        });
        boot_button_.OnPressUp([this]() {
            if (press_to_talk_enabled_) {
                Application::GetInstance().StopListening();
            }
        });
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot() {
        Settings settings("vendor");
        press_to_talk_enabled_ = settings.GetInt("press_to_talk", 0) != 0;

        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("PressToTalk"));
    }

    void InitializeSt7789Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_SPI_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = 2;
        io_config.pclk_hz = 80 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片ST7789
        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
        
        esp_lcd_panel_reset(panel);

        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, true);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        display_ = new SpiLcdDisplay(panel_io, panel,
                                DISPLAY_WIDTH, DISPLAY_HEIGHT, 
                                DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, 
                                DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, 
                                DISPLAY_SWAP_XY,
                                {
                                    .text_font = &font_puhui_20_4,
                                    .icon_font = &font_awesome_20_4,
                                    .emoji_font = font_emoji_32_init(),
                                });
        // display_->SetBacklight(50);  // 设置较高的亮度
    }

public:
    Esp32c3ChenglongBoard() : boot_button_(BOOT_BUTTON_GPIO) {  
        // 把 ESP32C3 的 VDD SPI 引脚作为普通 GPIO 口使用
        esp_efuse_write_field_bit(ESP_EFUSE_VDD_SPI_AS_GPIO);

        InitializeCodecI2c();
        InitializeButtons();
        InitializeIot();
        InitializeUart();  // 添加串口初始化
        //  InitUart();

        InitializeSpi();//tft显示屏
        InitializeSt7789Display();

        auto codec = GetAudioCodec();
        codec->SetOutputVolume(90);
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_SPI_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_SPI_SCK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    virtual Led* GetLed() override {
        // 返回一个空的LED实现，完全不初始化LED驱动
        // return new NoLed();
        if (!led_strip_) {
            led_strip_ = new SingleLed(BUILTIN_LED_GPIO);
        }
        return led_strip_;
    }
    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(codec_i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

    void SetPressToTalkEnabled(bool enabled) {
        press_to_talk_enabled_ = enabled;

        Settings settings("vendor", true);
        settings.SetInt("press_to_talk", enabled ? 1 : 0);
        ESP_LOGI(TAG, "Press to talk enabled: %d", enabled);
    }

    bool IsPressToTalkEnabled() {
        return press_to_talk_enabled_;
    }
};

DECLARE_BOARD(Esp32c3ChenglongBoard);



namespace iot {

class PressToTalk : public Thing {
public:
    PressToTalk() : Thing("PressToTalk", "控制对话模式，一种是长按对话，一种是单击后连续对话。") {
        // 定义设备的属性
        properties_.AddBooleanProperty("enabled", "true 表示长按说话模式，false 表示单击说话模式", []() -> bool {
            auto board = static_cast<Esp32c3ChenglongBoard*>(&Board::GetInstance());
            return board->IsPressToTalkEnabled();
        });

        // 定义设备可以被远程执行的指令
        methods_.AddMethod("SetEnabled", "启用或禁用长按说话模式，调用前需要经过用户确认", ParameterList({
            Parameter("enabled", "true 表示长按说话模式，false 表示单击说话模式", kValueTypeBoolean, true)
        }), [](const ParameterList& parameters) {
            bool enabled = parameters["enabled"].boolean();
            auto board = static_cast<Esp32c3ChenglongBoard*>(&Board::GetInstance());
            board->SetPressToTalkEnabled(enabled);
        });
    }
};

} // namespace iot

DECLARE_THING(PressToTalk);