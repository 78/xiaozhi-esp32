#include "wifi_board.h"
#include "config.h"                // 本目录下的 config.h
#include "ir_controller.h"        // main/components/ir/ir_controller.h
#include "codecs/box_audio_codec.h" // 根据项目已有 codec 选型
#include <driver/i2c_master.h>
#include <esp_log.h>

#define TAG "WaveshareC6AI"

class WaveshareC6AIBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;
    // IR 控制器指针（仅示例，实际可做成成员对象）
    IrController* ir_controller_;
    void InitializeCodecI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = { .enable_internal_pullup = 1 }
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
    }

public:
    WaveshareC6AIBoard()
    {
        InitializeCodecI2c();

        // 在这里实例化 IR 控制器：rx, tx, module_internal_modulator = true
        ir_controller_ = new IrController(IR_RX_GPIO, IR_TX_GPIO, true);

        // 如需注册更多工具或初始化外设，可在此处加入
        ESP_LOGI(TAG, "Waveshare C6 AI Terminal board initialized");
    }

    virtual ~WaveshareC6AIBoard() {
        delete ir_controller_;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static BoxAudioCodec audio_codec(codec_i2c_bus_, I2C_NUM_0,
            AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            GPIO_NUM_NC, /* PA pin not used here */ 0x18 /* example addr */);
        return &audio_codec;
    }
};

DECLARE_BOARD(WaveshareC6AIBoard);
