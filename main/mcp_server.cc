/*
 * MCP Server Implementation -  (XiaoZhi AI)
 * Features:
 *  - Live WiFi Signal & Quality Telemetry Task (Every 2 sec)
 *  - Assistant Mode State Switch Lock
 */

#include "mcp_server.h"
#include <esp_app_desc.h>
#include <esp_log.h>
#include <esp_pthread.h>
#include <algorithm>
#include <cstring>
#include <iterator>

#include "application.h"
#include "board.h"
#include "display.h"
#include "lvgl_image.h"
#include "lvgl_theme.h"
#include "settings.h"

#include "esp_now.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "MCP"

void RegisterAnimaBodyControl(McpServer* server);

McpServer::McpServer() {}

McpServer::~McpServer() = default;

void McpServer::AddCommonTools() {
    auto original_tools = std::move(tools_);
    auto& board = Board::GetInstance();

    RegisterAnimaBodyControl(this);

    AddTool("self.get_device_status",
            "Provides the real-time information of the device, including the current status of the audio speaker, screen, battery, network, etc.",
            PropertyList(), [&board](const PropertyList& properties) -> ReturnValue {
                return board.GetDeviceStatusJson();
            });

    AddTool("self.audio_speaker.set_volume",
            "Set the volume of the audio speaker.",
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
                    uint8_t brightness = static_cast<uint8_t>(properties["brightness"].value<int>());
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
#endif

    tools_.insert(tools_.end(), std::make_move_iterator(original_tools.begin()),
                  std::make_move_iterator(original_tools.end()));
}

void McpServer::AddUserOnlyTools() {
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

    AddUserOnlyTool(
        "self.upgrade_firmware",
        "Upgrade firmware from a specific URL.",
        PropertyList({Property("url", kPropertyTypeString)}),
        [this](const PropertyList& properties) -> ReturnValue {
            auto url = properties["url"].value<std::string>();
            auto& app = Application::GetInstance();
            app.Schedule([url, &app]() {
                app.UpgradeFirmware(url);
            });
            return true;
        });

    AddUserOnlyTool("self.assets.set_download_url", "Set the download url for the assets",
                    PropertyList({Property("url", kPropertyTypeString)}),
                    [](const PropertyList& properties) -> ReturnValue {
                        auto url = properties["url"].value<std::string>();
                        Settings settings("assets", true);
                        settings.SetString("download_url", url);
                        return true;
                    });
}

void McpServer::AddTool(std::unique_ptr<McpTool> tool) {
    if (std::find_if(tools_.begin(), tools_.end(), [&tool](const auto& existing) {
            return existing->name() == tool->name();
        }) != tools_.end()) {
        return;
    }
    tools_.push_back(std::move(tool));
}

void McpServer::AddTool(const std::string& name, const std::string& description,
                        const PropertyList& properties, ToolCallback callback) {
    AddTool(std::make_unique<McpTool>(name, description, properties, std::move(callback)));
}

void McpServer::AddUserOnlyTool(const std::string& name, const std::string& description,
                                const PropertyList& properties, ToolCallback callback) {
    auto tool = std::make_unique<McpTool>(name, description, properties, std::move(callback));
    tool->set_user_only(true);
    AddTool(std::move(tool));
}

void McpServer::ParseMessage(const std::string& message, ResponseSender response_sender) {
    CJsonUniquePtr json(cJSON_Parse(message.c_str()));
    if (json == nullptr) return;
    ParseMessage(json.get(), std::move(response_sender));
}

void McpServer::ParseCapabilities(const cJSON* capabilities) {}

void McpServer::ParseMessage(const cJSON* json, ResponseSender response_sender) {
    auto version = cJSON_GetObjectItem(json, "jsonrpc");
    if (version == nullptr || !cJSON_IsString(version) || strcmp(version->valuestring, "2.0") != 0) return;

    auto method = cJSON_GetObjectItem(json, "method");
    if (method == nullptr || !cJSON_IsString(method)) return;

    auto method_str = std::string(method->valuestring);
    if (method_str.find("notifications") == 0) return;

    auto params = cJSON_GetObjectItem(json, "params");
    auto id = cJSON_GetObjectItem(json, "id");
    if (id == nullptr || !cJSON_IsNumber(id)) return;
    auto id_int = id->valueint;

    if (method_str == "initialize") {
        auto app_desc = esp_app_get_description();
        std::string message = "{\"protocolVersion\":\"2024-11-05\",\"capabilities\":{\"tools\":{}},\"serverInfo\":{\"name\":\"" BOARD_NAME "\",\"version\":\"" + std::string(app_desc->version) + "\"}}";
        ReplyResult(id_int, message, response_sender);
    } else if (method_str == "tools/list") {
        GetToolsList(id_int, "", false, response_sender);
    } else if (method_str == "tools/call") {
        if (!cJSON_IsObject(params)) return;
        auto tool_name = cJSON_GetObjectItem(params, "name");
        auto tool_arguments = cJSON_GetObjectItem(params, "arguments");
        if (tool_name && cJSON_IsString(tool_name)) {
            DoToolCall(id_int, std::string(tool_name->valuestring), tool_arguments, std::move(response_sender));
        }
    }
}

void McpServer::SendResponse(const std::string& payload, const ResponseSender& response_sender) {
    if (response_sender) response_sender(payload);
    else Application::GetInstance().SendMcpMessage(payload);
}

void McpServer::ReplyResult(int id, const std::string& result, const ResponseSender& response_sender) {
    std::string payload = "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) + ",\"result\":" + result + "}";
    SendResponse(payload, response_sender);
}

void McpServer::ReplyError(int id, int code, const std::string& message, const ResponseSender& response_sender) {
    std::string payload = "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) + ",\"error\":{\"code\":" + std::to_string(code) + ",\"message\":\"" + message + "\"}}";
    SendResponse(payload, response_sender);
}

void McpServer::ReplyError(int id, const std::string& message, const ResponseSender& response_sender) {
    ReplyError(id, -32603, message, response_sender);
}

void McpServer::GetToolsList(int id, const std::string& cursor, bool list_user_only_tools, const ResponseSender& response_sender) {
    std::string json = "{\"tools\":[";
    for (size_t i = 0; i < tools_.size(); ++i) {
        if (!list_user_only_tools && tools_[i]->user_only()) continue;
        json += tools_[i]->to_json();
        if (i < tools_.size() - 1) json += ",";
    }
    if (json.back() == ',') json.pop_back();
    json += "]}";
    ReplyResult(id, json, response_sender);
}

void McpServer::DoToolCall(int id, const std::string& tool_name, const cJSON* tool_arguments, ResponseSender response_sender) {
    auto tool_iter = std::find_if(tools_.begin(), tools_.end(), [&tool_name](const auto& tool) {
        return tool->name() == tool_name;
    });

    if (tool_iter == tools_.end()) {
        ReplyError(id, -32602, "Unknown tool: " + tool_name, response_sender);
        return;
    }

    McpTool* tool = tool_iter->get();
    PropertyList arguments = tool->properties();

    auto& app = Application::GetInstance();
    app.Schedule([this, id, tool, arguments = std::move(arguments), response_sender = std::move(response_sender)]() {
        auto result = tool->Call(arguments);
        if (!result) {
            ReplyError(id, result.error(), response_sender);
            return;
        }
        ReplyResult(id, *result, response_sender);
    });
}


// ==================== ANIMA 6 BOARD 1 ESP-NOW & LIVE WIFI LOGIC ====================

static const char* ANIMA_TAG = "ANIMA_BOARD1_ESPNOW";

static uint8_t board2_mac[6] = {0x28, 0x84, 0x85, 0x8A, 0x71, 0x28};

static bool g_assistant_mode_active = true;

static void OnDataRecv(const esp_now_recv_info_t *esp_now_info, const uint8_t *incomingData, int len) {
    char message[32] = {0};
    int copy_len = (len < sizeof(message) - 1) ? len : sizeof(message) - 1;
    memcpy(message, incomingData, copy_len);
    message[copy_len] = '\0';

    ESP_LOGI(ANIMA_TAG, "Board 2 से संदेश प्राप्त हुआ: %s", message);

    if (strcmp(message, "ASSISTANT_ON") == 0) {
        g_assistant_mode_active = true;
        ESP_LOGI(ANIMA_TAG, "Assistant Mode ON: AI 'Anima' एक्टिव है।");
    } else if (strcmp(message, "ASSISTANT_OFF") == 0) {
        g_assistant_mode_active = false;
        ESP_LOGI(ANIMA_TAG, "Assistant Mode OFF: AI स्लीप मोड में चला गया है।");
    }
}

void SendAnimaCommand(const char* action_cmd) {
    if (!g_assistant_mode_active && strcmp(action_cmd, "FORCE") != 0) {
        ESP_LOGW(ANIMA_TAG, "Assistant Mode OFF है। एक्शन कमांड रद्द किया गया।");
        return;
    }

    char packet[64];
    snprintf(packet, sizeof(packet), "CMD:%s", action_cmd);
    
    esp_err_t result = esp_now_send(board2_mac, (uint8_t *)packet, strlen(packet));
    if (result == ESP_OK) {
        ESP_LOGI(ANIMA_TAG, "Board 2 को सर्वो कमांड भेजा गया: %s", packet);
    } else {
        ESP_LOGE(ANIMA_TAG, "ESP-NOW भेजने में त्रुटि: %d", result);
    }
}
static void WiFiStatusTask(void* pvParameters) {
    wifi_ap_record_t ap_info;
    char wifi_packet[64];

    while (1) {
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            int rssi = ap_info.rssi;
            
            // RSSI to Signal Percentage
            int quality = 0;
            if (rssi <= -100) quality = 0;
            else if (rssi >= -50) quality = 100;
            else quality = 2 * (rssi + 100);

            // फ़ॉर्मेट: WIFI:CON:<RSSI>:<QUALITY>%
            snprintf(wifi_packet, sizeof(wifi_packet), "WIFI:CON:%d:%d%%", rssi, quality);
        } else {
            snprintf(wifi_packet, sizeof(wifi_packet), "WIFI:DIS");
        }

        esp_now_send(board2_mac, (uint8_t*)wifi_packet, strlen(wifi_packet));

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
void Anima_ESPNow_Init() {
    if (esp_now_init() != ESP_OK) {
        ESP_LOGE(ANIMA_TAG, "ESP-NOW इनिशियलाइज़ेशन विफल");
        return;
    }

    esp_now_register_recv_cb(OnDataRecv);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, board2_mac, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        ESP_LOGE(ANIMA_TAG, "Board 2 Peer जोड़ने में विफल");
        return;
    }

    // बैकग्राउंड फ्रीआरटीओएस टास्क चालू करें
    xTaskCreate(WiFiStatusTask, "wifi_status_task", 3072, NULL, 5, NULL);

    ESP_LOGI(ANIMA_TAG, "ESP-NOW Bidirectional setup एवं WiFi Telemetry चालू है।");
}

// 5. MCP Tool रजिस्ट्रेशन (समस्त रोबोट एक्शन्स के विवरण के साथ)
void RegisterAnimaBodyControl(McpServer* server) {
    Anima_ESPNow_Init();

    server->AddTool("control_anima_body", 
        "Control Anima 6 robot body movements and postures. Pass one of the following exact action strings in the 'action' parameter:\n"
        "- 'namaste': Fold hands together in front to greet/namaste\n"
        "- 'raise_right_hand': Raise the right arm/hand up\n"
        "- 'raise_left_hand': Raise the left arm/hand up\n"
        "- 'turn_left': Rotate body base turn towards left\n"
        "- 'turn_right': Rotate body base turn towards right\n"
        "- 'look_up': Tilt head up\n"
        "- 'look_down': Tilt head down\n"
        "- 'look_left': Rotate head pan to left\n"
        "- 'look_right': Rotate head pan to right\n"
        "- 'scary_mode': Activate scary posture with raised arms and wide eyes\n"
        "- 'reset_pose': Return all servos and head to default resting position", 
        PropertyList({Property("action", kPropertyTypeString)}),
        [](const PropertyList& properties) -> ReturnValue {
            auto action = properties["action"].value<std::string>();
            SendAnimaCommand(action.c_str());
            return true;
        }
    );
}
