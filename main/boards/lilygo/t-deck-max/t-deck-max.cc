#include "application.h"
#include "backlight.h"
#include "button.h"
#include "codecs/es8311_audio_codec.h"
#include "config.h"
#include "display/lcd_display.h"
#include "t-deck-max_epd_display.h"
#include "wifi_board.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <mutex>

#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_lvgl_port.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {

constexpr const char* kTag = "TDeckMaxBoard";
constexpr int kI2cTimeoutMs = 100;

class MaxXl9555 {
public:
    bool Initialize(i2c_master_bus_handle_t bus) {
        i2c_device_config_t device_config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = T_DECK_MAX_XL9555_ADDRESS,
            .scl_speed_hz = 400000,
        };
        if (i2c_master_bus_add_device(bus, &device_config, &device_) != ESP_OK) {
            ESP_LOGE(kTag, "XL9555 device setup failed");
            return false;
        }

        direction_ = 0xFFFF;
        constexpr std::array<uint8_t, 11> output_pins = {
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
        };
        for (uint8_t pin : output_pins) {
            direction_ &= static_cast<uint16_t>(~(1U << pin));
        }

        // Configure latches before switching pins to outputs to avoid glitches.
        output_state_ = 0xFFFF;
        if (!WriteOutput() || !WriteRegister(0x06, direction_)) {
            ESP_LOGE(kTag, "XL9555 register setup failed");
            return false;
        }

        // Power rails and the ES8311 route are the MAX defaults used by the
        // factory example. The amplifier remains off until audio is enabled.
        return SetOutputState(0, true) && SetOutputState(1, true) && SetOutputState(2, true) &&
               SetOutputState(3, true) && SetOutputState(4, true) && SetOutputState(5, true) &&
               SetOutputState(T_DECK_MAX_XL9555_AMPLIFIER, false) &&
               SetOutputState(T_DECK_MAX_XL9555_TOUCH_RESET, true) && SetOutputState(8, true) &&
               SetOutputState(T_DECK_MAX_XL9555_KEYPAD_RESET, true) &&
               SetOutputState(T_DECK_MAX_XL9555_AUDIO_SEL, false);
    }

    bool SetOutputState(uint8_t pin, bool high) {
        if (device_ == nullptr || pin >= 16) {
            return false;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        if (high) {
            output_state_ |= static_cast<uint16_t>(1U << pin);
        } else {
            output_state_ &= static_cast<uint16_t>(~(1U << pin));
        }
        return WriteOutputLocked();
    }

    bool WriteOutput() {
        std::lock_guard<std::mutex> lock(mutex_);
        return WriteOutputLocked();
    }

private:
    bool WriteOutputLocked() { return WriteRegister(0x02, output_state_); }

    bool WriteRegister(uint8_t reg, uint16_t value) {
        if (device_ == nullptr) {
            return false;
        }
        uint8_t payload[3] = {
            reg,
            static_cast<uint8_t>(value & 0xFF),
            static_cast<uint8_t>(value >> 8),
        };
        return i2c_master_transmit(device_, payload, sizeof(payload), kI2cTimeoutMs) == ESP_OK;
    }

    i2c_master_dev_handle_t device_ = nullptr;
    uint16_t output_state_ = 0xFFFF;
    uint16_t direction_ = 0xFFFF;
    std::mutex mutex_;
};

class Cst328Touch {
public:
    bool Initialize(i2c_master_bus_handle_t bus) {
        i2c_device_config_t device_config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = T_DECK_MAX_TOUCH_ADDRESS,
            .scl_speed_hz = 200000,
        };
        if (i2c_master_bus_add_device(bus, &device_config, &device_) != ESP_OK) {
            ESP_LOGE(kTag, "CST328 device setup failed");
            return false;
        }

        // CST3xx controllers enter normal reporting mode through D109.
        const uint8_t normal_mode[] = {0xD1, 0x09};
        return i2c_master_transmit(device_, normal_mode, sizeof(normal_mode), kI2cTimeoutMs) ==
               ESP_OK;
    }

    bool ReadPoint(int16_t* x, int16_t* y) {
        if (device_ == nullptr || x == nullptr || y == nullptr) {
            return false;
        }

        const uint8_t report_register[] = {0xD0, 0x00};
        uint8_t report[7] = {};
        if (i2c_master_transmit_receive(device_, report_register, sizeof(report_register), report,
                                        sizeof(report), kI2cTimeoutMs) != ESP_OK) {
            return false;
        }

        const uint8_t point_count = static_cast<uint8_t>(report[5] & 0x7F);
        if (point_count > 1) {
            // Additional CST3xx points follow at D007. One point is enough for
            // LVGL, but consuming the complete report keeps the controller FIFO
            // synchronized for the next poll.
            const uint8_t extra_length =
                static_cast<uint8_t>(std::min<uint8_t>(point_count, 5) - 1U) * 5U + 3U;
            uint8_t extra[23] = {};
            ReadRegister(0xD007, extra, extra_length);
        }

        const bool valid =
            report[6] == 0xAB && report[0] != 0xAB && point_count != 0 && (report[5] & 0x80) == 0;
        const bool acknowledged = AcknowledgeReport();
        if (!valid || !acknowledged) {
            return false;
        }

        const uint16_t raw_x = static_cast<uint16_t>((report[1] << 4) | (report[3] >> 4));
        const uint16_t raw_y = static_cast<uint16_t>((report[2] << 4) | (report[3] & 0x0F));
        *x = static_cast<int16_t>(std::min<uint16_t>(raw_x, T_DECK_MAX_EPD_WIDTH - 1));
        *y = static_cast<int16_t>(std::min<uint16_t>(raw_y, T_DECK_MAX_EPD_HEIGHT - 1));
        return true;
    }

private:
    bool ReadRegister(uint16_t reg, uint8_t* data, size_t size) {
        const uint8_t command[] = {
            static_cast<uint8_t>(reg >> 8),
            static_cast<uint8_t>(reg & 0xFF),
        };
        return i2c_master_transmit_receive(device_, command, sizeof(command), data, size,
                                           kI2cTimeoutMs) == ESP_OK;
    }

    bool AcknowledgeReport() {
        const uint32_t clear_command = 0xD000AB;
        const uint8_t command[] = {
            static_cast<uint8_t>(clear_command >> 16),
            static_cast<uint8_t>(clear_command >> 8),
            static_cast<uint8_t>(clear_command),
        };
        return i2c_master_transmit(device_, command, sizeof(command), kI2cTimeoutMs) == ESP_OK;
    }

    i2c_master_dev_handle_t device_ = nullptr;
};

class Tca8418Keypad {
public:
    bool Initialize(i2c_master_bus_handle_t bus) {
        i2c_device_config_t device_config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = T_DECK_MAX_KEYPAD_ADDRESS,
            .scl_speed_hz = 400000,
        };
        if (i2c_master_bus_add_device(bus, &device_config, &device_) != ESP_OK) {
            ESP_LOGE(kTag, "TCA8418 device setup failed");
            return false;
        }

        // Match the MAX factory example: four rows and ten columns. TCA8418
        // uses the low row/column pins, with columns 8 and 9 in register 0x1F.
        if (!WriteRegister(0x1D, 0x0F) || !WriteRegister(0x1E, 0xFF) ||
            !WriteRegister(0x1F, 0x03) || !WriteRegister(0x01, 0x01)) {
            ESP_LOGE(kTag, "TCA8418 matrix setup failed");
            return false;
        }
        Flush();
        return true;
    }

    void Start() {
        if (xTaskCreate(Task, "tdeck_max_keypad", 3072, this, 4, nullptr) != pdPASS) {
            ESP_LOGE(kTag, "Failed to start keypad task");
        }
    }

private:
    static void Task(void* arg) {
        auto* keypad = static_cast<Tca8418Keypad*>(arg);
        while (true) {
            keypad->Poll();
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }

    bool ReadRegister(uint8_t reg, uint8_t* value) {
        return value != nullptr &&
               i2c_master_transmit_receive(device_, &reg, 1, value, 1, kI2cTimeoutMs) == ESP_OK;
    }

    bool WriteRegister(uint8_t reg, uint8_t value) {
        const uint8_t payload[] = {reg, value};
        return i2c_master_transmit(device_, payload, sizeof(payload), kI2cTimeoutMs) == ESP_OK;
    }

    void Flush() {
        for (uint8_t i = 0; i < 0x10; ++i) {
            uint8_t count = 0;
            if (!ReadRegister(0x03, &count) || (count & 0x0F) == 0) {
                break;
            }
            uint8_t event = 0;
            if (!ReadRegister(0x04, &event)) {
                break;
            }
        }
        WriteRegister(0x02, 0x03);
    }

    void Poll() {
        if (device_ == nullptr) {
            return;
        }

        uint8_t count = 0;
        if (!ReadRegister(0x03, &count)) {
            return;
        }
        count &= 0x0F;
        while (count-- != 0) {
            uint8_t event = 0;
            if (!ReadRegister(0x04, &event) || event == 0) {
                continue;
            }

            // The MAX factory firmware reports the active key with bit 7 set.
            const bool pressed = (event & 0x80) != 0;
            uint8_t key = static_cast<uint8_t>(event & 0x7F);
            if (key == 0 || key > T_DECK_MAX_KEYPAD_ROWS * T_DECK_MAX_KEYPAD_COLS) {
                continue;
            }
            --key;
            const uint8_t row = key / T_DECK_MAX_KEYPAD_COLS;
            const uint8_t col = (T_DECK_MAX_KEYPAD_COLS - 1) - key % T_DECK_MAX_KEYPAD_COLS;
            const char value = kKeyMap[row][col];

            if (pressed && (value == 'E' || value == ' ')) {
                Application::GetInstance().Schedule(
                    []() { Application::GetInstance().ToggleChatState(); });
            }
        }
    }

    static constexpr char kKeyMap[T_DECK_MAX_KEYPAD_ROWS][T_DECK_MAX_KEYPAD_COLS] = {
        {'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p'},
        {'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', '\b'},
        {'2', 'z', 'x', 'c', 'v', 'b', 'n', 'm', '$', 'E'},
        {'\0', '\0', '\0', '\0', '\0', 'U', '0', ' ', 'S', 'U'},
    };

    i2c_master_dev_handle_t device_ = nullptr;
};

constexpr char Tca8418Keypad::kKeyMap[T_DECK_MAX_KEYPAD_ROWS][T_DECK_MAX_KEYPAD_COLS];

class MaxEs8311AudioCodec final : public Es8311AudioCodec {
public:
    MaxEs8311AudioCodec(i2c_master_bus_handle_t bus, MaxXl9555& expander)
        : Es8311AudioCodec(bus, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                           T_DECK_MAX_ES8311_MCLK, T_DECK_MAX_ES8311_BCLK, T_DECK_MAX_ES8311_LRCK,
                           T_DECK_MAX_ES8311_DOUT, T_DECK_MAX_ES8311_DIN, GPIO_NUM_NC,
                           ES8311_CODEC_DEFAULT_ADDR, true, false),
          expander_(expander) {}

    void EnableOutput(bool enable) override {
        // LOW selects ES8311 and HIGH enables the external amplifier path.
        expander_.SetOutputState(T_DECK_MAX_XL9555_AUDIO_SEL, !enable);
        expander_.SetOutputState(T_DECK_MAX_XL9555_AMPLIFIER, enable);
        Es8311AudioCodec::EnableOutput(enable);
    }

private:
    MaxXl9555& expander_;
};

}  // namespace

class TDeckMaxBoard final : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    MaxXl9555 xl9555_;
    Cst328Touch touch_;
    Tca8418Keypad keypad_;
    Button boot_button_;
    bool touch_initialized_ = false;

    void InitializeI2c() {
        i2c_master_bus_config_t bus_config = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = T_DECK_MAX_I2C_SDA,
            .scl_io_num = T_DECK_MAX_I2C_SCL,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags =
                {
                    .enable_internal_pullup = 1,
                },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &i2c_bus_));
    }

    void InitializeXl9555() {
        if (!xl9555_.Initialize(i2c_bus_)) {
            ESP_LOGE(kTag,
                     "XL9555 initialization failed; dependent peripherals may be unavailable");
        }
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

    void InitializeKeypad() {
        gpio_config_t irq_config = {};
        irq_config.pin_bit_mask = 1ULL << T_DECK_MAX_KEYPAD_IRQ;
        irq_config.mode = GPIO_MODE_INPUT;
        irq_config.pull_up_en = GPIO_PULLUP_ENABLE;
        irq_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
        irq_config.intr_type = GPIO_INTR_DISABLE;
        gpio_config(&irq_config);

        gpio_config_t led_config = {};
        led_config.pin_bit_mask = 1ULL << T_DECK_MAX_KEYBOARD_LED;
        led_config.mode = GPIO_MODE_OUTPUT;
        led_config.pull_up_en = GPIO_PULLUP_DISABLE;
        led_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
        led_config.intr_type = GPIO_INTR_DISABLE;
        gpio_config(&led_config);
        gpio_set_level(T_DECK_MAX_KEYBOARD_LED, 0);

        if (keypad_.Initialize(i2c_bus_)) {
            keypad_.Start();
        }
    }

    void InitializeTouch() {
        if (touch_initialized_) {
            return;
        }

        xl9555_.SetOutputState(T_DECK_MAX_XL9555_TOUCH_RESET, false);
        vTaskDelay(pdMS_TO_TICKS(10));
        xl9555_.SetOutputState(T_DECK_MAX_XL9555_TOUCH_RESET, true);
        vTaskDelay(pdMS_TO_TICKS(50));

        gpio_config_t irq_config = {};
        irq_config.pin_bit_mask = 1ULL << T_DECK_MAX_TOUCH_IRQ;
        irq_config.mode = GPIO_MODE_INPUT;
        irq_config.pull_up_en = GPIO_PULLUP_ENABLE;
        irq_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
        irq_config.intr_type = GPIO_INTR_DISABLE;
        gpio_config(&irq_config);

        if (!touch_.Initialize(i2c_bus_)) {
            return;
        }

        if (!lvgl_port_lock(1000)) {
            ESP_LOGE(kTag, "Failed to lock LVGL while registering touch");
            return;
        }
        lv_indev_t* indev = lv_indev_create();
        if (indev != nullptr) {
            lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
            lv_indev_set_read_cb(indev, TouchInputReadCallback);
            lv_indev_set_user_data(indev, &touch_);
            lv_indev_set_disp(indev, lv_display_get_default());
            touch_initialized_ = true;
            ESP_LOGI(kTag, "CST328 touch initialized");
        }
        lvgl_port_unlock();
    }

    static void TouchInputReadCallback(lv_indev_t* indev, lv_indev_data_t* data) {
        auto* touch = static_cast<Cst328Touch*>(lv_indev_get_user_data(indev));
        int16_t x = 0;
        int16_t y = 0;
        if (touch != nullptr && touch->ReadPoint(&x, &y)) {
            data->state = LV_INDEV_STATE_PRESSED;
            data->point.x = x;
            data->point.y = y;
        } else {
            data->state = LV_INDEV_STATE_RELEASED;
        }
    }

public:
    TDeckMaxBoard() : boot_button_(GPIO_NUM_0) {
        InitializeI2c();
        InitializeXl9555();
        InitializeButtons();
        InitializeKeypad();
    }

    AudioCodec* GetAudioCodec() override {
        static MaxEs8311AudioCodec codec(i2c_bus_, xl9555_);
        return &codec;
    }

    Display* GetDisplay() override {
        static TDeckMaxEpdDisplay display;
        InitializeTouch();
        return &display;
    }

    Backlight* GetBacklight() override {
        static PwmBacklight backlight(T_DECK_MAX_EPD_BL, false);
        static std::once_flag restore_once;
        std::call_once(restore_once, []() { backlight.RestoreBrightness(); });
        return &backlight;
    }
};

DECLARE_BOARD(TDeckMaxBoard);
