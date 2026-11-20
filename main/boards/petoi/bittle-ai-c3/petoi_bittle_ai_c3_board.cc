#include "application.h"
#include "button.h"
#include "codecs/es8311_audio_codec.h"
#include "config.h"
#include "display/lcd_display.h"
#include "mcp_server.h"
#include "robot_uart.h"
#include "wifi_board.h"
#if CONFIG_PETOI_RUNTIME_LOCALE
#include "runtime_locale.h"
#endif
#if CONFIG_PETOI_ACTION_SEQUENCES
#include "robot_controller.h"
#include "robot_language.h"
#include "robot_tool_description.h"
using PetoiRobot = petoi::RobotController;
#else
using PetoiRobot = petoi::RobotUart;
#endif
#if CONFIG_PETOI_EXPRESSION_UI
#include "expression_display.h"
using PetoiBoardDisplay = PetoiExpressionDisplay;
#else
using PetoiBoardDisplay = SpiLcdDisplay;
#endif

#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>

#define TAG "PetoiBittleAiC3Board"

class PetoiBittleAiC3Board : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;
    Button boot_button_;
    PetoiBoardDisplay* display_ = nullptr;
    PetoiRobot robot_uart_;
#if CONFIG_PETOI_RUNTIME_LOCALE
    petoi::RuntimeLocale locale_;
#endif

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
            .flags =
                {
                    .enable_internal_pullup = 1,
                },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
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

    void InitializeSt7789Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_SPI_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = 0;
        io_config.pclk_hz = 20 * 1000 * 1000;
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

        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));

        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));
        display_ = new PetoiBoardDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                         DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X,
                                         DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

#if CONFIG_PETOI_EXPRESSION_UI
    void InitializeExpressionTools() {
        auto& mcp = McpServer::GetInstance();
        mcp.AddTool(
            "self.expression.look",
            "Move only the LCD eyes, not the robot head. direction: center/up/down/left/right.",
            PropertyList({Property("direction", kPropertyTypeString).SetMaxLength(16)}),
            [this](const PropertyList& p) -> ToolResult {
                display_->Look(p["direction"].value<std::string>());
                return true;
            });
        mcp.AddTool("self.expression.blink", "Blink LCD eyes; times 0=random, 1=once, 2=twice.",
                    PropertyList({Property("times", kPropertyTypeInteger, 0, 0, 2)}),
                    [this](const PropertyList& p) -> ToolResult {
                        display_->Blink(p["times"].value<int>());
                        return true;
                    });
        mcp.AddTool("self.expression.set_mouth_width", "Set LCD mouth width; 0 restores animation.",
                    PropertyList({Property("width", kPropertyTypeInteger, 0, 0, 240)}),
                    [this](const PropertyList& p) -> ToolResult {
                        display_->SetMouthWidth(p["width"].value<int>());
                        return true;
                    });
        mcp.AddTool("self.expression.set_quiet_after_speech",
                    "Narrow the LCD mouth after speech; does not mute the speaker.",
                    PropertyList({Property("enabled", kPropertyTypeBoolean, true)}),
                    [this](const PropertyList& p) -> ToolResult {
                        display_->SetQuietAfterSpeech(p["enabled"].value<bool>());
                        return true;
                    });
        mcp.AddTool("self.expression.set_mouth_style",
                    "LCD mouth style: wide/narrow/closed/default.",
                    PropertyList({Property("style", kPropertyTypeString).SetMaxLength(16)}),
                    [this](const PropertyList& p) -> ToolResult {
                        const auto style = p["style"].value<std::string>();
                        if (style == "closed")
                            display_->SetQuietAfterSpeech(true);
                        else if (style == "wide" || style == "narrow" || style == "default") {
                            display_->SetQuietAfterSpeech(false);
                            if (style == "default")
                                display_->SetMouthWidth(0);
                            else
                                display_->SetTemporaryMouthWidth(style == "wide" ? 170 : 52);
                        } else
                            return std::unexpected(std::string("Unknown mouth style"));
                        return true;
                    });
    }
#endif

    void InitializeRobotTools() {
        ESP_ERROR_CHECK(robot_uart_.Initialize());
        auto& mcp = McpServer::GetInstance();
#if CONFIG_PETOI_ACTION_SEQUENCES
        mcp.AddAsyncTool(
            "self.robot.send_command", petoi::kRobotToolDescription,
            PropertyList(
                {Property("text", kPropertyTypeString).SetMaxLength(petoi::kMaxCommandLength)}),
            [this](const PropertyList& properties, const McpRequestContext& context,
                   ToolCompletion completion) {
                robot_uart_.Submit(
                    properties["text"].value<std::string>(), context.id_json,
                    context.direct_transport,
                    [completion = std::move(completion)](petoi::RobotResult result) mutable {
                        if (result.ok()) {
                            completion(
                                std::string(result.code == petoi::RobotResultCode::TokenObserved
                                                ? "NyBoard completion token observed; commands="
                                                : "UART transmitted; physical completion not "
                                                  "tracked; commands=") +
                                std::to_string(result.completed));
                        } else {
                            const char* reason =
                                result.code == petoi::RobotResultCode::Timeout
                                    ? "Completion token timeout; the last command may already have "
                                      "executed"
                                : result.code == petoi::RobotResultCode::TxError
                                    ? "UART transmission failed; partial data may have been sent"
                                : result.code == petoi::RobotResultCode::Cancelled
                                    ? "Sequence cancelled; no further commands will be sent; an "
                                      "in-flight action may continue"
                                    : "Command rejected; no extra transmission was queued";
                            completion(std::unexpected(
                                std::string(reason) +
                                "; remaining actions stopped; do not automatically retry"));
                        }
                    });
            });
#else
        mcp.AddTool(
            "self.robot.send_command",
            "Send one ASCII Petoi NyBoard command, for example kup (stand), ksit (sit), "
            "or d (rest). The text parameter contains only the command, without a prefix "
            "or newline. Accepted means queued for transmission, not action completion. "
            "Never automatically retry an accepted command.",
            PropertyList(
                {Property("text", kPropertyTypeString).SetMaxLength(petoi::kMaxCommandLength)}),
            [this](const PropertyList& properties) -> ToolResult {
                const auto error = robot_uart_.Enqueue(properties["text"].value<std::string>());
                if (error != ESP_OK) {
                    return std::unexpected(std::string("Robot command rejected: ") +
                                           esp_err_to_name(error));
                }
                return std::string(
                    "accepted for UART transmission; action completion is not tracked");
            });
#endif
        mcp.AddTool("self.robot.get_status",
                    "Read the robot UART queue and last transmission error.", PropertyList(),
                    [this](const PropertyList&) -> ToolResult { return robot_uart_.GetStatus(); });
    }

public:
    PetoiBittleAiC3Board() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        InitializeSpi();
        InitializeSt7789Display();
        InitializeButtons();
        InitializeRobotTools();
#if CONFIG_PETOI_EXPRESSION_UI
        InitializeExpressionTools();
#endif
        GetBacklight()->SetBrightness(100);
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(
            codec_i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN, AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override { return display_; }

#if CONFIG_PETOI_RUNTIME_LOCALE
    void PrepareNetwork() override { locale_.Prepare(GetNetwork()); }
    const char* GetRuntimeLanguage() const override { return locale_.Code(); }
    const char* GetWakeWordModelHint() const override { return locale_.WakeModelHint(); }
    const char* LocalizeText(const char* text) const override { return locale_.Text(text); }
    std::string_view LocalizeSound(std::string_view sound) const override {
        return locale_.Sound(sound);
    }
#endif

    void OnUserTranscript(const std::string& text) override {
        (void)text;
#if CONFIG_PETOI_ACTION_SEQUENCES
        std::string commands;
#if CONFIG_PETOI_LOCAL_ACTIONS
        petoi::BuildLocalActionText(text, commands);
#endif
        robot_uart_.BeginTurn(commands);
#else
        (void)text;
#endif
    }

    void SetPowerSaveLevel(PowerSaveLevel level) override {
        // Keep modem sleep disabled while qualifying this board's network path.
        WifiBoard::SetPowerSaveLevel(
            level == PowerSaveLevel::LOW_POWER ? PowerSaveLevel::PERFORMANCE : level);
    }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }
};

DECLARE_BOARD(PetoiBittleAiC3Board);
