#include "wifi_board.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "i2c_device.h"
#include "audio_codec.h"
#include "led/led.h"
#include "assets/lang_config.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/i2s_std.h>
#include <esp_codec_dev.h>
#include <esp_codec_dev_defaults.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_gc9a01.h>

#include <mutex>

#define TAG "AtomS3R+EchoPyramid"

#define PYRAMID_SI5351_ADDR 0x60
#define PYRAMID_STM32_ADDR  0x1A
#define PYRAMID_AW87559_ADDR 0x5B
#define PYRAMID_POWER_ON_RETRY_COUNT 20
#define PYRAMID_POWER_ON_RETRY_DELAY_MS 250

#define STM32_SPK_RESTART_REG_ADDR 0xA0
#define STM32_RGB1_BRIGHTNESS_REG_ADDR 0x10
#define STM32_RGB2_BRIGHTNESS_REG_ADDR 0x11
#define STM32_RGB1_STATUS_REG_ADDR 0x20
#define STM32_RGB2_STATUS_REG_ADDR 0x60
#define STM32_RGB_NUM_MAX 13

#define AW87559_REG_ID      0x00
#define AW87559_REG_SYSCTRL 0x01
#define AW87559_REG_PAGR    0x06
#define AW87559_ID          0x5A
#define AW87559_SYS_EN_SW_MASK    (1 << 6)
#define AW87559_SYS_EN_BOOST_MASK (1 << 4)
#define AW87559_SYS_EN_PA_MASK    (1 << 3)
#define AW87559_GAIN_16_5DB 11

class Si5351 : public I2cDevice {
public:
    Si5351(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
        WriteReg(3, 0xFF); // Disable all clock outputs.
        WriteReg(16, 0x80);
        WriteReg(17, 0x80);
        WriteReg(18, 0x80);
        WriteReg(183, 0xC0); // Crystal load capacitance: 10 pF.
    }

    void SetMclk(uint32_t sample_rate) {
        if (sample_rate == 24000) {
            SetPll(884736000UL, 144); // 884.736 MHz / 144 = 6.144 MHz
        } else if (sample_rate == 16000) {
            SetPll(884736000UL, 216); // 4.096 MHz
        } else if (sample_rate == 44100) {
            SetPll(903168000UL, 80); // 11.2896 MHz
        } else if (sample_rate == 48000) {
            SetPll(884736000UL, 72); // 12.288 MHz
        } else {
            ESP_LOGW(TAG, "Unsupported Si5351 sample rate: %lu", static_cast<unsigned long>(sample_rate));
        }
    }

private:
    static constexpr uint32_t kXtalFreq = 27000000UL;

    void WriteRegs(uint8_t reg, const uint8_t* data, size_t length) {
        uint8_t buffer[9] = {};
        buffer[0] = reg;
        for (size_t i = 0; i < length; ++i) {
            buffer[i + 1] = data[i];
        }
        ESP_ERROR_CHECK(i2c_master_transmit(i2c_device_, buffer, length + 1, 100));
    }

    void SetPll(uint32_t pll_freq, uint32_t ms_div) {
        uint32_t a = pll_freq / kXtalFreq;
        uint32_t rest = pll_freq % kXtalFreq;
        uint32_t c = 1000000UL;
        uint32_t b = (rest * c) / kXtalFreq;

        uint32_t p1 = 128 * a + (128 * b) / c - 512;
        uint32_t p2 = 128 * b - c * ((128 * b) / c);
        uint32_t p3 = c;

        WriteReg(3, 0xFF);

        uint8_t pll_buf[8] = {
            static_cast<uint8_t>((p3 >> 8) & 0xFF),
            static_cast<uint8_t>(p3 & 0xFF),
            static_cast<uint8_t>((p1 >> 16) & 0x03),
            static_cast<uint8_t>((p1 >> 8) & 0xFF),
            static_cast<uint8_t>(p1 & 0xFF),
            static_cast<uint8_t>(((p3 >> 12) & 0xF0) | ((p2 >> 16) & 0x0F)),
            static_cast<uint8_t>((p2 >> 8) & 0xFF),
            static_cast<uint8_t>(p2 & 0xFF),
        };
        WriteRegs(26, pll_buf, sizeof(pll_buf));

        uint32_t ms_p1 = 128 * ms_div - 512;
        uint8_t ms_buf[8] = {
            0x00,
            0x01,
            static_cast<uint8_t>((ms_p1 >> 16) & 0x03),
            static_cast<uint8_t>((ms_p1 >> 8) & 0xFF),
            static_cast<uint8_t>(ms_p1 & 0xFF),
            0x00,
            0x00,
            0x00,
        };
        WriteRegs(50, ms_buf, sizeof(ms_buf)); // Multisynth1 -> CLK1

        WriteReg(17, 0x4F); // CLK1 from PLLA, 8 mA drive.
        WriteReg(16, 0x80);
        WriteReg(18, 0x80);
        WriteReg(177, 0xA0); // Reset PLLA.
        vTaskDelay(pdMS_TO_TICKS(10));
        WriteReg(3, 0xFD); // Enable CLK1 only.

        ESP_LOGI(TAG, "Si5351 CLK1 set to %lu Hz", static_cast<unsigned long>(pll_freq / ms_div));
    }
};

class Aw87559 : public I2cDevice {
public:
    Aw87559(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
        auto id = ReadReg(AW87559_REG_ID);
        if (id != AW87559_ID) {
            ESP_LOGW(TAG, "Unexpected AW87559 ID: 0x%02x", id);
        }

        UpdateBits(AW87559_REG_SYSCTRL, AW87559_SYS_EN_SW_MASK, AW87559_SYS_EN_SW_MASK);
        UpdateBits(AW87559_REG_SYSCTRL, AW87559_SYS_EN_BOOST_MASK, AW87559_SYS_EN_BOOST_MASK);
        UpdateBits(AW87559_REG_SYSCTRL, AW87559_SYS_EN_PA_MASK, AW87559_SYS_EN_PA_MASK);
        UpdateBits(AW87559_REG_PAGR, 0x1F, AW87559_GAIN_16_5DB);
    }

private:
    void UpdateBits(uint8_t reg, uint8_t mask, uint8_t value) {
        auto reg_value = ReadReg(reg);
        reg_value &= ~mask;
        reg_value |= value & mask;
        WriteReg(reg, reg_value);
    }
};

class Stm32PyramidCtrl : public I2cDevice {
public:
    Stm32PyramidCtrl(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
        ResetSpeaker();
        SetBrightness(1, 100);
        SetBrightness(2, 100);
        SetAllRgb(1, 0, 0, 64);
        SetAllRgb(2, 0, 0, 64);
    }

    void ResetSpeaker() {
        WriteReg(STM32_SPK_RESTART_REG_ADDR, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    void SetBrightness(uint8_t channel, uint8_t brightness) {
        if (brightness > 100) {
            brightness = 100;
        }
        WriteReg(channel == 1 ? STM32_RGB1_BRIGHTNESS_REG_ADDR : STM32_RGB2_BRIGHTNESS_REG_ADDR, brightness);
    }

    void SetAllRgb(uint8_t channel, uint8_t r, uint8_t g, uint8_t b) {
        const uint8_t base = (channel == 1) ? STM32_RGB1_STATUS_REG_ADDR : STM32_RGB2_STATUS_REG_ADDR;

        // Echo Pyramid STM32 uses 4-byte stride per LED: (B,G,R,0x00).
        // One page is 0x10 bytes and contains 4 LEDs.
        for (int page = 0; page < 4; ++page) {
            uint8_t reg = base + static_cast<uint8_t>(page * 0x10);
            uint8_t payload[1 + 16] = {0};
            payload[0] = reg;
            for (int i = 0; i < 4; ++i) {
                payload[1 + i * 4 + 0] = b;
                payload[1 + i * 4 + 1] = g;
                payload[1 + i * 4 + 2] = r;
                payload[1 + i * 4 + 3] = 0x00;
            }
            ESP_ERROR_CHECK(i2c_master_transmit(i2c_device_, payload, sizeof(payload), 100));
        }
    }

    void SetStatusColor(uint8_t r, uint8_t g, uint8_t b) {
        SetAllRgb(1, r, g, b);
        SetAllRgb(2, r, g, b);
    }
};

class PyramidStatusLed : public Led {
public:
    void SetController(Stm32PyramidCtrl* ctrl) { ctrl_ = ctrl; }

    void OnStateChanged() override {
        if (ctrl_ == nullptr) {
            return;
        }

        auto& app = Application::GetInstance();
        switch (app.GetDeviceState()) {
            case kDeviceStateListening:
                ctrl_->SetStatusColor(0, 64, 0);   // green
                break;
            case kDeviceStateSpeaking:
                ctrl_->SetStatusColor(64, 0, 0);   // red
                break;
            default:
                ctrl_->SetStatusColor(0, 0, 64);   // blue
                break;
        }
    }

private:
    Stm32PyramidCtrl* ctrl_ = nullptr;
};

class Lp5562 : public I2cDevice {
public:
    Lp5562(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
        WriteReg(0x00, 0B01000000); // Set chip_en to 1
        WriteReg(0x08, 0B00000001); // Enable internal clock
        WriteReg(0x70, 0B00000000); // Configure all LED outputs to be controlled from I2C registers

        // PWM clock frequency 558 Hz
        auto data = ReadReg(0x08);
        data = data | 0B01000000;
        WriteReg(0x08, data);
    }

    void SetBrightness(uint8_t brightness) {
        // Map 0~100 to 0~255
        brightness = brightness * 255 / 100;
        WriteReg(0x0E, brightness);
    }
};

class CustomBacklight : public Backlight {
public:
    CustomBacklight(Lp5562* lp5562) : lp5562_(lp5562) {}

    void SetBrightnessImpl(uint8_t brightness) override {
        if (lp5562_) {
            lp5562_->SetBrightness(brightness);
        } else {
            ESP_LOGE(TAG, "LP5562 not available");
        }
    }

private:
    Lp5562* lp5562_ = nullptr;
};

class PyramidAudioCodec : public AudioCodec {
public:
    PyramidAudioCodec(void* i2c_master_handle, i2c_port_t i2c_port, int input_sample_rate, int output_sample_rate,
        gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din,
        gpio_num_t pa_pin, uint8_t es8311_addr, uint8_t es7210_addr, bool input_reference) {
        duplex_ = true;
        input_reference_ = input_reference;
        input_channels_ = input_reference_ ? 2 : 1;
        output_channels_ = 1;
        input_sample_rate_ = input_sample_rate;
        output_sample_rate_ = output_sample_rate;
        input_gain_ = 30;
        pa_pin_ = pa_pin;

        CreateDuplexChannels(mclk, bclk, ws, dout, din);

        audio_codec_i2s_cfg_t i2s_cfg = {
            .port = I2S_NUM_0,
            .rx_handle = rx_handle_,
            .tx_handle = tx_handle_,
        };
        data_if_ = audio_codec_new_i2s_data(&i2s_cfg);
        assert(data_if_ != NULL);

        audio_codec_i2c_cfg_t i2c_cfg = {
            .port = i2c_port,
            .addr = es8311_addr,
            .bus_handle = i2c_master_handle,
        };
        out_ctrl_if_ = audio_codec_new_i2c_ctrl(&i2c_cfg);
        assert(out_ctrl_if_ != NULL);

        gpio_if_ = audio_codec_new_gpio();
        assert(gpio_if_ != NULL);

        es8311_codec_cfg_t es8311_cfg = {};
        es8311_cfg.ctrl_if = out_ctrl_if_;
        es8311_cfg.gpio_if = gpio_if_;
        es8311_cfg.codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC;
        es8311_cfg.pa_pin = pa_pin_;
        es8311_cfg.use_mclk = true;
        es8311_cfg.hw_gain.pa_voltage = 5.0;
        es8311_cfg.hw_gain.codec_dac_voltage = 3.3;
        out_codec_if_ = es8311_codec_new(&es8311_cfg);
        assert(out_codec_if_ != NULL);

        esp_codec_dev_cfg_t dev_cfg = {
            .dev_type = ESP_CODEC_DEV_TYPE_OUT,
            .codec_if = out_codec_if_,
            .data_if = data_if_,
        };
        output_dev_ = esp_codec_dev_new(&dev_cfg);
        assert(output_dev_ != NULL);

        i2c_cfg.addr = es7210_addr;
        in_ctrl_if_ = audio_codec_new_i2c_ctrl(&i2c_cfg);
        assert(in_ctrl_if_ != NULL);

        es7210_codec_cfg_t es7210_cfg = {};
        es7210_cfg.ctrl_if = in_ctrl_if_;
        es7210_cfg.mic_selected = ES7210_SEL_MIC1 | ES7210_SEL_MIC3;
        in_codec_if_ = es7210_codec_new(&es7210_cfg);
        assert(in_codec_if_ != NULL);

        dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_IN;
        dev_cfg.codec_if = in_codec_if_;
        input_dev_ = esp_codec_dev_new(&dev_cfg);
        assert(input_dev_ != NULL);

        ESP_LOGI(TAG, "Pyramid audio codec initialized");
    }

    virtual ~PyramidAudioCodec() {
        if (output_dev_) {
            esp_codec_dev_close(output_dev_);
            esp_codec_dev_delete(output_dev_);
        }
        if (input_dev_) {
            esp_codec_dev_close(input_dev_);
            esp_codec_dev_delete(input_dev_);
        }

        audio_codec_delete_codec_if(in_codec_if_);
        audio_codec_delete_ctrl_if(in_ctrl_if_);
        audio_codec_delete_codec_if(out_codec_if_);
        audio_codec_delete_ctrl_if(out_ctrl_if_);
        audio_codec_delete_gpio_if(gpio_if_);
        audio_codec_delete_data_if(data_if_);
    }

    void SetOutputVolume(int volume) override {
        std::lock_guard<std::mutex> lock(data_if_mutex_);
        if (output_dev_ != nullptr) {
            ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(output_dev_, volume));
        }
        AudioCodec::SetOutputVolume(volume);
    }

    void EnableInput(bool enable) override {
        std::lock_guard<std::mutex> lock(data_if_mutex_);
        if (enable == input_enabled_) {
            return;
        }
        if (enable) {
            esp_codec_dev_sample_info_t fs = {
                .bits_per_sample = 16,
                .channel = 2,
                .channel_mask = ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0),
                .sample_rate = static_cast<uint32_t>(input_sample_rate_),
                .mclk_multiple = 0,
            };
            if (input_reference_) {
                fs.channel_mask |= ESP_CODEC_DEV_MAKE_CHANNEL_MASK(1);
            }
            ESP_ERROR_CHECK(esp_codec_dev_open(input_dev_, &fs));
            ESP_ERROR_CHECK(esp_codec_dev_set_in_channel_gain(input_dev_, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0), input_gain_));
        } else {
            ESP_ERROR_CHECK(esp_codec_dev_close(input_dev_));
        }
        AudioCodec::EnableInput(enable);
    }

    void EnableOutput(bool enable) override {
        std::lock_guard<std::mutex> lock(data_if_mutex_);
        if (enable == output_enabled_) {
            return;
        }
        if (enable) {
            esp_codec_dev_sample_info_t fs = {
                .bits_per_sample = 16,
                .channel = 1,
                .channel_mask = 0,
                .sample_rate = static_cast<uint32_t>(output_sample_rate_),
                .mclk_multiple = 0,
            };
            ESP_ERROR_CHECK(esp_codec_dev_open(output_dev_, &fs));
            ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(output_dev_, output_volume_));
        } else {
            ESP_ERROR_CHECK(esp_codec_dev_close(output_dev_));
        }
        AudioCodec::EnableOutput(enable);
    }

private:
    const audio_codec_data_if_t* data_if_ = nullptr;
    const audio_codec_ctrl_if_t* out_ctrl_if_ = nullptr;
    const audio_codec_if_t* out_codec_if_ = nullptr;
    const audio_codec_ctrl_if_t* in_ctrl_if_ = nullptr;
    const audio_codec_if_t* in_codec_if_ = nullptr;
    const audio_codec_gpio_if_t* gpio_if_ = nullptr;
    esp_codec_dev_handle_t output_dev_ = nullptr;
    esp_codec_dev_handle_t input_dev_ = nullptr;
    gpio_num_t pa_pin_ = GPIO_NUM_NC;
    std::mutex data_if_mutex_;

    void CreateDuplexChannels(gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din) {
        assert(input_sample_rate_ == output_sample_rate_);

        i2s_chan_config_t chan_cfg = {
            .id = I2S_NUM_0,
            .role = I2S_ROLE_MASTER,
            .dma_desc_num = AUDIO_CODEC_DMA_DESC_NUM,
            .dma_frame_num = AUDIO_CODEC_DMA_FRAME_NUM,
            .auto_clear_after_cb = true,
            .auto_clear_before_cb = false,
            .intr_priority = 0,
        };
        ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle_, &rx_handle_));

        i2s_std_config_t std_cfg = {
            .clk_cfg = {
                .sample_rate_hz = static_cast<uint32_t>(output_sample_rate_),
                .clk_src = I2S_CLK_SRC_DEFAULT,
                .mclk_multiple = I2S_MCLK_MULTIPLE_256,
#ifdef I2S_HW_VERSION_2
                .ext_clk_freq_hz = 0,
#endif
            },
            .slot_cfg = {
                .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
                .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
                .slot_mode = I2S_SLOT_MODE_STEREO,
                .slot_mask = I2S_STD_SLOT_BOTH,
                .ws_width = I2S_DATA_BIT_WIDTH_16BIT,
                .ws_pol = false,
                .bit_shift = true,
#ifdef I2S_HW_VERSION_2
                .left_align = true,
                .big_endian = false,
                .bit_order_lsb = false,
#endif
            },
            .gpio_cfg = {
                .mclk = mclk,
                .bclk = bclk,
                .ws = ws,
                .dout = dout,
                .din = din,
                .invert_flags = {
                    .mclk_inv = false,
                    .bclk_inv = false,
                    .ws_inv = false,
                },
            },
        };

        ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &std_cfg));
        ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle_, &std_cfg));
        ESP_ERROR_CHECK(i2s_channel_enable(tx_handle_));
        ESP_ERROR_CHECK(i2s_channel_enable(rx_handle_));
        ESP_LOGI(TAG, "Pyramid duplex I2S channels created");
    }

    int Read(int16_t* dest, int samples) override {
        if (input_enabled_) {
            ESP_ERROR_CHECK_WITHOUT_ABORT(esp_codec_dev_read(input_dev_, reinterpret_cast<void*>(dest), samples * sizeof(int16_t)));
        }
        return samples;
    }

    int Write(const int16_t* data, int samples) override {
        if (output_enabled_) {
            ESP_ERROR_CHECK_WITHOUT_ABORT(esp_codec_dev_write(output_dev_, const_cast<int16_t*>(data), samples * sizeof(int16_t)));
        }
        return samples;
    }
};

static const gc9a01_lcd_init_cmd_t gc9107_lcd_init_cmds[] = {
    //  {cmd, { data }, data_size, delay_ms}
    {0xfe, (uint8_t[]){0x00}, 0, 0},
    {0xef, (uint8_t[]){0x00}, 0, 0},
    {0xb0, (uint8_t[]){0xc0}, 1, 0},
    {0xb2, (uint8_t[]){0x2f}, 1, 0},
    {0xb3, (uint8_t[]){0x03}, 1, 0},
    {0xb6, (uint8_t[]){0x19}, 1, 0},
    {0xb7, (uint8_t[]){0x01}, 1, 0},
    {0xac, (uint8_t[]){0xcb}, 1, 0},
    {0xab, (uint8_t[]){0x0e}, 1, 0},
    {0xb4, (uint8_t[]){0x04}, 1, 0},
    {0xa8, (uint8_t[]){0x19}, 1, 0},
    {0xb8, (uint8_t[]){0x08}, 1, 0},
    {0xe8, (uint8_t[]){0x24}, 1, 0},
    {0xe9, (uint8_t[]){0x48}, 1, 0},
    {0xea, (uint8_t[]){0x22}, 1, 0},
    {0xc6, (uint8_t[]){0x30}, 1, 0},
    {0xc7, (uint8_t[]){0x18}, 1, 0},
    {0xf0,
    (uint8_t[]){0x1f, 0x28, 0x04, 0x3e, 0x2a, 0x2e, 0x20, 0x00, 0x0c, 0x06,
                0x00, 0x1c, 0x1f, 0x0f},
    14, 0},
    {0xf1,
    (uint8_t[]){0x00, 0x2d, 0x2f, 0x3c, 0x6f, 0x1c, 0x0b, 0x00, 0x00, 0x00,
                0x07, 0x0d, 0x11, 0x0f},
    14, 0},
};

class AtomS3rEchoPyramidBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    i2c_master_bus_handle_t i2c_bus_internal_;
    Si5351* si5351_ = nullptr;
    Aw87559* aw87559_ = nullptr;
    Stm32PyramidCtrl* stm32_ = nullptr;
    PyramidStatusLed led_;
    Lp5562* lp5562_ = nullptr;
    Display* display_ = nullptr;
    Button boot_button_;
    bool is_pyramid_connected_ = false;

    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_1,
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

        i2c_bus_cfg.i2c_port = I2C_NUM_0;
        i2c_bus_cfg.sda_io_num = GPIO_NUM_45;
        i2c_bus_cfg.scl_io_num = GPIO_NUM_0;
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_internal_));
    }

    void I2cDetect() {
        is_pyramid_connected_ = false;
        bool has_es8311 = false;
        bool has_es7210 = false;
        bool has_si5351 = false;
        bool has_stm32 = false;
        bool has_aw87559 = false;
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
                    if (address == (AUDIO_CODEC_ES8311_ADDR >> 1)) {
                        has_es8311 = true;
                    } else if (address == (AUDIO_CODEC_ES7210_ADDR >> 1)) {
                        has_es7210 = true;
                    } else if (address == PYRAMID_SI5351_ADDR) {
                        has_si5351 = true;
                    } else if (address == PYRAMID_STM32_ADDR) {
                        has_stm32 = true;
                    } else if (address == PYRAMID_AW87559_ADDR) {
                        has_aw87559 = true;
                    }
                } else if (ret == ESP_ERR_TIMEOUT) {
                    printf("UU ");
                } else {
                    printf("-- ");
                }
            }
            printf("\r\n");
        }

        is_pyramid_connected_ = has_es8311 && has_es7210 && has_si5351 && has_stm32 && has_aw87559;
    }

    void WaitForPyramidConnection() {
        for (int attempt = 0; attempt < PYRAMID_POWER_ON_RETRY_COUNT; ++attempt) {
            I2cDetect();
            if (is_pyramid_connected_) {
                if (attempt > 0) {
                    ESP_LOGI(TAG, "Echo Pyramid detected after %d retries", attempt);
                }
                return;
            }

            ESP_LOGW(TAG, "Echo Pyramid not ready, retrying (%d/%d)",
                attempt + 1, PYRAMID_POWER_ON_RETRY_COUNT);
            vTaskDelay(pdMS_TO_TICKS(PYRAMID_POWER_ON_RETRY_DELAY_MS));
        }
    }

    void CheckPyramidConnection() {
        if (is_pyramid_connected_) {
            return;
        }

        InitializeLp5562();
        InitializeSpi();
        InitializeGc9107Display();
        InitializeButtons();
        GetBacklight()->SetBrightness(100);

        display_->SetupUI();
        display_->SetStatus(Lang::Strings::ERROR);
        display_->SetEmotion("triangle_exclamation");
        display_->SetChatMessage("system", "Echo Pyramid\nnot connected");

        while (1) {
            ESP_LOGE(TAG, "Echo Pyramid is disconnected");
            vTaskDelay(pdMS_TO_TICKS(1000));

            I2cDetect();
            if (is_pyramid_connected_) {
                vTaskDelay(pdMS_TO_TICKS(500));
                I2cDetect();
                if (is_pyramid_connected_) {
                    ESP_LOGI(TAG, "Echo Pyramid is reconnected");
                    vTaskDelay(pdMS_TO_TICKS(200));
                    esp_restart();
                }
            }
        }
    }

    void InitializePyramidDevices() {
        ESP_LOGI(TAG, "Init Echo Pyramid devices");
        si5351_ = new Si5351(i2c_bus_, PYRAMID_SI5351_ADDR);
        si5351_->SetMclk(AUDIO_OUTPUT_SAMPLE_RATE);
        stm32_ = new Stm32PyramidCtrl(i2c_bus_, PYRAMID_STM32_ADDR);
        led_.SetController(stm32_);
        led_.OnStateChanged();
        aw87559_ = new Aw87559(i2c_bus_, PYRAMID_AW87559_ADDR);
    }

    void InitializeLp5562() {
        ESP_LOGI(TAG, "Init LP5562");
        lp5562_ = new Lp5562(i2c_bus_internal_, 0x30);
    }

    void InitializeSpi() {
        ESP_LOGI(TAG, "Initialize SPI bus");
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = GPIO_NUM_21;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = GPIO_NUM_15;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeGc9107Display() {
        ESP_LOGI(TAG, "Init GC9107 display");

        ESP_LOGI(TAG, "Install panel IO");
        esp_lcd_panel_io_handle_t io_handle = NULL;
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = GPIO_NUM_14;
        io_config.dc_gpio_num = GPIO_NUM_42;
        io_config.spi_mode = 0;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &io_handle));

        ESP_LOGI(TAG, "Install GC9A01 panel driver");
        esp_lcd_panel_handle_t panel_handle = NULL;
        gc9a01_vendor_config_t gc9107_vendor_config = {
            .init_cmds = gc9107_lcd_init_cmds,
            .init_cmds_size = sizeof(gc9107_lcd_init_cmds) / sizeof(gc9a01_lcd_init_cmd_t),
        };
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_48;
        panel_config.rgb_endian = LCD_RGB_ENDIAN_BGR;
        panel_config.bits_per_pixel = 16;
        panel_config.vendor_config = &gc9107_vendor_config;

        ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

        display_ = new SpiLcdDisplay(io_handle, panel_handle,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
    }

public:
    AtomS3rEchoPyramidBoard() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        WaitForPyramidConnection();
        CheckPyramidConnection();
        InitializePyramidDevices();
        InitializeLp5562();
        InitializeSpi();
        InitializeGc9107Display();
        InitializeButtons();
        GetBacklight()->RestoreBrightness();
    }

    virtual Led* GetLed() override {
        return &led_;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static PyramidAudioCodec audio_codec(
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
            AUDIO_CODEC_ES7210_ADDR,
            AUDIO_INPUT_REFERENCE);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        static CustomBacklight backlight(lp5562_);
        return &backlight;
    }
};

DECLARE_BOARD(AtomS3rEchoPyramidBoard);
