#include "wifi_board.h"
#include "audio_codecs/es8311_audio_codec.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "i2c_device.h"
#include "iot/thing_manager.h"
#include "assets/lang_config.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <wifi_station.h>

#define TAG "AtomS3R M12+EchoBase"

#define PI4IOE_ADDR          0x43
#define PI4IOE_REG_CTRL      0x00
#define PI4IOE_REG_IO_PP     0x07
#define PI4IOE_REG_IO_DIR    0x03
#define PI4IOE_REG_IO_OUT    0x05
#define PI4IOE_REG_IO_PULLUP 0x0D

class Pi4ioe : public I2cDevice {
public:
    Pi4ioe(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
        WriteReg(PI4IOE_REG_IO_PP, 0x00); // Set to high-impedance
        WriteReg(PI4IOE_REG_IO_PULLUP, 0xFF); // Enable pull-up
        WriteReg(PI4IOE_REG_IO_DIR, 0x6E); // Set input=0, output=1
        WriteReg(PI4IOE_REG_IO_OUT, 0xFF); // Set outputs to 1
    }

    void SetSpeakerMute(bool mute) {
        WriteReg(PI4IOE_REG_IO_OUT, mute ? 0x00 : 0xFF);
    }
};

class AtomS3rCamM12EchoBaseBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    Pi4ioe* pi4ioe_ = nullptr;
    bool is_echo_base_connected_ = false;

    void InitializeI2c() {
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
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    void I2cDetect() {
        is_echo_base_connected_ = false;
        uint8_t echo_base_connected_flag = 0x00;
        uint8_t address;
        printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\r\n");
        for (int i = 0; i < 128; i += 16) {
            printf("%02x: ", i);
            for (int j = 0; j < 16; j++) {
                fflush(stdout);
                address = i + j;
                esp_err_t ret = i2c_master_probe(i2c_bus_, address, pdMS_TO_TICKS(200));
                if (ret == ESP_OK) {
                    printf("%02x ", address);
                    if (address == 0x18) {
                        echo_base_connected_flag |= 0xF0;
                    } else if (address == 0x43) {
                        echo_base_connected_flag |= 0x0F;
                    }
                } else if (ret == ESP_ERR_TIMEOUT) {
                    printf("UU ");
                } else {
                    printf("-- ");
                }
            }
            printf("\r\n");
        }
        is_echo_base_connected_ = (echo_base_connected_flag == 0xFF);
    }

    void CheckEchoBaseConnection() {
        if (is_echo_base_connected_) {
            return;
        }
        
        while (1) {
            ESP_LOGE(TAG, "Atomic Echo Base is disconnected");
            vTaskDelay(pdMS_TO_TICKS(1000));

            // Rerun detection
            I2cDetect();
            if (is_echo_base_connected_) {
                vTaskDelay(pdMS_TO_TICKS(500));
                I2cDetect();
                if (is_echo_base_connected_) {
                    ESP_LOGI(TAG, "Atomic Echo Base is reconnected");
                    vTaskDelay(pdMS_TO_TICKS(200));
                    esp_restart();
                }
            }
        }
    }

    void InitializePi4ioe() {
        ESP_LOGI(TAG, "Init PI4IOE");
        pi4ioe_ = new Pi4ioe(i2c_bus_, PI4IOE_ADDR);
        pi4ioe_->SetSpeakerMute(false);
    }

    void EnableCameraPower() {
        gpio_reset_pin((gpio_num_t)18);
        gpio_set_direction((gpio_num_t)18, GPIO_MODE_OUTPUT);
        gpio_set_pull_mode((gpio_num_t)18, GPIO_PULLDOWN_ONLY);

        ESP_LOGI(TAG, "Camera Power Enabled");

        vTaskDelay(pdMS_TO_TICKS(200));
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
    }

public:
    AtomS3rCamM12EchoBaseBoard() {
        EnableCameraPower(); // IO18 还会控制指示灯
        InitializeI2c();
        I2cDetect();
        CheckEchoBaseConnection();
        InitializePi4ioe();
        InitializeIot();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(
            i2c_bus_, 
            I2C_NUM_0, 
            AUDIO_INPUT_SAMPLE_RATE, 
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, 
            AUDIO_I2S_GPIO_BCLK, 
            AUDIO_I2S_GPIO_WS, 
            AUDIO_I2S_GPIO_DOUT, 
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_GPIO_PA, 
            AUDIO_CODEC_ES8311_ADDR, 
            false);
        return &audio_codec;
    }
};

DECLARE_BOARD(AtomS3rCamM12EchoBaseBoard);
