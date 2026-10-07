#include "application.h"
#include "backlight.h"
#include "bq27220.h"
#include "button.h"
#include "codecs/es8311_audio_codec.h"
#include "config.h"
#include "display/lcd_display.h"
#include "esp_lcd_co5300.h"
#include "esp_lcd_touch_cst9220.h"
#include "esp_video.h"
#include "i2c_bus.h"
#include "led/gpio_led.h"
#include "lvgl_theme.h"
#include "mcp_server.h"
#include "settings.h"
#include "wifi_board.h"

#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <driver/spi_master.h>
#include <esp_check.h>
#include <esp_efuse.h>
#include <esp_efuse_table.h>
#include <esp_log.h>
#include <esp_lvgl_port.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <hal/usb_serial_jtag_ll.h>

#include <cstring>

#define TAG "EspMosaico"
#define LCD_OPCODE_WRITE_CMD (0x02ULL)

namespace {

enum class BoardVariant {
    kV1_0,
    kV1_2,
};

constexpr uint16_t HardwareVersion(uint8_t major, uint8_t minor) {
    return (static_cast<uint16_t>(major) << 8) | minor;
}

constexpr size_t kModuleEepromSize = 0x86;
constexpr int kModuleEepromTimeoutMs = 100;
constexpr size_t kModuleEepromDescCrcOffset = 0x34;
constexpr size_t kModuleEepromMfgCrcOffset = 0x3E;
constexpr size_t kModuleEepromParamCrcOffset = 0x84;
constexpr uint8_t kModuleTypeCamera = 0x07;

// Mosaico BSP's interim fixed-EDV profile, characterized for its 80 mAh cell.
static const parameter_cedv_t kBatteryCedv = {
    .full_charge_cap = 80,
    .design_cap = 80,
    .reserve_cap = 0,
    .near_full = 5,
    .self_discharge_rate = 20,
    .EDV0 = 3000,
    .EDV1 = 3410,
    .EDV2 = 3530,
    .EMF = 3670,
    .C0 = 115,
    .R0 = 968,
    .T0 = 4547,
    .R1 = 4764,
    .TC = 11,
    .C1 = 0,
    .DOD0 = 4147,
    .DOD10 = 4002,
    .DOD20 = 3969,
    .DOD30 = 3938,
    .DOD40 = 3880,
    .DOD50 = 3824,
    .DOD60 = 3794,
    .DOD70 = 3753,
    .DOD80 = 3677,
    .DOD90 = 3574,
    .DOD100 = 3490,
};

static const gauging_config_t kBatteryGauging = {
    .CCT = true,
    .SC = true,
    .FCC_LIM = true,
    .FC_FOR_VDQ = true,
    .IGNORE_SD = true,
};

uint16_t Crc16(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 1U) ? static_cast<uint16_t>((crc >> 1) ^ 0xA001U)
                             : static_cast<uint16_t>(crc >> 1);
        }
    }
    return crc;
}

uint16_t ReadLe16(const uint8_t* data) {
    return static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
}

bool IsCameraModuleDescriptor(const uint8_t* descriptor) {
    return std::memcmp(descriptor, "ESP", 3) == 0 && descriptor[3] == kModuleTypeCamera &&
           ReadLe16(descriptor + kModuleEepromDescCrcOffset) ==
               Crc16(descriptor, kModuleEepromDescCrcOffset) &&
           ReadLe16(descriptor + kModuleEepromMfgCrcOffset) ==
               Crc16(descriptor + 0x36, kModuleEepromMfgCrcOffset - 0x36) &&
           ReadLe16(descriptor + kModuleEepromParamCrcOffset) ==
               Crc16(descriptor + 0x40, kModuleEepromParamCrcOffset - 0x40);
}

static const co5300_lcd_init_cmd_t kCo5300InitCommands[] = {
    {0x11, nullptr, 0, 600},
    {0xFE, (uint8_t[]){0x20}, 1, 0},
    {0x19, (uint8_t[]){0x10}, 1, 0},
    {0x1C, (uint8_t[]){0xA0}, 1, 0},
    {0xFE, (uint8_t[]){0x00}, 1, 0},
    {0xC4, (uint8_t[]){0x80}, 1, 0},
    {0x3A, (uint8_t[]){0x55}, 1, 0},
    {0x35, (uint8_t[]){0x00}, 1, 0},
    {0x53, (uint8_t[]){0x20}, 1, 0},
    {0x51, (uint8_t[]){0xFF}, 1, 0},
    {0x63, (uint8_t[]){0xFF}, 1, 0},
    {0x2A, (uint8_t[]){0x00, 0x00, 0x01, 0xDF}, 4, 0},
    {0x2B, (uint8_t[]){0x00, 0x00, 0x01, 0xDF}, 4, 0},
    {0x29, nullptr, 0, 600},
};

}  // namespace

class MosaicoBacklight : public Backlight {
public:
    MosaicoBacklight(esp_lcd_panel_io_handle_t panel_io, Display* display)
        : panel_io_(panel_io), display_(display) {}

protected:
    void SetBrightnessImpl(uint8_t brightness) override {
        DisplayLockGuard lock(display_);
        const uint8_t data[] = {static_cast<uint8_t>((255 * brightness) / 100)};
        const int command = (0x51 << 8) | (LCD_OPCODE_WRITE_CMD << 24);
        ESP_ERROR_CHECK_WITHOUT_ABORT(
            esp_lcd_panel_io_tx_param(panel_io_, command, data, sizeof(data)));
    }

private:
    esp_lcd_panel_io_handle_t panel_io_;
    Display* display_;
};

class MosaicoDisplay : public SpiLcdDisplay {
public:
    using SpiLcdDisplay::SpiLcdDisplay;

    void SetupUI() override {
        SpiLcdDisplay::SetupUI();

        // Use dark on a fresh device, while preserving a user-selected theme.
        Settings settings("display", false);
        if (settings.GetString("theme").empty()) {
            auto* dark_theme = LvglThemeManager::GetInstance().GetTheme("dark");
            if (dark_theme != nullptr) {
                SetTheme(dark_theme);
            }
        }

        DisplayLockGuard lock(this);
        // Keep Wi-Fi, mute, and battery icons within the rounded-screen safe area.
        lv_obj_set_style_pad_left(top_bar_, DISPLAY_TOP_BAR_HORIZONTAL_INSET, 0);
        lv_obj_set_style_pad_right(top_bar_, DISPLAY_TOP_BAR_HORIZONTAL_INSET, 0);
    }
};

class EspMosaicoBoard : public WifiBoard {
private:
    BoardVariant variant_ = BoardVariant::kV1_0;
    i2c_bus_handle_t shared_i2c_bus_ = nullptr;
    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    i2c_master_bus_handle_t camera_i2c_bus_ = nullptr;
    Button ai_button_;
    MosaicoDisplay* display_ = nullptr;
    MosaicoBacklight* backlight_ = nullptr;
    esp_lcd_touch_handle_t touch_ = nullptr;
    esp_lcd_panel_io_handle_t touch_io_ = nullptr;
    lv_indev_t* touch_indev_ = nullptr;
    EspVideo* camera_ = nullptr;
    bq27220_handle_t battery_ = nullptr;

    bool IsV1_0() const { return variant_ == BoardVariant::kV1_0; }

    esp_err_t DetectBoardVariant() {
        uint16_t version = 0;
        ESP_RETURN_ON_ERROR(
            esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA, &version, sizeof(version) * 8), TAG,
            "read hardware version from eFuse failed");
        switch (version) {
            case HardwareVersion(1, 0):
                variant_ = BoardVariant::kV1_0;
                break;
            case HardwareVersion(1, 1):
            case HardwareVersion(1, 2):
                variant_ = BoardVariant::kV1_2;
                break;
            default:
                ESP_LOGE(TAG, "unsupported ESP-Mosaico hardware version 0x%04X", version);
                return ESP_ERR_NOT_SUPPORTED;
        }
        ESP_LOGI(TAG, "ESP-Mosaico v%u.%u detected", version >> 8, version & 0xFF);
        return ESP_OK;
    }

    void ConfigureOutput(gpio_num_t pin, int level, gpio_mode_t mode = GPIO_MODE_OUTPUT) {
        ESP_ERROR_CHECK(gpio_set_level(pin, level));
        const gpio_config_t config = {
            .pin_bit_mask = BIT64(pin),
            .mode = mode,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&config));
    }

    void InitializeCameraFlash() {
        const gpio_config_t config = {
            .pin_bit_mask = BIT64(CAMERA_FLASH_GPIO),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_set_level(CAMERA_FLASH_GPIO, 1));
        ESP_ERROR_CHECK(gpio_config(&config));
    }

    void InitializePower() {
        ESP_ERROR_CHECK(DetectBoardVariant());
        ConfigureOutput(POWER_VCC_3V3_GPIO, 0);  // VCC_PW is active low.
        if (IsV1_0()) {
            ConfigureOutput(POWER_CODEC_3V3_V1_0_GPIO, 1);
        }
        ConfigureOutput(POWER_SHUTDOWN_GPIO, 1, GPIO_MODE_OUTPUT_OD);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    void InitializeI2c() {
        const i2c_config_t config = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = IsV1_0() ? I2C_SDA_V1_0 : I2C_SDA_V1_2,
            .scl_io_num = IsV1_0() ? I2C_SCL_V1_0 : I2C_SCL_V1_2,
            .sda_pullup_en = true,
            .scl_pullup_en = true,
            .master = {.clk_speed = BATTERY_I2C_SPEED_HZ},
            .clk_flags = 0,
        };
        shared_i2c_bus_ = i2c_bus_create(I2C_NUM_0, &config);
        if (shared_i2c_bus_ == nullptr) {
            ESP_LOGE(TAG, "create shared I2C bus failed");
            ESP_ERROR_CHECK(ESP_FAIL);
        }
        i2c_bus_ = i2c_bus_get_internal_bus_handle(shared_i2c_bus_);

        if (i2c_bus_ == nullptr) {
            ESP_LOGE(TAG, "get shared I2C master handle failed");
            ESP_ERROR_CHECK(ESP_FAIL);
        }
    }

    void InitializeBattery() {
        const bq27220_config_t config = {
            .i2c_bus = shared_i2c_bus_,
            .cfg = &kBatteryGauging,
            .cedv = &kBatteryCedv,
        };
        battery_ = bq27220_create(&config);
        if (battery_ == nullptr) {
            ESP_LOGW(TAG, "BQ27220 at 0x%02X not detected; battery reporting disabled",
                     BATTERY_BQ27220_I2C_ADDR);
            return;
        }
        const esp_err_t err = bq27220_seal(battery_);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "seal BQ27220 failed: %s; battery reporting disabled",
                     esp_err_to_name(err));
            ESP_ERROR_CHECK_WITHOUT_ABORT(bq27220_delete(battery_));
            battery_ = nullptr;
            return;
        }
        ESP_LOGI(TAG, "BQ27220 ready: %u mV", bq27220_get_voltage(battery_));
    }

    void InitializeButtons() {
        ai_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
    }

    void InitializeDisplay() {
        const gpio_num_t sclk = IsV1_0() ? DISPLAY_QSPI_SCLK_V1_0 : DISPLAY_QSPI_SCLK_V1_2;
        const gpio_num_t reset = IsV1_0() ? DISPLAY_QSPI_RST_V1_0 : DISPLAY_QSPI_RST_V1_2;
        const spi_bus_config_t bus_config = {
            .data0_io_num = DISPLAY_QSPI_D0,
            .data1_io_num = DISPLAY_QSPI_D1,
            .sclk_io_num = sclk,
            .data2_io_num = DISPLAY_QSPI_D2,
            .data3_io_num = DISPLAY_QSPI_D3,
            .data4_io_num = GPIO_NUM_NC,
            .data5_io_num = GPIO_NUM_NC,
            .data6_io_num = GPIO_NUM_NC,
            .data7_io_num = GPIO_NUM_NC,
            .max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t),
            .flags = SPICOMMON_BUSFLAG_QUAD,
        };
        ESP_ERROR_CHECK(spi_bus_initialize(DISPLAY_QSPI_HOST, &bus_config, SPI_DMA_CH_AUTO));

        esp_lcd_panel_io_handle_t panel_io = nullptr;
        const esp_lcd_panel_io_spi_config_t io_config = {
            .cs_gpio_num = DISPLAY_QSPI_CS,
            .dc_gpio_num = GPIO_NUM_NC,
            .spi_mode = 0,
            .pclk_hz = 40 * 1000 * 1000,
            .trans_queue_depth = 10,
            .lcd_cmd_bits = 32,
            .lcd_param_bits = 8,
            .flags = {.quad_mode = true, .psram_dma_direct = true},
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(DISPLAY_QSPI_HOST, &io_config, &panel_io));

        co5300_vendor_config_t vendor_config = {
            .init_cmds = kCo5300InitCommands,
            .init_cmds_size = sizeof(kCo5300InitCommands) / sizeof(kCo5300InitCommands[0]),
            .flags = {.use_qspi_interface = true},
        };
        esp_lcd_panel_handle_t panel = nullptr;
        const esp_lcd_panel_dev_config_t panel_config = {
            .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
            .bits_per_pixel = 16,
            .reset_gpio_num = reset,
            .vendor_config = &vendor_config,
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_co5300(panel_io, &panel_config, &panel));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

        display_ = new MosaicoDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                      DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X,
                                      DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
        backlight_ = new MosaicoBacklight(panel_io, display_);
        backlight_->RestoreBrightness();
    }

    void InitializeTouch() {
        const esp_lcd_touch_config_t touch_config = {
            .x_max = DISPLAY_WIDTH - 1,
            .y_max = DISPLAY_HEIGHT - 1,
            .rst_gpio_num = GPIO_NUM_NC,
            .int_gpio_num = TOUCH_INT_PIN,
            .levels = {.reset = 0, .interrupt = 0},
            .flags = {.swap_xy = false, .mirror_x = false, .mirror_y = false},
        };
        const esp_lcd_panel_io_i2c_config_t io_config = {
            .dev_addr = ESP_LCD_TOUCH_IO_I2C_CST9220_ADDRESS,
            .scl_speed_hz = 400 * 1000,
            .control_phase_bytes = 1,
            .dc_bit_offset = 0,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .flags = {.disable_control_phase = true},
            .transaction_timeout_ms = 100,
        };
        esp_err_t err = esp_lcd_new_panel_io_i2c(i2c_bus_, &io_config, &touch_io_);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "create CST9220 panel IO failed: %s; continuing without touch",
                     esp_err_to_name(err));
            return;
        }
        err = esp_lcd_touch_new_i2c_cst9220(touch_io_, &touch_config, &touch_);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "initialize CST9220 failed: %s; continuing without touch",
                     esp_err_to_name(err));
            ESP_ERROR_CHECK(esp_lcd_panel_io_del(touch_io_));
            touch_io_ = nullptr;
            return;
        }
        const lvgl_port_touch_cfg_t port_config = {
            .disp = lv_display_get_default(),
            .handle = touch_,
        };
        touch_indev_ = lvgl_port_add_touch(&port_config);
    }

    void InitializeCameraI2c() {
        if (IsV1_0()) {
            camera_i2c_bus_ = i2c_bus_;
            return;
        }
        const i2c_master_bus_config_t config = {
            .i2c_port = CAMERA_I2C_PORT_V1_2,
            .sda_io_num = CAMERA_I2C_SDA,
            .scl_io_num = CAMERA_I2C_SCL,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {.enable_internal_pullup = 1},
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&config, &camera_i2c_bus_));
    }

    bool DetectCameraModule() {
        ConfigureOutput(CAMERA_EEPROM_ADDR_SELECT_LEFT_GPIO, 0);
        ConfigureOutput(CAMERA_EEPROM_ADDR_SELECT_RIGHT_GPIO, 1);

        const i2c_device_config_t device_config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = CAMERA_EEPROM_I2C_ADDR,
            .scl_speed_hz = CAMERA_SCCB_FREQ_HZ,
        };
        i2c_master_dev_handle_t eeprom = nullptr;
        esp_err_t err = i2c_master_bus_add_device(camera_i2c_bus_, &device_config, &eeprom);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "attach left expansion EEPROM failed: %s", esp_err_to_name(err));
            return false;
        }

        uint8_t descriptor[kModuleEepromSize] = {};
        const uint8_t offset = 0;
        // The I2C master API takes milliseconds and converts to ticks internally.
        err = i2c_master_transmit_receive(eeprom, &offset, sizeof(offset), descriptor,
                                          sizeof(descriptor), kModuleEepromTimeoutMs);
        const esp_err_t remove_err = i2c_master_bus_rm_device(eeprom);
        if (remove_err != ESP_OK) {
            ESP_LOGW(TAG, "detach left expansion EEPROM failed: %s", esp_err_to_name(remove_err));
        }
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "read left expansion EEPROM at 0x%02X failed: %s; camera disabled",
                     CAMERA_EEPROM_I2C_ADDR, esp_err_to_name(err));
            return false;
        }
        if (!IsCameraModuleDescriptor(descriptor)) {
            ESP_LOGW(TAG, "left expansion descriptor is not a valid CameraBoard");
            return false;
        }
        ESP_LOGI(TAG, "CameraBoard detected in left expansion slot");
        return true;
    }

    void InitializeCamera() {
        InitializeCameraI2c();
        if (!DetectCameraModule()) {
            return;
        }

        // CameraBoard flash is active low; keep it off while streaming.
        // Use dedicated initialization to keep the internal pull-up enabled.
        InitializeCameraFlash();

        // Camera D2 is GPIO33, shared with USB Serial/JTAG. This build uses
        // the UART console, and JTAG must release the pad before DVP starts.
        usb_serial_jtag_ll_disable_intr_mask(USB_SERIAL_JTAG_LL_INTR_MASK);
        usb_serial_jtag_ll_phy_enable_pad(false);
        usb_serial_jtag_ll_enable_bus_clock(false);

        const esp_cam_ctlr_dvp_pin_config_t dvp_pins = {
            .data_width = CAM_CTLR_DATA_WIDTH_8,
            .data_io = {CAMERA_PIN_D0, CAMERA_PIN_D1, CAMERA_PIN_D2, CAMERA_PIN_D3, CAMERA_PIN_D4,
                        CAMERA_PIN_D5, CAMERA_PIN_D6, CAMERA_PIN_D7},
            .vsync_io = CAMERA_PIN_VSYNC,
            .de_io = CAMERA_PIN_HREF,
            .pclk_io = CAMERA_PIN_PCLK,
            .xclk_io = CAMERA_PIN_XCLK,
        };
        const esp_video_init_sccb_config_t sccb_config = {
            .init_sccb = false,
            .i2c_handle = camera_i2c_bus_,
            .freq = CAMERA_SCCB_FREQ_HZ,
        };
        const esp_video_init_dvp_config_t dvp_config = {
            .sccb_config = sccb_config,
            .reset_pin = CAMERA_PIN_RESET,
            .pwdn_pin = CAMERA_PIN_PWDN,
            .dvp_pin = dvp_pins,
            .xclk_freq = 0,
        };
        const esp_video_init_config_t video_config = {
            .dvp = &dvp_config,
        };
        camera_ = new EspVideo(video_config);
    }

    void InitializeTools() {
        if (camera_ == nullptr) {
            return;
        }

        McpServer::GetInstance().AddTool(
            "self.camera.set_flash", "Turn the CameraBoard flash on or off.",
            PropertyList({Property("enabled", kPropertyTypeBoolean)}),
            [this](const PropertyList& properties) -> ToolResult {
                const bool enabled = properties["enabled"].value<bool>();
                const esp_err_t err = gpio_set_level(CAMERA_FLASH_GPIO, enabled ? 0 : 1);
                if (err != ESP_OK) {
                    return std::unexpected("Failed to set CameraBoard flash: " +
                                           std::string(esp_err_to_name(err)));
                }
                return true;
            });
    }

public:
    EspMosaicoBoard() : ai_button_(AI_BUTTON_GPIO) {
        InitializePower();
        InitializeI2c();
        InitializeBattery();
        InitializeButtons();
        InitializeDisplay();
        InitializeTouch();
        InitializeCamera();
        InitializeTools();
    }

    AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(
            i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN, AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

    Display* GetDisplay() override { return display_; }
    Backlight* GetBacklight() override { return backlight_; }
    Camera* GetCamera() override { return camera_; }

    bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        if (battery_ == nullptr) {
            level = 0;
            charging = false;
            discharging = false;
            return false;
        }
        const int16_t current_ma = bq27220_get_current(battery_);
        charging = current_ma > 0;
        discharging = current_ma < 0;
        level = bq27220_get_state_of_charge(battery_);
        return true;
    }

    Led* GetLed() override {
        if (!IsV1_0()) {
            return Board::GetLed();
        }
        static GpioLed led(STATUS_LED_V1_0_GPIO, true);
        return &led;
    }
};

DECLARE_BOARD(EspMosaicoBoard);
