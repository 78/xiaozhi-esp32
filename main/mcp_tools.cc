#include "mcp_tools.h"

#include <esp_log.h>
#include <nvs.h>

#include <algorithm>
#include <iterator>

#include "application.h"
#include "board.h"
#include "display.h"
#include "lvgl_image.h"
#include "lvgl_theme.h"
#include "mcp_server.h"
#include "settings.h"
#include "web_search_format.h"

#define TAG "MCP"

namespace {

constexpr char kTavilyToolName[] = "self.tavily.web_search";
constexpr char kTavilyApiKeyNvsKey[] = "tavily_api_key";

bool IsValidTavilyApiKey(std::string_view key) {
    return key.size() <= McpTools::kTavilyApiKeyMaxLength &&
           std::all_of(key.begin(), key.end(), [](unsigned char c) { return c > ' ' && c < 0x7f; });
}

std::string LoadTavilyApiKey() {
    nvs_handle_t nvs;
    if (nvs_open("wifi", NVS_READONLY, &nvs) != ESP_OK) {
        return {};
    }
    char key[McpTools::kTavilyApiKeyMaxLength + 1] = {};
    std::size_t size = sizeof(key);
    auto error = nvs_get_str(nvs, kTavilyApiKeyNvsKey, key, &size);
    nvs_close(nvs);
    return error == ESP_OK && IsValidTavilyApiKey(key) ? key : "";
}

ToolResult SearchTavily(const PropertyList& properties) {
    auto key = LoadTavilyApiKey();
    if (key.empty()) {
        return std::unexpected("Tavily API key has been cleared");
    }
    auto query = properties["query"].value<std::string>();
    if (query.find_first_not_of(" \t\r\n") == std::string::npos) {
        return std::unexpected("Enter a search query");
    }

    CJsonUniquePtr request(cJSON_CreateObject());
    if (!request || !cJSON_AddStringToObject(request.get(), "query", query.c_str()) ||
        !cJSON_AddNumberToObject(request.get(), "max_results", 3) ||
        !cJSON_AddBoolToObject(request.get(), "include_answer", true)) {
        return std::unexpected("Could not create Tavily request");
    }
    CJsonStringUniquePtr body(cJSON_PrintUnformatted(request.get()));
    if (!body) {
        return std::unexpected("Could not encode Tavily request");
    }

    auto network = Board::GetInstance().GetNetwork();
    auto http = network ? network->CreateHttp(4) : nullptr;
    if (!http) {
        return std::unexpected("Network unavailable");
    }
    http->SetTimeout(8000);
    http->SetHeader("Content-Type", "application/json");
    http->SetHeader("Authorization", "Bearer " + key);
    http->SetContent(std::string(body.get()));
    if (auto opened = http->Open("POST", "https://api.tavily.com/search"); !opened) {
        return std::unexpected("Could not connect to Tavily: " + opened.error().ToString());
    }

    auto status = http->GetStatusCode();
    if (!status) {
        return std::unexpected("Could not read Tavily status: " + status.error().ToString());
    }
    if (*status != 200) {
        return std::unexpected("Tavily returned HTTP " + std::to_string(*status));
    }
    auto response = ReadBounded(*http, kMaxWebSearchResponseBytes);
    http->Close();
    if (!response) {
        return std::unexpected(std::move(response.error()));
    }
    auto result = FormatWebSearchResults(*response, query);
    if (!result) {
        return std::unexpected(std::move(result.error()));
    }
    return std::move(*result);
}

void SetTavilyToolEnabled(McpServer& server, bool enabled) {
    if (!enabled) {
        server.RemoveTool(kTavilyToolName);
        return;
    }
    auto tool = std::make_unique<McpTool>(
        kTavilyToolName,
        "Search the web from this device using the configured Tavily account. "
        "Provide a concise search query. Returns an answer and source URLs with snippets.",
        PropertyList({Property("query", kPropertyTypeString).SetMaxLength(400)}), SearchTavily);
    tool->set_runs_in_background(true);
    server.AddTool(std::move(tool));
}

}  // namespace

bool McpTools::HasTavilyApiKey() { return !LoadTavilyApiKey().empty(); }

esp_err_t McpTools::SaveTavilyApiKey(const std::string& key) {
    if (!IsValidTavilyApiKey(key)) {
        return ESP_ERR_INVALID_ARG;
    }
    const bool was_configured = HasTavilyApiKey();
    const bool configured = !key.empty();
    nvs_handle_t nvs;
    auto result = nvs_open("wifi", NVS_READWRITE, &nvs);
    if (result != ESP_OK) {
        return result;
    }
    if (configured) {
        result = nvs_set_str(nvs, kTavilyApiKeyNvsKey, key.c_str());
    } else {
        result = nvs_erase_key(nvs, kTavilyApiKeyNvsKey);
        if (result == ESP_ERR_NVS_NOT_FOUND) {
            result = ESP_OK;
        }
    }
    if (result == ESP_OK) {
        result = nvs_commit(nvs);
    }
    nvs_close(nvs);
    if (result == ESP_OK && was_configured != configured) {
        SetTavilyToolEnabled(McpServer::GetInstance(), configured);
    }
    return result;
}

void McpServer::AddCommonTools() {
    // *Important* To speed up the response time, we add the common tools to the beginning of
    // the tools list to utilize the prompt cache.
    // **重要** 为了提升响应速度，我们把常用的工具放在前面，利用 prompt cache 的特性。

    // Backup the original tools list and restore it after adding the common tools.
    decltype(tools_) original_tools;
    {
        std::lock_guard lock(tools_mutex_);
        original_tools.swap(tools_);
    }
    auto& board = Board::GetInstance();

    // Do not add custom tools here.
    // Custom tools must be added in the board's InitializeTools function.

    AddTool("self.get_device_status",
            "Provides the real-time information of the device, including the current status of the "
            "audio speaker, screen, battery, network, and why the device last restarted.\n"
            "Use this tool for: \n"
            "1. Answering questions about current condition (e.g. what is the current volume of "
            "the audio speaker?)\n"
            "2. Answering why the device rebooted or restarted - whether it was powered on, "
            "reset by its button, or crashed. This is about the device, not about the "
            "conversation.\n"
            "3. As the first step to control the device (e.g. turn up / down the volume of the "
            "audio speaker, etc.)",
            PropertyList(), [&board](const PropertyList& properties) -> ReturnValue {
                return board.GetDeviceStatusJson();
            });

    AddTool("self.audio_speaker.set_volume",
            "Set the volume of the audio speaker. If the current volume is unknown, you must call "
            "`self.get_device_status` tool first and then call this tool.",
            PropertyList({Property("volume", kPropertyTypeInteger, 0, 100)}),
            [&board](const PropertyList& properties) -> ReturnValue {
                auto codec = board.GetAudioCodec();
                codec->SetOutputVolume(properties["volume"].value<int>());
                return true;
            });

    auto backlight = board.GetBacklight();
    if (backlight) {
        AddTool("self.screen.set_brightness", "Set the brightness of the screen.",
                PropertyList({Property("brightness", kPropertyTypeInteger, 0, 100)}),
                [backlight](const PropertyList& properties) -> ReturnValue {
                    uint8_t brightness =
                        static_cast<uint8_t>(properties["brightness"].value<int>());
                    backlight->SetBrightness(brightness, true);
                    return true;
                });
    }

#ifdef HAVE_LVGL
    auto display = board.GetDisplay();
    if (display && display->GetTheme() != nullptr) {
        AddTool("self.screen.set_theme",
                "Set the theme of the screen. The theme can be `light` or `dark`.",
                PropertyList({Property("theme", kPropertyTypeString)}),
                [display](const PropertyList& properties) -> ReturnValue {
                    auto theme_name = properties["theme"].value<std::string>();
                    auto& theme_manager = LvglThemeManager::GetInstance();
                    auto theme = theme_manager.GetTheme(theme_name);
                    if (theme != nullptr) {
                        display->SetTheme(theme);
                        return true;
                    }
                    return false;
                });
    }

    auto camera = board.GetCamera();
    if (camera) {
        AddTool("self.camera.take_photo",
                "Always remember you have a camera. If the user asks you to see something, use "
                "this tool to take a photo and then explain it.\n"
                "Args:\n"
                "  `question`: The question that you want to ask about the photo.\n"
                "Return:\n"
                "  A JSON object that provides the photo information.",
                PropertyList({Property("question", kPropertyTypeString)}),
                [](const PropertyList& properties, const McpRequestContext& context) -> ToolResult {
                    auto camera = Board::GetInstance().GetCamera();
                    if (camera == nullptr) {
                        return std::unexpected("Camera unavailable");
                    }
                    // Lower the priority to do the camera capture
                    TaskPriorityReset priority_reset(1);

                    if (!camera->Capture()) {
                        return std::unexpected("Failed to capture photo");
                    }
                    auto question = properties["question"].value<std::string>();
                    auto result = context.vision ? camera->Explain(question, context.vision->url,
                                                                   context.vision->token)
                                                 : camera->Explain(question);
                    if (!result) {
                        return std::unexpected(std::move(result.error()));
                    }
                    return std::move(*result);
                });
    }
#endif

    if (McpTools::HasTavilyApiKey()) {
        SetTavilyToolEnabled(*this, true);
    }

    // Restore the original tools list to the end of the tools list
    std::lock_guard lock(tools_mutex_);
    tools_.insert(tools_.end(), std::make_move_iterator(original_tools.begin()),
                  std::make_move_iterator(original_tools.end()));
}

void McpServer::AddUserOnlyTools() {
    // System tools
    AddUserOnlyTool("self.get_system_info", "Get the system information", PropertyList(),
                    [this](const PropertyList& properties) -> ReturnValue {
                        auto& board = Board::GetInstance();
                        return board.GetSystemInfoJson();
                    });

    AddUserOnlyTool("self.reboot", "Reboot the system", PropertyList(),
                    [this](const PropertyList& properties) -> ReturnValue {
                        auto& app = Application::GetInstance();
                        app.Schedule([&app]() {
                            ESP_LOGW(TAG, "User requested reboot");
                            vTaskDelay(pdMS_TO_TICKS(1000));

                            app.Reboot();
                        });
                        return true;
                    });

    // Firmware upgrade
    AddUserOnlyTool(
        "self.upgrade_firmware",
        "Upgrade firmware from a specific URL. This will download and install the firmware, then "
        "reboot the device.",
        PropertyList({Property("url", kPropertyTypeString,
                               "The URL of the firmware binary file to download and install")}),
        [this](const PropertyList& properties) -> ReturnValue {
            auto url = properties["url"].value<std::string>();
            ESP_LOGI(TAG, "User requested firmware upgrade from URL: %s", url.c_str());

            auto& app = Application::GetInstance();
            app.Schedule([url, &app]() {
                bool success = app.UpgradeFirmware(url);
                if (!success) {
                    ESP_LOGE(TAG, "Firmware upgrade failed");
                }
            });

            return true;
        });

    // Display control
#ifdef HAVE_LVGL
    auto display = Board::GetInstance().GetDisplay();
    if (display != nullptr && display->SupportsGuiOperations()) {
        AddUserOnlyTool("self.screen.get_info",
                        "Information about the screen, including width, height, etc.",
                        PropertyList(), [display](const PropertyList& properties) -> ReturnValue {
                            cJSON* json = cJSON_CreateObject();
                            cJSON_AddNumberToObject(json, "width", display->width());
                            cJSON_AddNumberToObject(json, "height", display->height());
                            cJSON_AddBoolToObject(json, "monochrome", display->IsMonochrome());
                            return json;
                        });

#if CONFIG_LV_USE_SNAPSHOT
        AddUserOnlyTool(
            "self.screen.snapshot", "Snapshot the screen and upload it to a specific URL",
            PropertyList({Property("url", kPropertyTypeString),
                          Property("quality", kPropertyTypeInteger, 80, 1, 100)}),
            [display](const PropertyList& properties) -> ToolResult {
                auto url = properties["url"].value<std::string>();
                auto quality = properties["quality"].value<int>();

                std::string jpeg_data;
                if (!display->SnapshotToJpeg(jpeg_data, quality)) {
                    return std::unexpected("Failed to snapshot screen");
                }

                ESP_LOGI(TAG, "Upload snapshot %u bytes to %s", jpeg_data.size(), url.c_str());

                // 构造multipart/form-data请求体
                std::string boundary = "----ESP32_SCREEN_SNAPSHOT_BOUNDARY";

                auto http = Board::GetInstance().GetNetwork()->CreateHttp(3);
                http->SetHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
                if (auto opened = http->Open("POST", url); !opened) {
                    return std::unexpected("Failed to open URL: " + url + " (" +
                                           opened.error().ToString() + ")");
                }
                auto write_or_fail = [&http](const char* data,
                                             size_t size) -> std::optional<std::string> {
                    if (auto written = http->Write(data, size); !written) {
                        return written.error().ToString();
                    }
                    return std::nullopt;
                };
                {
                    // 文件字段头部
                    std::string file_header;
                    file_header += "--" + boundary + "\r\n";
                    file_header +=
                        "Content-Disposition: form-data; name=\"file\"; "
                        "filename=\"screenshot.jpg\"\r\n";
                    file_header += "Content-Type: image/jpeg\r\n";
                    file_header += "\r\n";
                    if (auto error = write_or_fail(file_header.c_str(), file_header.size());
                        error) {
                        http->Close();
                        return std::unexpected("Failed to upload screenshot: " + *error);
                    }
                }

                // JPEG数据
                if (auto error = write_or_fail((const char*)jpeg_data.data(), jpeg_data.size());
                    error) {
                    http->Close();
                    return std::unexpected("Failed to upload screenshot: " + *error);
                }

                {
                    // multipart尾部
                    std::string multipart_footer;
                    multipart_footer += "\r\n--" + boundary + "--\r\n";
                    if (auto error =
                            write_or_fail(multipart_footer.c_str(), multipart_footer.size());
                        error) {
                        http->Close();
                        return std::unexpected("Failed to upload screenshot: " + *error);
                    }
                }
                if (auto error = write_or_fail("", 0); error) {
                    http->Close();
                    return std::unexpected("Failed to upload screenshot: " + *error);
                }

                auto upload_status = http->GetStatusCode();
                if (!upload_status) {
                    return std::unexpected(upload_status.error().ToString());
                }
                if (*upload_status != 200) {
                    return std::unexpected("Unexpected status code: " +
                                           std::to_string(*upload_status));
                }
                std::string result = http->ReadAll();
                http->Close();
                ESP_LOGI(TAG, "Snapshot screen result: %s", result.c_str());
                return true;
            });

        AddUserOnlyTool(
            "self.screen.preview_image", "Preview an image on the screen",
            PropertyList({Property("url", kPropertyTypeString)}),
            [display](const PropertyList& properties) -> ToolResult {
                auto url = properties["url"].value<std::string>();
                auto http = Board::GetInstance().GetNetwork()->CreateHttp(3);

                if (auto opened = http->Open("GET", url); !opened) {
                    return std::unexpected("Failed to open URL: " + url + " (" +
                                           opened.error().ToString() + ")");
                }
                auto status_code = http->GetStatusCode();
                if (!status_code) {
                    return std::unexpected(status_code.error().ToString());
                }
                if (*status_code != 200) {
                    return std::unexpected("Unexpected status code: " +
                                           std::to_string(*status_code));
                }

                size_t content_length = http->GetBodyLength();
                using BufferPtr = std::unique_ptr<char, decltype(&heap_caps_free)>;
                BufferPtr data(
                    static_cast<char*>(heap_caps_malloc(content_length, MALLOC_CAP_8BIT)),
                    heap_caps_free);
                if (data == nullptr) {
                    return std::unexpected("Failed to allocate memory for image: " + url);
                }
                size_t total_read = 0;
                while (total_read < content_length) {
                    auto ret = http->Read(data.get() + total_read, content_length - total_read);
                    if (!ret) {
                        return std::unexpected("Failed to download image: " + url);
                    }
                    if (*ret == 0) {
                        break;
                    }
                    total_read += *ret;
                }
                http->Close();

                auto image = std::make_unique<LvglAllocatedImage>(data.release(), total_read);
                if (!image->IsValid()) {
                    return std::unexpected("Downloaded image is invalid: " + url);
                }
                display->SetPreviewImage(std::move(image));
                return true;
            });
#endif  // CONFIG_LV_USE_SNAPSHOT
    }
#endif  // HAVE_LVGL

    // Assets download url (always registered — Settings storage works regardless of partition
    // layout)
    AddUserOnlyTool("self.assets.set_download_url", "Set the download url for the assets",
                    PropertyList({Property("url", kPropertyTypeString)}),
                    [](const PropertyList& properties) -> ReturnValue {
                        auto url = properties["url"].value<std::string>();
                        Settings settings("assets", true);
                        settings.SetString("download_url", url);
                        return true;
                    });
}
