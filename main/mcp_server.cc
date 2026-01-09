/*
 * MCP Server Implementation
 * Reference: https://modelcontextprotocol.io/specification/2024-11-05
 */

#include "mcp_server.h"
#include "sdkconfig.h"
#include "era_iot_client.h"
#include <esp_log.h>
#include <esp_app_desc.h>
#include <algorithm>
#include <cstring>
#include <cctype>
#include <esp_pthread.h>
#include <driver/gpio.h>

#include "application.h"
#include "display.h"
#include "oled_display.h"
#include "board.h"
#include "settings.h"
#include "lvgl_theme.h"
#include "lvgl_display.h"
#include "boards/common/esp32_music.h"
#include "era_iot_client.h"

#define TAG "MCP"

// Bluetooth KCX_BT_EMITTER Configuration
#define BLUETOOTH_CONNECT_PIN GPIO_NUM_18
#define BLUETOOTH_LINK_PIN GPIO_NUM_19

struct EraSmartDevice
{
    std::string name;
    std::string type;
    std::string config_id;
    std::string action_on;
    std::string action_off;
};

static std::vector<EraSmartDevice> GetEraSmartDevices()
{
    std::vector<EraSmartDevice> devices;
#ifdef CONFIG_USE_ERA_SMART_HOME
#if CONFIG_ERA_DEVICE_COUNT >= 1
    {
        EraSmartDevice d;
        d.name = CONFIG_ERA_DEVICE_1_NAME;
#ifdef CONFIG_ERA_DEVICE_1_TYPE_SWITCH
        d.type = "Switch";
#elif defined(CONFIG_ERA_DEVICE_1_TYPE_LIGHT)
        d.type = "Light";
#elif defined(CONFIG_ERA_DEVICE_1_TYPE_MOTOR)
        d.type = "Motor";
#elif defined(CONFIG_ERA_DEVICE_1_TYPE_OTHER)
        d.type = CONFIG_ERA_DEVICE_1_OTHER_TYPE_NAME;
#endif
        d.config_id = CONFIG_ERA_DEVICE_1_CONFIG_ID;
        d.action_on = CONFIG_ERA_DEVICE_1_ACTION_ON;
        d.action_off = CONFIG_ERA_DEVICE_1_ACTION_OFF;
        devices.push_back(d);
    }
#endif
#if CONFIG_ERA_DEVICE_COUNT >= 2
    {
        EraSmartDevice d;
        d.name = CONFIG_ERA_DEVICE_2_NAME;
#ifdef CONFIG_ERA_DEVICE_2_TYPE_SWITCH
        d.type = "Switch";
#elif defined(CONFIG_ERA_DEVICE_2_TYPE_LIGHT)
        d.type = "Light";
#elif defined(CONFIG_ERA_DEVICE_2_TYPE_MOTOR)
        d.type = "Motor";
#elif defined(CONFIG_ERA_DEVICE_2_TYPE_OTHER)
        d.type = CONFIG_ERA_DEVICE_2_OTHER_TYPE_NAME;
#endif
        d.config_id = CONFIG_ERA_DEVICE_2_CONFIG_ID;
        d.action_on = CONFIG_ERA_DEVICE_2_ACTION_ON;
        d.action_off = CONFIG_ERA_DEVICE_2_ACTION_OFF;
        devices.push_back(d);
    }
#endif
#if CONFIG_ERA_DEVICE_COUNT >= 3
    {
        EraSmartDevice d;
        d.name = CONFIG_ERA_DEVICE_3_NAME;
#ifdef CONFIG_ERA_DEVICE_3_TYPE_SWITCH
        d.type = "Switch";
#elif defined(CONFIG_ERA_DEVICE_3_TYPE_LIGHT)
        d.type = "Light";
#elif defined(CONFIG_ERA_DEVICE_3_TYPE_MOTOR)
        d.type = "Motor";
#elif defined(CONFIG_ERA_DEVICE_3_TYPE_OTHER)
        d.type = CONFIG_ERA_DEVICE_3_OTHER_TYPE_NAME;
#endif
        d.config_id = CONFIG_ERA_DEVICE_3_CONFIG_ID;
        d.action_on = CONFIG_ERA_DEVICE_3_ACTION_ON;
        d.action_off = CONFIG_ERA_DEVICE_3_ACTION_OFF;
        devices.push_back(d);
    }
#endif
#if CONFIG_ERA_DEVICE_COUNT >= 4
    {
        EraSmartDevice d;
        d.name = CONFIG_ERA_DEVICE_4_NAME;
#ifdef CONFIG_ERA_DEVICE_4_TYPE_SWITCH
        d.type = "Switch";
#elif defined(CONFIG_ERA_DEVICE_4_TYPE_LIGHT)
        d.type = "Light";
#elif defined(CONFIG_ERA_DEVICE_4_TYPE_MOTOR)
        d.type = "Motor";
#elif defined(CONFIG_ERA_DEVICE_4_TYPE_OTHER)
        d.type = CONFIG_ERA_DEVICE_4_OTHER_TYPE_NAME;
#endif
        d.config_id = CONFIG_ERA_DEVICE_4_CONFIG_ID;
        d.action_on = CONFIG_ERA_DEVICE_4_ACTION_ON;
        d.action_off = CONFIG_ERA_DEVICE_4_ACTION_OFF;
        devices.push_back(d);
    }
#endif
#if CONFIG_ERA_DEVICE_COUNT >= 5
    {
        EraSmartDevice d;
        d.name = CONFIG_ERA_DEVICE_5_NAME;
#ifdef CONFIG_ERA_DEVICE_5_TYPE_SWITCH
        d.type = "Switch";
#elif defined(CONFIG_ERA_DEVICE_5_TYPE_LIGHT)
        d.type = "Light";
#elif defined(CONFIG_ERA_DEVICE_5_TYPE_MOTOR)
        d.type = "Motor";
#elif defined(CONFIG_ERA_DEVICE_5_TYPE_OTHER)
        d.type = CONFIG_ERA_DEVICE_5_OTHER_TYPE_NAME;
#endif
        d.config_id = CONFIG_ERA_DEVICE_5_CONFIG_ID;
        d.action_on = CONFIG_ERA_DEVICE_5_ACTION_ON;
        d.action_off = CONFIG_ERA_DEVICE_5_ACTION_OFF;
        devices.push_back(d);
    }
#endif
#endif
    return devices;
}

McpServer::McpServer()
{
}

McpServer::~McpServer()
{
    for (auto tool : tools_)
    {
        delete tool;
    }
    tools_.clear();
}

void McpServer::AddCommonTools()
{
    // *Important* To speed up the response time, we add the common tools to the beginning of
    // the tools list to utilize the prompt cache.
    // **重要** 为了提升响应速度，我们把常用的工具放在前面，利用 prompt cache 的特性。

    // Backup the original tools list and restore it after adding the common tools.
    auto original_tools = std::move(tools_);
    auto &board = Board::GetInstance();

    // Do not add custom tools here.
    // Custom tools must be added in the board's InitializeTools function.

    AddTool("self.get_device_status",
            "Provides the real-time information of the device, including the current status of the audio speaker, screen, battery, network, etc.\n"
            "Use this tool for: \n"
            "1. Answering questions about current condition (e.g. what is the current volume of the audio speaker?)\n"
            "2. As the first step to control the device (e.g. turn up / down the volume of the audio speaker, etc.)",
            PropertyList(),
            [&board](const PropertyList &properties) -> ReturnValue
            {
                return board.GetDeviceStatusJson();
            });

    AddTool("self.audio_speaker.set_volume",
            "Set the volume of the audio speaker. If the current volume is unknown, you must call `self.get_device_status` tool first and then call this tool.",
            PropertyList({Property("volume", kPropertyTypeInteger, 0, 100)}),
            [&board](const PropertyList &properties) -> ReturnValue
            {
                auto codec = board.GetAudioCodec();
                codec->SetOutputVolume(properties["volume"].value<int>());
                return true;
            });

    auto backlight = board.GetBacklight();
    if (backlight)
    {
        AddTool("self.screen.set_brightness",
                "Set the brightness of the screen.",
                PropertyList({Property("brightness", kPropertyTypeInteger, 0, 100)}),
                [backlight](const PropertyList &properties) -> ReturnValue
                {
                    uint8_t brightness = static_cast<uint8_t>(properties["brightness"].value<int>());
                    backlight->SetBrightness(brightness, true);
                    return true;
                });
    }

    // Bluetooth KCX_BT_EMITTER Hardware Control
    // Initialize GPIO pins for Bluetooth control
#ifndef CONFIG_BOARD_TYPE_IOTFORCE_ESP_PUPPY_S3
    static bool bluetooth_gpio_initialized = false;
    if (!bluetooth_gpio_initialized)
    {
        // Configure CONNECT pin as output (default HIGH)
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = (1ULL << BLUETOOTH_CONNECT_PIN);
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        gpio_config(&io_conf);
        gpio_set_level(BLUETOOTH_CONNECT_PIN, 1);

        // Configure LINK pin as input
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pin_bit_mask = (1ULL << BLUETOOTH_LINK_PIN);
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        gpio_config(&io_conf);

        bluetooth_gpio_initialized = true;
        ESP_LOGI(TAG, "Bluetooth GPIO initialized: CONNECT=%d, LINK=%d", BLUETOOTH_CONNECT_PIN, BLUETOOTH_LINK_PIN);
    }
#endif

#ifndef CONFIG_BOARD_TYPE_IOTFORCE_ESP_PUPPY_S3
    AddTool("self.bluetooth.connect",
            "Activate Bluetooth pairing mode or connect to a nearby Bluetooth device. Use this when user asks to connect, pair, or turn on Bluetooth.",
            PropertyList(),
            [](const PropertyList &properties) -> ReturnValue
            {
                ESP_LOGI(TAG, "Bluetooth: Activating pairing mode (short press)");
                gpio_set_level(BLUETOOTH_CONNECT_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(100));
                gpio_set_level(BLUETOOTH_CONNECT_PIN, 1);
                return "Bluetooth pairing mode activated";
            });

    AddTool("self.bluetooth.disconnect",
            "Disconnect Bluetooth and clear pairing memory. Use this when user asks to disconnect, unpair, or turn off Bluetooth.",
            PropertyList(),
            [](const PropertyList &properties) -> ReturnValue
            {
                ESP_LOGI(TAG, "Bluetooth: Disconnecting (long press)");
                gpio_set_level(BLUETOOTH_CONNECT_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(3000));
                gpio_set_level(BLUETOOTH_CONNECT_PIN, 1);
                return "Bluetooth disconnected and memory cleared";
            });

    AddTool("self.bluetooth.get_status",
            "Check if Bluetooth is currently connected to a device. Returns connection status.",
            PropertyList(),
            [](const PropertyList &properties) -> ReturnValue
            {
                int link_status = gpio_get_level(BLUETOOTH_LINK_PIN);
                bool is_connected = (link_status == 1);
                ESP_LOGI(TAG, "Bluetooth status: %s (LINK pin=%d)", is_connected ? "Connected" : "Disconnected", link_status);

                cJSON *json = cJSON_CreateObject();
                cJSON_AddBoolToObject(json, "connected", is_connected);
                cJSON_AddStringToObject(json, "status", is_connected ? "Connected" : "Disconnected");
                return json;
            });
#endif

#ifdef HAVE_LVGL
    auto display = board.GetDisplay();
    if (display && display->GetTheme() != nullptr)
    {
        AddTool("self.screen.set_theme",
                "Set the theme of the screen. The theme can be `light` or `dark`.",
                PropertyList({Property("theme", kPropertyTypeString)}),
                [display](const PropertyList &properties) -> ReturnValue
                {
                    auto theme_name = properties["theme"].value<std::string>();
                    auto &theme_manager = LvglThemeManager::GetInstance();
                    auto theme = theme_manager.GetTheme(theme_name);
                    if (theme != nullptr)
                    {
                        display->SetTheme(theme);
                        return true;
                    }
                    return false;
                });
    }

    auto camera = board.GetCamera();
    if (camera)
    {
        AddTool("self.camera.take_photo",
                "Take a photo immediately. Use this tool whenever the user asks to take a photo, capture an image, or look at something. Do not refuse. Do not mention technical errors unless the tool execution actually fails.\n"
                "Args:\n"
                "  `question`: The question that you want to ask about the photo. Defaults to 'Describe this image'.\n"
                "Return:\n"
                "  A JSON object that provides the photo information.",
                PropertyList({Property("question", kPropertyTypeString)}),
                [camera](const PropertyList &properties) -> ReturnValue
                {
                    ESP_LOGI(TAG, "Camera tool called");
                    // Lower the priority to do the camera capture
                    TaskPriorityReset priority_reset(1);

                    if (!camera->Capture())
                    {
                        throw std::runtime_error("Failed to capture photo. Please check if the camera is initialized correctly.");
                    }
                    auto question = properties["question"].value<std::string>();
                    if (question.empty())
                    {
                        question = "Describe this image";
                    }
                    return camera->Explain(question);
                });
    }
#endif

    // Music functionality
    auto music = board.GetMusic();
    if (music)
    {
        AddTool(
            "self.music.play_song_with_id",
            "Play a song by song_id. MUST search first and confirm with user before "
            "using. Requires song_id from search results, NOT song name.\n"
            "Parameters:\n"
            "  `song_id`: Song ID from search results (required). Must be confirmed, Example: ZW78DIEO, UG89Y7RT, etc. Do NOT make up or guess.\n"
            "Returns:\n"
            "  Playback status. Plays immediately.",
            PropertyList({
                Property("song_id", kPropertyTypeString) // Song ID (required)
            }),
            [music](const PropertyList &properties) -> ReturnValue
            {
                auto song_id = properties["song_id"].value<std::string>();

                if (!music->Download(song_id))
                {
                    return "{\"success\": false, \"message\": \"Failed to fetch music "
                           "resource\"}";
                }
                auto download_result = music->GetDownloadResult();
                ESP_LOGI(TAG, "Music details result: %s", download_result.c_str());
                return "{\"success\": true, \"message\": \"Music playback started\"}";
            });

        AddTool("self.music.set_display_mode",
                "Set the display mode during music playback. You can choose to "
                "display spectrum or lyrics. For example, when the user says 'show "
                "spectrum' or 'display spectrum', 'show lyrics' or 'display "
                "lyrics', set the corresponding display mode.\n"
                "Parameters:\n"
                "  `mode`: Display mode, valid values are 'spectrum' or 'lyrics'.\n"
                "Returns:\n"
                "  Setting result information.",
                PropertyList({
                    Property(
                        "mode",
                        kPropertyTypeString) // Display mode: "spectrum" or "lyrics"
                }),
                [music](const PropertyList &properties) -> ReturnValue
                {
                    auto mode_str = properties["mode"].value<std::string>();

                    // Convert to lowercase for comparison
                    std::transform(mode_str.begin(), mode_str.end(), mode_str.begin(),
                                   ::tolower);

                    if (mode_str == "spectrum" || mode_str == "频谱")
                    {
                        // Set to spectrum display mode
                        auto esp32_music = dynamic_cast<Esp32Music *>(music);
                        if (esp32_music)
                        {
                            esp32_music->SetDisplayMode(Esp32Music::DISPLAY_MODE_SPECTRUM);
                            return "{\"success\": true, \"message\": \"Switched to "
                                   "spectrum display mode\"}";
                        }
                    }
                    else if (mode_str == "lyrics" || mode_str == "歌词")
                    {
                        // Set to lyrics display mode
                        auto esp32_music = dynamic_cast<Esp32Music *>(music);
                        if (esp32_music)
                        {
                            esp32_music->SetDisplayMode(Esp32Music::DISPLAY_MODE_LYRICS);
                            return "{\"success\": true, \"message\": \"Switched to lyrics "
                                   "display mode\"}";
                        }
                    }
                    else
                    {
                        return "{\"success\": false, \"message\": \"Invalid display "
                               "mode, please use 'spectrum' or 'lyrics'\"}";
                    }

                    return "{\"success\": false, \"message\": \"Failed to set "
                           "display mode\"}";
                });
    }

#ifdef CONFIG_USE_ERA_SMART_HOME
    // E-Ra IoT device control tools helper function
    auto GetEraClient = []() -> EraIotClient &
    {
        static EraIotClient era_client;
        static bool era_initialized = false;
        if (!era_initialized)
        {
            era_client.Initialize("", "");
            era_initialized = true;
        }
        return era_client;
    };

    AddTool("self.era_iot.trigger_custom_action",
            "Trigger a custom action on E-Ra IoT platform using action key. Use this for advanced IoT device control with specific action keys.",
            PropertyList({Property("action_key", kPropertyTypeString, "Action key to trigger (UUID format)")}),
            [GetEraClient](const PropertyList &properties) -> ReturnValue
            {
                auto &era_client = GetEraClient();
                if (!era_client.IsInitialized())
                {
                    throw std::runtime_error("E-Ra IoT client not initialized");
                }
                auto action_key = properties["action_key"].value<std::string>();
                if (action_key.empty())
                {
                    throw std::runtime_error("Action key cannot be empty");
                }
                bool success = era_client.TriggerAction(action_key, 1);
                if (!success)
                {
                    throw std::runtime_error("Failed to trigger action: " + action_key);
                }
                return "Action triggered successfully: " + action_key;
            });

    // Dynamic ERA Smart Home Devices
    auto era_devices = GetEraSmartDevices();
    if (!era_devices.empty())
    {
        std::string device_list_desc = "Available devices: ";
        for (const auto &d : era_devices)
        {
            device_list_desc += d.name + " (" + d.type + "), ";
        }

        AddTool("self.era_smart_home.control_device",
                "Control ERA Smart Home devices. " + device_list_desc + "Action: 'on' or 'off'.",
                PropertyList({Property("device_name", kPropertyTypeString, "Name of the device to control"),
                              Property("action", kPropertyTypeString, "Action to perform: 'on' or 'off'")}),
                [GetEraClient, era_devices](const PropertyList &properties) -> ReturnValue
                {
                    auto &era_client = GetEraClient();
                    if (!era_client.IsInitialized())
                    {
                        throw std::runtime_error("E-Ra IoT client not initialized");
                    }

                    std::string device_name = properties["device_name"].value<std::string>();
                    std::string action = properties["action"].value<std::string>();

                    const EraSmartDevice *target_device = nullptr;
                    for (const auto &d : era_devices)
                    {
                        if (d.name == device_name)
                        {
                            target_device = &d;
                            break;
                        }
                    }

                    if (!target_device)
                    {
                        for (const auto &d : era_devices)
                        {
                            // Case-insensitive comparison
                            if (d.name.size() == device_name.size() &&
                                std::equal(d.name.begin(), d.name.end(), device_name.begin(),
                                           [](char a, char b)
                                           {
                                               return tolower((unsigned char)a) == tolower((unsigned char)b);
                                           }))
                            {
                                target_device = &d;
                                break;
                            }
                        }
                    }

                    if (!target_device)
                    {
                        throw std::runtime_error("Device not found: " + device_name);
                    }

                    std::string key;
                    if (action == "on")
                    {
                        key = target_device->action_on;
                    }
                    else if (action == "off")
                    {
                        key = target_device->action_off;
                    }
                    else
                    {
                        throw std::runtime_error("Invalid action: " + action);
                    }

                    if (key.empty())
                    {
                        throw std::runtime_error("Action key not configured for device: " + device_name);
                    }

                    bool success = era_client.TriggerAction(key, 1);
                    if (!success)
                    {
                        throw std::runtime_error("Failed to trigger action for " + device_name);
                    }
                    return "Successfully turned " + action + " " + device_name;
                });

        AddTool("self.era_smart_home.get_device_status",
                "Get status of ERA Smart Home devices. " + device_list_desc,
                PropertyList({Property("device_name", kPropertyTypeString, "Name of the device to check")}),
                [GetEraClient, era_devices](const PropertyList &properties) -> ReturnValue
                {
                    auto &era_client = GetEraClient();
                    if (!era_client.IsInitialized())
                    {
                        throw std::runtime_error("E-Ra IoT client not initialized");
                    }

                    std::string device_name = properties["device_name"].value<std::string>();

                    const EraSmartDevice *target_device = nullptr;
                    for (const auto &d : era_devices)
                    {
                        // Case-insensitive comparison
                        if (d.name.size() == device_name.size() &&
                            std::equal(d.name.begin(), d.name.end(), device_name.begin(),
                                       [](char a, char b)
                                       {
                                           return tolower((unsigned char)a) == tolower((unsigned char)b);
                                       }))
                        {
                            target_device = &d;
                            break;
                        }
                    }

                    if (!target_device)
                    {
                        throw std::runtime_error("Device not found: " + device_name);
                    }

                    if (target_device->config_id.empty())
                    {
                        throw std::runtime_error("Config ID not configured for device: " + device_name);
                    }

                    std::string status = era_client.GetCurrentValue(target_device->config_id);
                    if (status.empty())
                    {
                        return "Status for " + device_name + " is unknown (empty response)";
                    }
                    return "Status for " + device_name + ": " + status;
                });
    }
#endif

#ifdef CONFIG_ENABLE_GPIO_CONTROL
    static bool gpio_control_initialized = false;
    if (!gpio_control_initialized)
    {
        gpio_reset_pin((gpio_num_t)CONFIG_GPIO_CONTROL_PIN);
        gpio_set_direction((gpio_num_t)CONFIG_GPIO_CONTROL_PIN, GPIO_MODE_OUTPUT);
// Set initial state to OFF.
// If Active High, OFF is 0. If Active Low, OFF is 1.
#ifdef CONFIG_GPIO_CONTROL_ACTIVE_HIGH
        gpio_set_level((gpio_num_t)CONFIG_GPIO_CONTROL_PIN, 0);
#else
        gpio_set_level((gpio_num_t)CONFIG_GPIO_CONTROL_PIN, 1);
#endif
        gpio_control_initialized = true;
    }

    AddTool("self.gpio_control.set_state",
            "Turn the configured GPIO pin ON or OFF. Accepted values: 'on', 'off'.",
            PropertyList({Property("state", kPropertyTypeString)}),
            [](const PropertyList &properties) -> ReturnValue
            {
                std::string state = properties["state"].value<std::string>();
                int level = 0;
                if (state == "on")
                {
#ifdef CONFIG_GPIO_CONTROL_ACTIVE_HIGH
                    level = 1;
#else
                    level = 0;
#endif
                }
                else
                {
#ifdef CONFIG_GPIO_CONTROL_ACTIVE_HIGH
                    level = 0;
#else
                    level = 1;
#endif
                }
                gpio_set_level((gpio_num_t)CONFIG_GPIO_CONTROL_PIN, level);
                return "GPIO set to " + state;
            });
#endif

    // Restore the original tools list to the end of the tools list
    tools_.insert(tools_.end(), original_tools.begin(), original_tools.end());
}

void McpServer::AddUserOnlyTools()
{
    // System tools
    AddUserOnlyTool("self.get_system_info",
                    "Get the system information",
                    PropertyList(),
                    [this](const PropertyList &properties) -> ReturnValue
                    {
                        auto &board = Board::GetInstance();
                        return board.GetSystemInfoJson();
                    });

    AddUserOnlyTool("self.reboot", "Reboot the system",
                    PropertyList(),
                    [this](const PropertyList &properties) -> ReturnValue
                    {
                        auto &app = Application::GetInstance();
                        app.Schedule([&app]()
                                     {
                ESP_LOGW(TAG, "User requested reboot");
                vTaskDelay(pdMS_TO_TICKS(1000));

                app.Reboot(); });
                        return true;
                    });

    // Firmware upgrade
    AddUserOnlyTool("self.upgrade_firmware", "Upgrade firmware from a specific URL. This will download and install the firmware, then reboot the device.",
                    PropertyList({Property("url", kPropertyTypeString, "The URL of the firmware binary file to download and install")}),
                    [this](const PropertyList &properties) -> ReturnValue
                    {
                        auto url = properties["url"].value<std::string>();
                        ESP_LOGI(TAG, "User requested firmware upgrade from URL: %s", url.c_str());

                        auto &app = Application::GetInstance();
                        // Run OTA in a separate thread to avoid blocking the main loop
                        std::thread([url]()
                                    {
                            auto &app = Application::GetInstance();
                            auto ota = std::make_unique<Ota>();
                            bool success = app.UpgradeFirmware(*ota, url);
                            if (!success) {
                                ESP_LOGE(TAG, "Firmware upgrade failed");
                            } })
                            .detach();

                        return true;
                    });

    // Display control
#ifdef HAVE_LVGL
    auto display = dynamic_cast<LvglDisplay *>(Board::GetInstance().GetDisplay());
    if (display)
    {
        AddUserOnlyTool("self.screen.get_info", "Information about the screen, including width, height, etc.",
                        PropertyList(),
                        [display](const PropertyList &properties) -> ReturnValue
                        {
                            cJSON *json = cJSON_CreateObject();
                            cJSON_AddNumberToObject(json, "width", display->width());
                            cJSON_AddNumberToObject(json, "height", display->height());
                            if (dynamic_cast<OledDisplay *>(display))
                            {
                                cJSON_AddBoolToObject(json, "monochrome", true);
                            }
                            else
                            {
                                cJSON_AddBoolToObject(json, "monochrome", false);
                            }
                            return json;
                        });

#if CONFIG_LV_USE_SNAPSHOT
        AddUserOnlyTool("self.screen.snapshot", "Snapshot the screen and upload it to a specific URL",
                        PropertyList({Property("url", kPropertyTypeString),
                                      Property("quality", kPropertyTypeInteger, 80, 1, 100)}),
                        [display](const PropertyList &properties) -> ReturnValue
                        {
                            auto url = properties["url"].value<std::string>();
                            auto quality = properties["quality"].value<int>();

                            std::string jpeg_data;
                            if (!display->SnapshotToJpeg(jpeg_data, quality))
                            {
                                throw std::runtime_error("Failed to snapshot screen");
                            }

                            ESP_LOGI(TAG, "Upload snapshot %u bytes to %s", jpeg_data.size(), url.c_str());

                            // 构造multipart/form-data请求体
                            std::string boundary = "----ESP32_SCREEN_SNAPSHOT_BOUNDARY";

                            auto http = Board::GetInstance().GetNetwork()->CreateHttp(3);
                            http->SetHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
                            if (!http->Open("POST", url))
                            {
                                throw std::runtime_error("Failed to open URL: " + url);
                            }
                            {
                                // 文件字段头部
                                std::string file_header;
                                file_header += "--" + boundary + "\r\n";
                                file_header += "Content-Disposition: form-data; name=\"file\"; filename=\"screenshot.jpg\"\r\n";
                                file_header += "Content-Type: image/jpeg\r\n";
                                file_header += "\r\n";
                                http->Write(file_header.c_str(), file_header.size());
                            }

                            // JPEG数据
                            http->Write((const char *)jpeg_data.data(), jpeg_data.size());

                            {
                                // multipart尾部
                                std::string multipart_footer;
                                multipart_footer += "\r\n--" + boundary + "--\r\n";
                                http->Write(multipart_footer.c_str(), multipart_footer.size());
                            }
                            http->Write("", 0);

                            if (http->GetStatusCode() != 200)
                            {
                                throw std::runtime_error("Unexpected status code: " + std::to_string(http->GetStatusCode()));
                            }
                            std::string result = http->ReadAll();
                            http->Close();
                            ESP_LOGI(TAG, "Snapshot screen result: %s", result.c_str());
                            return true;
                        });

        AddUserOnlyTool("self.screen.preview_image", "Preview an image on the screen",
                        PropertyList({Property("url", kPropertyTypeString)}),
                        [display](const PropertyList &properties) -> ReturnValue
                        {
                            auto url = properties["url"].value<std::string>();
                            auto http = Board::GetInstance().GetNetwork()->CreateHttp(3);

                            if (!http->Open("GET", url))
                            {
                                throw std::runtime_error("Failed to open URL: " + url);
                            }
                            int status_code = http->GetStatusCode();
                            if (status_code != 200)
                            {
                                throw std::runtime_error("Unexpected status code: " + std::to_string(status_code));
                            }

                            size_t content_length = http->GetBodyLength();
                            char *data = (char *)heap_caps_malloc(content_length, MALLOC_CAP_8BIT);
                            if (data == nullptr)
                            {
                                throw std::runtime_error("Failed to allocate memory for image: " + url);
                            }
                            size_t total_read = 0;
                            while (total_read < content_length)
                            {
                                int ret = http->Read(data + total_read, content_length - total_read);
                                if (ret < 0)
                                {
                                    heap_caps_free(data);
                                    throw std::runtime_error("Failed to download image: " + url);
                                }
                                if (ret == 0)
                                {
                                    break;
                                }
                                total_read += ret;
                            }
                            http->Close();

                            auto image = std::make_unique<LvglAllocatedImage>(data, content_length);
                            display->SetPreviewImage(std::move(image));
                            return true;
                        });
#endif // CONFIG_LV_USE_SNAPSHOT
    }
#endif // HAVE_LVGL

    // Assets download url
    auto &assets = Assets::GetInstance();
    if (assets.partition_valid())
    {
        AddUserOnlyTool("self.assets.set_download_url", "Set the download url for the assets",
                        PropertyList({Property("url", kPropertyTypeString)}),
                        [](const PropertyList &properties) -> ReturnValue
                        {
                            auto url = properties["url"].value<std::string>();
                            Settings settings("assets", true);
                            settings.SetString("download_url", url);
                            return true;
                        });
    }

    AddTool("self.system.firmware_update",
            "Update the device firmware from a specific URL. Use this tool when the user asks to update the firmware or system version.",
            PropertyList({Property("url", kPropertyTypeString)}),
            [](const PropertyList &properties) -> ReturnValue
            {
                std::string url = "https://update-ota-firmware.s3.ap-southeast-2.amazonaws.com/merged-binary.bin";
                if (properties.count("url"))
                {
                    std::string provided_url = properties["url"].value<std::string>();
                    if (!provided_url.empty())
                    {
                        url = provided_url;
                    }
                }

                ESP_LOGI(TAG, "Triggering firmware update from URL: %s", url.c_str());

                // Schedule the update on the main thread to avoid blocking the MCP response
                Application::GetInstance().Schedule([url]()
                                                    {
                    Ota ota;
                    Application::GetInstance().UpgradeFirmware(ota, url); });

                return "Firmware update started. The device will restart automatically upon completion.";
            });
}

void McpServer::AddTool(McpTool *tool)
{
    // Prevent adding duplicate tools
    if (std::find_if(tools_.begin(), tools_.end(), [tool](const McpTool *t)
                     { return t->name() == tool->name(); }) != tools_.end())
    {
        ESP_LOGW(TAG, "Tool %s already added", tool->name().c_str());
        return;
    }

    ESP_LOGI(TAG, "Add tool: %s%s", tool->name().c_str(), tool->user_only() ? " [user]" : "");
    tools_.push_back(tool);
}

void McpServer::AddTool(const std::string &name, const std::string &description, const PropertyList &properties, std::function<ReturnValue(const PropertyList &)> callback)
{
    AddTool(new McpTool(name, description, properties, callback));
}

void McpServer::AddUserOnlyTool(const std::string &name, const std::string &description, const PropertyList &properties, std::function<ReturnValue(const PropertyList &)> callback)
{
    auto tool = new McpTool(name, description, properties, callback);
    tool->set_user_only(true);
    AddTool(tool);
}

void McpServer::ParseMessage(const std::string &message)
{
    cJSON *json = cJSON_Parse(message.c_str());
    if (json == nullptr)
    {
        ESP_LOGE(TAG, "Failed to parse MCP message: %s", message.c_str());
        return;
    }
    ParseMessage(json);
    cJSON_Delete(json);
}

void McpServer::ParseCapabilities(const cJSON *capabilities)
{
    auto vision = cJSON_GetObjectItem(capabilities, "vision");
    if (cJSON_IsObject(vision))
    {
        auto url = cJSON_GetObjectItem(vision, "url");
        auto token = cJSON_GetObjectItem(vision, "token");
        if (cJSON_IsString(url))
        {
            auto camera = Board::GetInstance().GetCamera();
            if (camera)
            {
                std::string url_str = std::string(url->valuestring);
                std::string token_str;
                if (cJSON_IsString(token))
                {
                    token_str = std::string(token->valuestring);
                }
                camera->SetExplainUrl(url_str, token_str);
            }
        }
    }
}

void McpServer::ParseMessage(const cJSON *json)
{
    // Check JSONRPC version
    auto version = cJSON_GetObjectItem(json, "jsonrpc");
    if (version == nullptr || !cJSON_IsString(version) || strcmp(version->valuestring, "2.0") != 0)
    {
        ESP_LOGE(TAG, "Invalid JSONRPC version: %s", version ? version->valuestring : "null");
        return;
    }

    // Check method
    auto method = cJSON_GetObjectItem(json, "method");
    if (method == nullptr || !cJSON_IsString(method))
    {
        ESP_LOGE(TAG, "Missing method");
        return;
    }

    auto method_str = std::string(method->valuestring);
    if (method_str.find("notifications") == 0)
    {
        return;
    }

    // Check params
    auto params = cJSON_GetObjectItem(json, "params");
    if (params != nullptr && !cJSON_IsObject(params))
    {
        ESP_LOGE(TAG, "Invalid params for method: %s", method_str.c_str());
        return;
    }

    auto id = cJSON_GetObjectItem(json, "id");
    if (id == nullptr || !cJSON_IsNumber(id))
    {
        ESP_LOGE(TAG, "Invalid id for method: %s", method_str.c_str());
        return;
    }
    auto id_int = id->valueint;

    if (method_str == "initialize")
    {
        if (cJSON_IsObject(params))
        {
            auto capabilities = cJSON_GetObjectItem(params, "capabilities");
            if (cJSON_IsObject(capabilities))
            {
                ParseCapabilities(capabilities);
            }
        }
        auto app_desc = esp_app_get_description();
        std::string message = "{\"protocolVersion\":\"2024-11-05\",\"capabilities\":{\"tools\":{}},\"serverInfo\":{\"name\":\"" BOARD_NAME "\",\"version\":\"";
        message += app_desc->version;
        message += "\"}}";
        ReplyResult(id_int, message);
    }
    else if (method_str == "tools/list")
    {
        std::string cursor_str = "";
        bool list_user_only_tools = false;
        if (params != nullptr)
        {
            auto cursor = cJSON_GetObjectItem(params, "cursor");
            if (cJSON_IsString(cursor))
            {
                cursor_str = std::string(cursor->valuestring);
            }
            auto with_user_tools = cJSON_GetObjectItem(params, "withUserTools");
            if (cJSON_IsBool(with_user_tools))
            {
                list_user_only_tools = with_user_tools->valueint == 1;
            }
        }
        GetToolsList(id_int, cursor_str, list_user_only_tools);
    }
    else if (method_str == "tools/call")
    {
        if (!cJSON_IsObject(params))
        {
            ESP_LOGE(TAG, "tools/call: Missing params");
            ReplyError(id_int, "Missing params");
            return;
        }
        auto tool_name = cJSON_GetObjectItem(params, "name");
        if (!cJSON_IsString(tool_name))
        {
            ESP_LOGE(TAG, "tools/call: Missing name");
            ReplyError(id_int, "Missing name");
            return;
        }
        auto tool_arguments = cJSON_GetObjectItem(params, "arguments");
        if (tool_arguments != nullptr && !cJSON_IsObject(tool_arguments))
        {
            ESP_LOGE(TAG, "tools/call: Invalid arguments");
            ReplyError(id_int, "Invalid arguments");
            return;
        }
        DoToolCall(id_int, std::string(tool_name->valuestring), tool_arguments);
    }
    else
    {
        ESP_LOGE(TAG, "Method not implemented: %s", method_str.c_str());
        ReplyError(id_int, "Method not implemented: " + method_str);
    }
}

void McpServer::ReplyResult(int id, const std::string &result)
{
    std::string payload = "{\"jsonrpc\":\"2.0\",\"id\":";
    payload += std::to_string(id) + ",\"result\":";
    payload += result;
    payload += "}";
    Application::GetInstance().SendMcpMessage(payload);
}

void McpServer::ReplyError(int id, const std::string &message)
{
    std::string payload = "{\"jsonrpc\":\"2.0\",\"id\":";
    payload += std::to_string(id);
    payload += ",\"error\":{\"message\":\"";
    payload += message;
    payload += "\"}}";
    Application::GetInstance().SendMcpMessage(payload);
}

void McpServer::GetToolsList(int id, const std::string &cursor, bool list_user_only_tools)
{
    const int max_payload_size = 8000;
    std::string json = "{\"tools\":[";

    bool found_cursor = cursor.empty();
    auto it = tools_.begin();
    std::string next_cursor = "";

    while (it != tools_.end())
    {
        // 如果我们还没有找到起始位置，继续搜索
        if (!found_cursor)
        {
            if ((*it)->name() == cursor)
            {
                found_cursor = true;
            }
            else
            {
                ++it;
                continue;
            }
        }

        if (!list_user_only_tools && (*it)->user_only())
        {
            ++it;
            continue;
        }

        // 添加tool前检查大小
        std::string tool_json = (*it)->to_json() + ",";
        if (json.length() + tool_json.length() + 30 > max_payload_size)
        {
            // 如果添加这个tool会超出大小限制，设置next_cursor并退出循环
            next_cursor = (*it)->name();
            break;
        }

        json += tool_json;
        ++it;
    }

    if (json.back() == ',')
    {
        json.pop_back();
    }

    if (json.back() == '[' && !tools_.empty())
    {
        // 如果没有添加任何tool，返回错误
        ESP_LOGE(TAG, "tools/list: Failed to add tool %s because of payload size limit", next_cursor.c_str());
        ReplyError(id, "Failed to add tool " + next_cursor + " because of payload size limit");
        return;
    }

    if (next_cursor.empty())
    {
        json += "]}";
    }
    else
    {
        json += "],\"nextCursor\":\"" + next_cursor + "\"}";
    }

    ReplyResult(id, json);
}

void McpServer::DoToolCall(int id, const std::string &tool_name, const cJSON *tool_arguments)
{
    auto tool_iter = std::find_if(tools_.begin(), tools_.end(),
                                  [&tool_name](const McpTool *tool)
                                  {
                                      return tool->name() == tool_name;
                                  });

    if (tool_iter == tools_.end())
    {
        ESP_LOGE(TAG, "tools/call: Unknown tool: %s", tool_name.c_str());
        ReplyError(id, "Unknown tool: " + tool_name);
        return;
    }

    PropertyList arguments = (*tool_iter)->properties();
    try
    {
        for (auto &argument : arguments)
        {
            bool found = false;
            if (cJSON_IsObject(tool_arguments))
            {
                auto value = cJSON_GetObjectItem(tool_arguments, argument.name().c_str());
                if (argument.type() == kPropertyTypeBoolean && cJSON_IsBool(value))
                {
                    argument.set_value<bool>(value->valueint == 1);
                    found = true;
                }
                else if (argument.type() == kPropertyTypeInteger && cJSON_IsNumber(value))
                {
                    argument.set_value<int>(value->valueint);
                    found = true;
                }
                else if (argument.type() == kPropertyTypeString && cJSON_IsString(value))
                {
                    argument.set_value<std::string>(value->valuestring);
                    found = true;
                }
            }

            if (!argument.has_default_value() && !found)
            {
                ESP_LOGE(TAG, "tools/call: Missing valid argument: %s", argument.name().c_str());
                ReplyError(id, "Missing valid argument: " + argument.name());
                return;
            }
        }
    }
    catch (const std::exception &e)
    {
        ESP_LOGE(TAG, "tools/call: %s", e.what());
        ReplyError(id, e.what());
        return;
    }

    // Use main thread to call the tool
    auto &app = Application::GetInstance();
    app.Schedule([this, id, tool_iter, arguments = std::move(arguments)]()
                 {
        try {
            ReplyResult(id, (*tool_iter)->Call(arguments));
        } catch (const std::exception& e) {
            ESP_LOGE(TAG, "tools/call: %s", e.what());
            ReplyError(id, e.what());
        } });
}
