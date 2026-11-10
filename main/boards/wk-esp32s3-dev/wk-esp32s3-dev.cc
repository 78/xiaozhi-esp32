#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "display/lcd_display.h"
#include "display/oled_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "board.h"
#include "mcp_server.h"
#include "cjson_utils.h"
#include "lamp_controller.h"
#include "led/single_led.h"
#include "assets/lang_config.h"
#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <driver/spi_common.h>
#include <atomic>
#include <memory>
#include <string>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>


#if defined(LCD_TYPE_ILI9341_SERIAL)
#include "esp_lcd_ili9341.h"
#endif

#if defined(LCD_TYPE_NV3030B_SERIAL)
#include "esp_lcd_nv3030b.h"
#endif

#if defined(LCD_TYPE_ILI9486_SERIAL)
#include "esp_lcd_ili9486.h"
#endif

#if defined(LCD_TYPE_GC9A01_SERIAL)
#include "esp_lcd_gc9a01.h"
static const gc9a01_lcd_init_cmd_t gc9107_lcd_init_cmds[] = {
    //  {cmd, { data }, data_size, delay_ms}
    {0xfe, (uint8_t[]){0x00}, 0, 0},
    {0xef, (uint8_t[]){0x00}, 0, 0},
    {0xb0, (uint8_t[]){0xc0}, 1, 0},
    {0xb1, (uint8_t[]){0x80}, 1, 0},
    {0xb2, (uint8_t[]){0x27}, 1, 0},
    {0xb3, (uint8_t[]){0x13}, 1, 0},
    {0xb6, (uint8_t[]){0x19}, 1, 0},
    {0xb7, (uint8_t[]){0x05}, 1, 0},
    {0xac, (uint8_t[]){0xc8}, 1, 0},
    {0xab, (uint8_t[]){0x0f}, 1, 0},
    {0x3a, (uint8_t[]){0x05}, 1, 0},
    {0xb4, (uint8_t[]){0x04}, 1, 0},
    {0xa8, (uint8_t[]){0x08}, 1, 0},
    {0xb8, (uint8_t[]){0x08}, 1, 0},
    {0xea, (uint8_t[]){0x02}, 1, 0},
    {0xe8, (uint8_t[]){0x2A}, 1, 0},
    {0xe9, (uint8_t[]){0x47}, 1, 0},
    {0xe7, (uint8_t[]){0x5f}, 1, 0},
    {0xc6, (uint8_t[]){0x21}, 1, 0},
    {0xc7, (uint8_t[]){0x15}, 1, 0},
    {0xf0,
    (uint8_t[]){0x1D, 0x38, 0x09, 0x4D, 0x92, 0x2F, 0x35, 0x52, 0x1E, 0x0C,
                0x04, 0x12, 0x14, 0x1f},
    14, 0},
    {0xf1,
    (uint8_t[]){0x16, 0x40, 0x1C, 0x54, 0xA9, 0x2D, 0x2E, 0x56, 0x10, 0x0D,
                0x0C, 0x1A, 0x14, 0x1E},
    14, 0},
    {0xf4, (uint8_t[]){0x00, 0x00, 0xFF}, 3, 0},
    {0xba, (uint8_t[]){0xFF, 0xFF}, 2, 0},
};
#endif
 
#define TAG "WkEsp32s3Dev"

namespace {

// Optional Lila features (web search + Telegram). Both tools are enabled at
// build time through Kconfig string values, see main/Kconfig.projbuild. They
// are registered only when the matching credentials are not empty, so a stock
// build exposes neither tool.
constexpr int kLilaHttpTimeoutMs = 15000;
constexpr uint32_t kLilaTelegramTaskStack = 6144;
constexpr UBaseType_t kLilaTelegramTaskPriority = 2;
constexpr size_t kLilaSearchSnippetLength = 400;

// Serialize a cJSON tree and release it. Returns an empty string on failure.
std::string LilaJsonToString(cJSON* json) {
    if (json == nullptr) {
        return std::string();
    }
    CJsonStringUniquePtr text(cJSON_PrintUnformatted(json));
    cJSON_Delete(json);
    return text != nullptr ? std::string(text.get()) : std::string();
}

// Perform a JSON POST request. Returns false when the request could not be
// completed; the HTTP status code and body are returned through the out params.
// `label` is logged instead of the URL because provider URLs may embed secrets
// (Telegram bot URLs contain the bot token).
bool LilaHttpPostJson(const std::string& label, const std::string& url, const std::string& body,
                      int& status_code, std::string& response) {
    auto http = Board::GetInstance().GetNetwork()->CreateHttp(3);
    if (http == nullptr) {
        ESP_LOGE(TAG, "Failed to create HTTP client for %s", label.c_str());
        return false;
    }

    http->SetTimeout(kLilaHttpTimeoutMs);
    http->SetHeader("Content-Type", "application/json");
    http->SetHeader("Accept", "application/json");
    std::string request = body;
    http->SetContent(std::move(request));

    auto opened = http->Open("POST", url);
    if (!opened) {
        ESP_LOGE(TAG, "Failed to open %s: %s", label.c_str(), opened.error().ToString().c_str());
        return false;
    }

    auto status = http->GetStatusCode();
    if (!status) {
        ESP_LOGE(TAG, "Failed to read HTTP status for %s: %s", label.c_str(),
                 status.error().ToString().c_str());
        http->Close();
        return false;
    }
    status_code = *status;
    response = http->ReadAll();
    http->Close();
    return true;
}

// Web search through Tavily (https://tavily.com).
ToolResult LilaWebSearch(const std::string& query, int max_results) {
    if (CONFIG_XIAOZHI_TAVILY_API_KEY[0] == '\0') {
        return std::unexpected(
            "Tavily API key is not configured. Set it in menuconfig and rebuild.");
    }

    // Allow only a single in-flight search.
    static std::atomic<bool> search_in_progress{false};
    bool expected = false;
    if (!search_in_progress.compare_exchange_strong(expected, true)) {
        return std::unexpected("A Tavily search is already in progress");
    }
    struct InProgressGuard {
        std::atomic<bool>* flag;
        ~InProgressGuard() { flag->store(false); }
    } guard{&search_in_progress};

    cJSON* request = cJSON_CreateObject();
    if (request == nullptr) {
        return std::unexpected("Failed to allocate Tavily request");
    }
    cJSON_AddStringToObject(request, "api_key", CONFIG_XIAOZHI_TAVILY_API_KEY);
    cJSON_AddStringToObject(request, "query", query.c_str());
    cJSON_AddNumberToObject(request, "max_results", max_results);
    cJSON_AddStringToObject(request, "search_depth", "basic");
    std::string body = LilaJsonToString(request);

    int status_code = 0;
    std::string response;
    if (!LilaHttpPostJson("Tavily API", "https://api.tavily.com/search", body, status_code,
                          response)) {
        return std::unexpected("Failed to reach Tavily");
    }
    if (status_code < 200 || status_code >= 300) {
        ESP_LOGE(TAG, "Tavily search failed with status %d: %s", status_code, response.c_str());
        return std::unexpected("Tavily search failed with status " + std::to_string(status_code));
    }

    CJsonUniquePtr parsed(cJSON_Parse(response.c_str()));
    if (parsed == nullptr) {
        return std::unexpected("Failed to parse Tavily response");
    }

    std::string output;
    cJSON* answer = cJSON_GetObjectItem(parsed.get(), "answer");
    if (cJSON_IsString(answer) && answer->valuestring[0] != '\0') {
        output += answer->valuestring;
        output += "\n\n";
    }

    cJSON* results = cJSON_GetObjectItem(parsed.get(), "results");
    if (cJSON_IsArray(results)) {
        int index = 1;
        cJSON* item = nullptr;
        cJSON_ArrayForEach(item, results) {
            cJSON* title = cJSON_GetObjectItem(item, "title");
            cJSON* url = cJSON_GetObjectItem(item, "url");
            cJSON* content = cJSON_GetObjectItem(item, "content");
            output += std::to_string(index) + ". ";
            output += cJSON_IsString(title) ? title->valuestring : "(untitled)";
            output += "\n";
            if (cJSON_IsString(url)) {
                output += url->valuestring;
                output += "\n";
            }
            if (cJSON_IsString(content)) {
                std::string snippet(content->valuestring);
                if (snippet.size() > kLilaSearchSnippetLength) {
                    snippet.resize(kLilaSearchSnippetLength);
                    snippet += "...";
                }
                output += snippet;
                output += "\n";
            }
            output += "\n";
            ++index;
        }
    }

    if (output.empty()) {
        return std::string("No results found.");
    }
    return output;
}

// A Telegram message is delivered from a short-lived worker task so the main
// task (which runs MCP tool callbacks) never blocks on the network.
struct LilaTelegramJob {
    std::string text;
};

void LilaTelegramTask(void* arg) {
    std::unique_ptr<LilaTelegramJob> job(static_cast<LilaTelegramJob*>(arg));

    cJSON* request = cJSON_CreateObject();
    if (request != nullptr) {
        cJSON_AddStringToObject(request, "chat_id", CONFIG_XIAOZHI_TELEGRAM_CHAT_ID);
        cJSON_AddStringToObject(request, "text", job->text.c_str());
        cJSON_AddBoolToObject(request, "disable_web_page_preview", true);
        std::string body = LilaJsonToString(request);

        std::string url = std::string("https://api.telegram.org/bot") +
                          CONFIG_XIAOZHI_TELEGRAM_BOT_TOKEN + "/sendMessage";
        int status_code = 0;
        std::string response;
        if (LilaHttpPostJson("Telegram API", url, body, status_code, response)) {
            if (status_code >= 200 && status_code < 300) {
                ESP_LOGI(TAG, "Telegram message sent");
            } else {
                ESP_LOGE(TAG, "Telegram API returned status %d: %s", status_code,
                         response.c_str());
            }
        }
    }

    vTaskDelete(nullptr);
}

}  // namespace

class WkEsp32s3Dev : public WifiBoard {
private:
 
    Button boot_button_;
    // LcdDisplay* display_;
    Display* display_ = nullptr;
    i2c_master_bus_handle_t display_i2c_bus_;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    Button volume_up_button_;
    Button volume_down_button_;

#if CONFIG_WK_ESP32S3_DEV_DISPLAY_OLED
  void InitializeDisplayI2c() {
        i2c_master_bus_config_t bus_config = {
            .i2c_port = (i2c_port_t)0,
            .sda_io_num = DISPLAY_SDA_PIN,
            .scl_io_num = DISPLAY_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &display_i2c_bus_));
    }

    void InitializeSsd1306Display() {
        // SSD1306 config
        // IDF 5.5 and 6.x declare these fields in a different order. Assign
        // them individually so C++ designated-initializer ordering is irrelevant.
        esp_lcd_panel_io_i2c_config_t io_config = {};
        io_config.dev_addr = 0x3C;
        io_config.scl_speed_hz = 400 * 1000;
        io_config.control_phase_bytes = 1;
        io_config.dc_bit_offset = 6;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        io_config.on_color_trans_done = nullptr;
        io_config.user_ctx = nullptr;
        io_config.flags.dc_low_on_data = 0;
        io_config.flags.disable_control_phase = 0;

        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(display_i2c_bus_, &io_config, &panel_io_));

        ESP_LOGI(TAG, "Install SSD1306 driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.bits_per_pixel = 1;

        esp_lcd_panel_ssd1306_config_t ssd1306_config = {
            .height = static_cast<uint8_t>(DISPLAY_HEIGHT),
        };
        panel_config.vendor_config = &ssd1306_config;

#ifdef SH1106
        ESP_ERROR_CHECK(esp_lcd_new_panel_sh1106(panel_io_, &panel_config, &panel_));
#else
        ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(panel_io_, &panel_config, &panel_));
#endif
        ESP_LOGI(TAG, "SSD1306 driver installed");

        // Reset the display
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        if (esp_lcd_panel_init(panel_) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize display");
            display_ = new NoDisplay();
            return;
        }
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_, false));

        // Set the display to on
        ESP_LOGI(TAG, "Turning display on");
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

        display_ = new OledDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
    }

#elif CONFIG_WK_ESP32S3_DEV_DISPLAY_LCD
    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_CLK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeLcdDisplay() {
        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install LCD panel IO");

        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;

        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io_));

        // 初始化液晶屏驱动芯片
        ESP_LOGD(TAG, "Install LCD driver");

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;

    #if defined(LCD_TYPE_ILI9341_SERIAL)
        ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io_, &panel_config, &panel_));
    #elif defined(LCD_TYPE_ILI9486_SERIAL)
        ESP_ERROR_CHECK(esp_lcd_new_panel_ili9486(panel_io_, &panel_config, &panel_));
    #elif defined(LCD_TYPE_NV3030B_SERIAL)
        ESP_ERROR_CHECK(esp_lcd_new_panel_nv3030b(panel_io_, &panel_config, &panel_));
    #elif defined(LCD_TYPE_GC9A01_SERIAL)
        gc9a01_vendor_config_t gc9107_vendor_config = {
            .init_cmds = gc9107_lcd_init_cmds,
            .init_cmds_size = sizeof(gc9107_lcd_init_cmds) / sizeof(gc9a01_lcd_init_cmd_t),
        };
        panel_config.vendor_config = &gc9107_vendor_config;
        ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(panel_io_, &panel_config, &panel_));
    #else
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io_, &panel_config, &panel_));
    #endif

        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_, DISPLAY_INVERT_COLOR));
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_, DISPLAY_SWAP_XY));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));

        display_ = new SpiLcdDisplay(panel_io_, panel_,
                                    DISPLAY_WIDTH,
                                    DISPLAY_HEIGHT,
                                    DISPLAY_OFFSET_X,
                                    DISPLAY_OFFSET_Y,
                                    DISPLAY_MIRROR_X,
                                    DISPLAY_MIRROR_Y,
                                    DISPLAY_SWAP_XY);
    }
#endif
    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });


        volume_up_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        volume_up_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(100);
            GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);
        });

        volume_down_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        volume_down_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(0);
            GetDisplay()->ShowNotification(Lang::Strings::MUTED);
        });
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeTools() {
        static LampController lamp(LAMP_GPIO);

        auto& mcp_server = McpServer::GetInstance();

        // Custom tools must be added here (not in McpServer::AddCommonTools).
        if (CONFIG_XIAOZHI_TAVILY_API_KEY[0] != '\0') {
            mcp_server.AddTool(
                "web.search",
                "Search the web for recent information using Tavily. Use this when answering "
                "questions that require current or sourced information.",
                PropertyList({Property("query", kPropertyTypeString),
                              Property("max_results", kPropertyTypeInteger, 5, 1, 10)}),
                [](const PropertyList& properties) -> ToolResult {
                    auto query = properties["query"].value<std::string>();
                    if (query.empty()) {
                        return std::unexpected("Search query must not be empty");
                    }
                    int max_results = properties["max_results"].value<int>();
                    return LilaWebSearch(query, max_results);
                });
        }

        if (CONFIG_XIAOZHI_TELEGRAM_BOT_TOKEN[0] != '\0' &&
            CONFIG_XIAOZHI_TELEGRAM_CHAT_ID[0] != '\0') {
            mcp_server.AddTool(
                "self.telegram.send",
                "Send a text message to the user's phone through Telegram. Use it when the user "
                "asks to send, forward or notify something to their phone or Telegram, for "
                "example a list, a note or a summary.",
                PropertyList({Property("text", kPropertyTypeString)}),
                [](const PropertyList& properties) -> ToolResult {
                    auto text = properties["text"].value<std::string>();
                    if (text.empty()) {
                        return std::unexpected("Message text must not be empty");
                    }
                    auto* job = new LilaTelegramJob{std::move(text)};
                    BaseType_t created =
                        xTaskCreate(LilaTelegramTask, "lila_tg", kLilaTelegramTaskStack,
                                    job, kLilaTelegramTaskPriority, nullptr);
                    if (created != pdPASS) {
                        delete job;
                        return std::unexpected("Failed to queue Telegram message");
                    }
                    return std::string("Message queued for Telegram.");
                });
        }
    }

public:
    WkEsp32s3Dev() :
        boot_button_(BOOT_BUTTON_GPIO), 
        volume_up_button_(VOLUME_UP_BUTTON_GPIO),
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO){
#if CONFIG_WK_ESP32S3_DEV_DISPLAY_OLED
        InitializeDisplayI2c();
        InitializeSsd1306Display();
#elif CONFIG_WK_ESP32S3_DEV_DISPLAY_LCD
        InitializeSpi();
        InitializeLcdDisplay();
        if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            GetBacklight()->RestoreBrightness();
        }
#endif
        InitializeButtons();
        InitializeTools();

    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
#ifdef AUDIO_I2S_METHOD_SIMPLEX
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
             AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, I2S_STD_SLOT_RIGHT,  // 扬声器右声道
            AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN, I2S_STD_SLOT_LEFT); // 左声道 麦克风
#else
        static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
#endif
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }
#if CONFIG_WK_ESP32S3_DEV_DISPLAY_LCD
    virtual Backlight* GetBacklight() override {
        if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
            return &backlight;
        }
        return nullptr;
    }
#endif
};

DECLARE_BOARD(WkEsp32s3Dev);
