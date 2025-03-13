#include "wifi_board.h"
#include "audio_codecs/es8311_audio_codec.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "i2c_device.h"
#include "iot/thing_manager.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <wifi_station.h>
#include "led/circular_strip.h"

#define TAG "XX+EchoBase" // 日志标签

// PI4IOE5V6416 I2C扩展芯片的寄存器地址
#define PI4IOE_ADDR          0x43 // PI4IOE设备地址
#define PI4IOE_REG_CTRL      0x00 // 控制寄存器
#define PI4IOE_REG_IO_PP     0x07 // 输入/输出端口配置寄存器
#define PI4IOE_REG_IO_DIR    0x03 // 方向寄存器
#define PI4IOE_REG_IO_OUT    0x05 // 输出寄存器
#define PI4IOE_REG_IO_PULLUP 0x0D // 上拉寄存器

// PI4IOE5V6416 I2C扩展芯片类
class Pi4ioe : public I2cDevice {
public:
    Pi4ioe(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
        WriteReg(PI4IOE_REG_IO_PP, 0x00); // 设置端口为高阻态
        WriteReg(PI4IOE_REG_IO_PULLUP, 0xFF); // 启用上拉电阻
        WriteReg(PI4IOE_REG_IO_DIR, 0x6E); // 设置输入=0，输出=1
        WriteReg(PI4IOE_REG_IO_OUT, 0xFF); // 设置输出为1
    }

    // 设置扬声器静音状态
    void SetSpeakerMute(bool mute) {
        WriteReg(PI4IOE_REG_IO_OUT, mute ? 0x00 : 0xFF); // 根据静音状态设置输出
    }
};

// AtomMatrixEchoBaseBoard开发板类
class AtomMatrixEchoBaseBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_; // I2C总线句柄
    Pi4ioe* pi4ioe_; // PI4IOE扩展芯片
    Button face_button_; // 面板按钮

    // 初始化I2C外设
    void InitializeI2c() {
        // I2C总线配置
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_1, // I2C端口号
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN, // SDA引脚
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN, // SCL引脚
            .clk_source = I2C_CLK_SRC_DEFAULT, // 时钟源
            .glitch_ignore_cnt = 7, // 毛刺忽略计数
            .intr_priority = 0, // 中断优先级
            .trans_queue_depth = 0, // 传输队列深度
            .flags = {
                .enable_internal_pullup = 1, // 启用内部上拉
            },
        };

        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_)); // 创建I2C总线
    }

    // I2C设备检测
    void I2cDetect() {
        uint8_t address;
        printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\r\n");
        for (int i = 0; i < 128; i += 16) {
            printf("%02x: ", i);
            for (int j = 0; j < 16; j++) {
                fflush(stdout);
                address = i + j;
                esp_err_t ret = i2c_master_probe(i2c_bus_, address, pdMS_TO_TICKS(200)); // 探测设备
                if (ret == ESP_OK) {
                    printf("%02x ", address); // 设备存在
                } else if (ret == ESP_ERR_TIMEOUT) {
                    printf("UU "); // 超时
                } else {
                    printf("-- "); // 设备不存在
                }
            }
            printf("\r\n");
        }
    }

    // 初始化PI4IOE扩展芯片
    void InitializePi4ioe() {
        ESP_LOGI(TAG, "Init PI4IOE"); // 日志：初始化PI4IOE
        pi4ioe_ = new Pi4ioe(i2c_bus_, PI4IOE_ADDR); // 创建PI4IOE对象
        pi4ioe_->SetSpeakerMute(false); // 取消扬声器静音
    }

    // 初始化按钮
    void InitializeButtons() {
        face_button_.OnClick([this]() {
            ESP_LOGI(TAG, "  ===>>>  face_button_.OnClick "); // 日志：按钮点击事件
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration(); // 重置WiFi配置
            } 
            app.ToggleChatState(); // 切换聊天状态
        });
    }

    // 初始化物联网设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker")); // 添加扬声器设备
    }

public:
    AtomMatrixEchoBaseBoard() : face_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c(); // 初始化I2C
        I2cDetect(); // I2C设备检测
        InitializePi4ioe(); // 初始化PI4IOE
        InitializeButtons(); // 初始化按钮
        InitializeIot(); // 初始化物联网设备
    }

    // 获取LED对象
    virtual Led* GetLed() override {
        static CircularStrip led(BUILTIN_LED_GPIO, 25); // 创建环形LED灯带
        return &led;
    }

    // 获取音频编解码器对象
    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(
            i2c_bus_, 
            I2C_NUM_1, 
            AUDIO_INPUT_SAMPLE_RATE, 
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, 
            AUDIO_I2S_GPIO_BCLK, 
            AUDIO_I2S_GPIO_WS, 
            AUDIO_I2S_GPIO_DOUT, 
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_GPIO_PA, 
            AUDIO_CODEC_ES8311_ADDR, 
            false); // 创建ES8311音频编解码器
        return &audio_codec;
    }
};

DECLARE_BOARD(AtomMatrixEchoBaseBoard); // 声明开发板