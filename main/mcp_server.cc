/*
 * MCP Server Implementation
 * Reference: https://modelcontextprotocol.io/specification/2024-11-05
 */

#include "mcp_server.h"
#include <esp_log.h>
#include <esp_app_desc.h>
#include <algorithm>
#include <cstring>
#include <esp_pthread.h>

#include "application.h"
#include "display.h"
#include "board.h"
#include "settings.h"
#include "lvgl_theme.h"
#include "lvgl_display.h"
#include "schedule_manager.h"
#include "timer_manager.h"

#define TAG "MCP"

#define DEFAULT_TOOLCALL_STACK_SIZE 6144

McpServer::McpServer() {
}

McpServer::~McpServer() {
    for (auto tool : tools_) {
        delete tool;
    }
    tools_.clear();
}

void McpServer::AddCommonTools() {
    // *Important* To speed up the response time, we add the common tools to the beginning of
    // the tools list to utilize the prompt cache.
    // **重要** 为了提升响应速度，我们把常用的工具放在前面，利用 prompt cache 的特性。

    // Backup the original tools list and restore it after adding the common tools.
    auto original_tools = std::move(tools_);
    auto& board = Board::GetInstance();

    // Do not add custom tools here.
    // Custom tools must be added in the board's InitializeTools function.

    AddTool("self.get_device_status",
        "Provides the real-time information of the device, including the current status of the audio speaker, screen, battery, network, etc.\n"
        "Use this tool for: \n"
        "1. Answering questions about current condition (e.g. what is the current volume of the audio speaker?)\n"
        "2. As the first step to control the device (e.g. turn up / down the volume of the audio speaker, etc.)",
        PropertyList(),
        [&board](const PropertyList& properties) -> ReturnValue {
            return board.GetDeviceStatusJson();
        });

    AddTool("self.audio_speaker.set_volume", 
        "Set the volume of the audio speaker. If the current volume is unknown, you must call `self.get_device_status` tool first and then call this tool.",
        PropertyList({
            Property("volume", kPropertyTypeInteger, 0, 100)
        }), 
        [&board](const PropertyList& properties) -> ReturnValue {
            auto codec = board.GetAudioCodec();
            codec->SetOutputVolume(properties["volume"].value<int>());
            return true;
        });
    
    auto backlight = board.GetBacklight();
    if (backlight) {
        AddTool("self.screen.set_brightness",
            "Set the brightness of the screen.",
            PropertyList({
                Property("brightness", kPropertyTypeInteger, 0, 100)
            }),
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
            PropertyList({
                Property("theme", kPropertyTypeString)
            }),
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
            "Take a photo and explain it. Use this tool after the user asks you to see something.\n"
            "Args:\n"
            "  `question`: The question that you want to ask about the photo.\n"
            "Return:\n"
            "  A JSON object that provides the photo information.",
            PropertyList({
                Property("question", kPropertyTypeString)
            }),
            [camera](const PropertyList& properties) -> ReturnValue {
                if (!camera->Capture()) {
                    throw std::runtime_error("Failed to capture photo");
                }
                auto question = properties["question"].value<std::string>();
                return camera->Explain(question);
            });
    }
#endif

    // Restore the original tools list to the end of the tools list
    tools_.insert(tools_.end(), original_tools.begin(), original_tools.end());
}

void McpServer::AddUserOnlyTools() {
    // System tools
    AddTool("self.get_system_info",
        "Get the system information",
        PropertyList(),
        [this](const PropertyList& properties) -> ReturnValue {
            auto& board = Board::GetInstance();
            return board.GetSystemInfoJson();
        });

    AddTool("self.reboot", "Reboot the system",
        PropertyList(),
        [this](const PropertyList& properties) -> ReturnValue {
            std::thread([]() {
                ESP_LOGW(TAG, "User requested reboot");
                vTaskDelay(pdMS_TO_TICKS(1000));
                auto& app = Application::GetInstance();
                app.Reboot();
            }).detach();
            return true;
        });

    // Music control (only if music player is available)
    if (Board::GetInstance().GetMusic()) {
        auto music = Board::GetInstance().GetMusic();

        // 播放指定歌曲（带歌曲名与可选歌手名）
        AddTool("self.music.play_song",
            "播放指定的歌曲。当用户要求播放音乐时使用此工具，会自动获取歌曲详情并开始流式播放。\n"
            "参数:\n"
            "  `song_name`: 要播放的歌曲名称（必需）。\n"
            "  `artist_name`: 要播放的歌曲艺术家名称（可选，默认为空字符串）。\n"
            "返回:\n"
            "  播放状态信息，不需确认，立刻播放歌曲。",
            PropertyList({
                Property("song_name", kPropertyTypeString),
                Property("artist_name", kPropertyTypeString, "")
            }),
            [music](const PropertyList& properties) -> ReturnValue {
                auto song_name = properties["song_name"].value<std::string>();
                auto artist_name = properties["artist_name"].value<std::string>();
                
                if (!music->Download(song_name, artist_name)) {
                    return "{\"success\": false, \"message\": \"获取音乐资源失败\"}";
                }
                auto download_result = music->GetDownloadResult();
                ESP_LOGI(TAG, "Music details result: %s", download_result.c_str());
                return "{\"success\": true, \"message\": \"音乐开始播放\"}";
            });
        AddTool("self.music.set_volume",
            "Set music volume (0-100).",
            PropertyList({
                Property("volume", kPropertyTypeInteger, 0, 100)
            }),
            [music](const PropertyList& properties) -> ReturnValue {
                int vol = properties["volume"].value<int>();
                bool ok = music->SetVolume(vol);
                return ok;
            });


        AddTool("self.music.play",
            "Play current music.",
            PropertyList(),
            [music](const PropertyList& properties) -> ReturnValue {
                bool ok = music->PlaySong();
                return ok;
            });

        // 兼容更明确的命名：stop_song / pause_song / resume_song
        AddTool("self.music.stop_song",
            "Stop current song.",
            PropertyList(),
            [music](const PropertyList& properties) -> ReturnValue {
                bool ok = music->StopSong();
                return ok;
            });

        AddTool("self.music.pause_song",
            "Pause current song.",
            PropertyList(),
            [music](const PropertyList& properties) -> ReturnValue {
                bool ok = music->PauseSong();
                return ok;
            });

        AddTool("self.music.resume_song",
            "Resume current song.",
            PropertyList(),
            [music](const PropertyList& properties) -> ReturnValue {
                bool ok = music->ResumeSong();
                return ok;
            });
    }

    // Display control
#ifdef HAVE_LVGL
    auto display = static_cast<LvglDisplay*>(Board::GetInstance().GetDisplay());
    if (display) {
        AddTool("self.screen.get_info", "Information about the screen, including width, height, etc.",
            PropertyList(),
            [display](const PropertyList& properties) -> ReturnValue {
                cJSON *json = cJSON_CreateObject();
                cJSON_AddNumberToObject(json, "width", display->width());
                cJSON_AddNumberToObject(json, "height", display->height());
                return json;
            });
        
        AddTool("self.screen.preview_image", "Preview an image on the screen",
            PropertyList({
                Property("url", kPropertyTypeString)
            }),
            [display](const PropertyList& properties) -> ReturnValue {
                auto url = properties["url"].value<std::string>();
                auto http = Board::GetInstance().GetNetwork()->CreateHttp(3);

                if (!http->Open("GET", url)) {
                    throw std::runtime_error("Failed to open URL: " + url);
                }
                if (http->GetStatusCode() != 200) {
                    throw std::runtime_error("Unexpected status code: " + std::to_string(http->GetStatusCode()));
                }

                size_t content_length = http->GetBodyLength();
                char* data = (char*)heap_caps_malloc(content_length, MALLOC_CAP_8BIT);
                size_t total_read = 0;
                while (total_read < content_length) {
                    int ret = http->Read(data + total_read, content_length - total_read);
                    if (ret < 0) {
                        heap_caps_free(data);
                        throw std::runtime_error("Failed to download image: " + url);
                    }
                    total_read += ret;
                }
                http->Close();

                auto img_dsc = (lv_img_dsc_t*)heap_caps_calloc(1, sizeof(lv_img_dsc_t), MALLOC_CAP_8BIT);
                img_dsc->data_size = content_length;
                img_dsc->data = (uint8_t*)data;
                if (lv_image_decoder_get_info(img_dsc, &img_dsc->header) != LV_RESULT_OK) {
                    heap_caps_free(data);
                    heap_caps_free(img_dsc);
                    throw std::runtime_error("Failed to get image info");
                }
                ESP_LOGI(TAG, "Preview image: %s size: %d resolution: %d x %d", url.c_str(), content_length, img_dsc->header.w, img_dsc->header.h);

                auto& app = Application::GetInstance();
                app.Schedule([display, img_dsc]() {
                    display->SetPreviewImage(img_dsc);
                });
                return true;
            });
    }
#endif

    // Assets download url
    auto assets = Board::GetInstance().GetAssets();
    if (assets) {
        if (assets->partition_valid()) {
            AddTool("self.assets.set_download_url", "Set the download url for the assets",
                PropertyList({
                    Property("url", kPropertyTypeString)
                }),
                [assets](const PropertyList& properties) -> ReturnValue {
                    auto url = properties["url"].value<std::string>();
                    Settings settings("assets", true);
                    settings.SetString("download_url", url);
                    return true;
                });
        }
    }

    // 日程管理工具
    auto& schedule_manager = ScheduleManager::GetInstance();
    
    AddTool("self.schedule.create_event",
        "创建新的日程事件。支持智能分类和提醒功能。\n"
        "参数:\n"
        "  `title`: 事件标题（必需）\n"
        "  `description`: 事件描述（可选）\n"
        "  `start_time`: 开始时间戳（必需）\n"
        "  `end_time`: 结束时间戳（可选，0表示无结束时间）\n"
        "  `category`: 事件分类（可选，如不提供将自动分类）\n"
        "  `is_all_day`: 是否全天事件（可选，默认false）\n"
        "  `reminder_minutes`: 提醒时间（分钟，可选，默认15分钟）\n"
        "返回:\n"
        "  事件ID字符串，用于后续操作",
        PropertyList({
            Property("title", kPropertyTypeString),
            Property("description", kPropertyTypeString, ""),
            Property("start_time", kPropertyTypeInteger),
            Property("end_time", kPropertyTypeInteger, 0),
            Property("category", kPropertyTypeString, ""),
            Property("is_all_day", kPropertyTypeBoolean, false),
            Property("reminder_minutes", kPropertyTypeInteger, 15, 0, 1440)
        }),
        [&schedule_manager](const PropertyList& properties) -> ReturnValue {
            auto title = properties["title"].value<std::string>();
            auto description = properties["description"].value<std::string>();
            time_t start_time = properties["start_time"].value<int>();
            time_t end_time = properties["end_time"].value<int>();
            auto category = properties["category"].value<std::string>();
            bool is_all_day = properties["is_all_day"].value<bool>();
            int reminder_minutes = properties["reminder_minutes"].value<int>();
            
            std::string event_id = schedule_manager.CreateEvent(
                title, description, start_time, end_time, 
                category, is_all_day, reminder_minutes);
            
            if (event_id.empty()) {
                return "{\"success\": false, \"message\": \"创建事件失败\"}";
            }
            
            return "{\"success\": true, \"event_id\": \"" + event_id + "\", \"message\": \"事件创建成功\"}";
        });

    AddTool("self.schedule.get_events",
        "获取所有日程事件。\n"
        "返回:\n"
        "  事件列表的JSON数组",
        PropertyList(),
        [&schedule_manager](const PropertyList& properties) -> ReturnValue {
            std::string json_str = schedule_manager.ExportToJson();
            return json_str;
        });

    AddTool("self.schedule.delete_event",
        "删除日程事件。\n"
        "参数:\n"
        "  `event_id`: 要删除的事件ID（必需）\n"
        "返回:\n"
        "  操作结果",
        PropertyList({
            Property("event_id", kPropertyTypeString)
        }),
        [&schedule_manager](const PropertyList& properties) -> ReturnValue {
            auto event_id = properties["event_id"].value<std::string>();
            
            bool success = schedule_manager.DeleteEvent(event_id);
            
            if (success) {
                return "{\"success\": true, \"message\": \"事件删除成功\"}";
            } else {
                return "{\"success\": false, \"message\": \"事件删除失败\"}";
            }
        });

    AddTool("self.schedule.get_statistics",
        "获取日程统计信息。\n"
        "返回:\n"
        "  统计信息的JSON对象",
        PropertyList(),
        [&schedule_manager](const PropertyList& properties) -> ReturnValue {
            int total_events = schedule_manager.GetEventCount();
            
            cJSON* json = cJSON_CreateObject();
            cJSON_AddNumberToObject(json, "total_events", total_events);
            cJSON_AddBoolToObject(json, "success", true);
            
            return json;
        });

    // 定时任务工具
    auto& timer_manager = TimerManager::GetInstance();
    
    AddTool("self.timer.create_countdown",
        "创建倒计时器。\n"
        "参数:\n"
        "  `name`: 计时器名称（必需）\n"
        "  `duration_ms`: 持续时间（毫秒，必需）\n"
        "  `description`: 描述（可选）\n"
        "返回:\n"
        "  计时器ID",
        PropertyList({
            Property("name", kPropertyTypeString),
            Property("duration_ms", kPropertyTypeInteger, 1000, 100, 3600000),
            Property("description", kPropertyTypeString, "")
        }),
        [&timer_manager](const PropertyList& properties) -> ReturnValue {
            auto name = properties["name"].value<std::string>();
            uint32_t duration_ms = properties["duration_ms"].value<int>();
            auto description = properties["description"].value<std::string>();
            
            std::string timer_id = timer_manager.CreateCountdownTimer(name, duration_ms, description);
            
            return "{\"success\": true, \"timer_id\": \"" + timer_id + "\", \"message\": \"倒计时器创建成功\"}";
        });

    AddTool("self.timer.create_delayed_task",
        "创建延时执行MCP工具的任务。\n"
        "参数:\n"
        "  `name`: 任务名称（必需）\n"
        "  `delay_ms`: 延时时间（毫秒，必需）\n"
        "  `mcp_tool_name`: MCP工具名称（必需）\n"
        "  `mcp_tool_args`: MCP工具参数（可选）\n"
        "  `description`: 描述（可选）\n"
        "返回:\n"
        "  任务ID",
        PropertyList({
            Property("name", kPropertyTypeString),
            Property("delay_ms", kPropertyTypeInteger, 1000, 100, 3600000),
            Property("mcp_tool_name", kPropertyTypeString),
            Property("mcp_tool_args", kPropertyTypeString, ""),
            Property("description", kPropertyTypeString, "")
        }),
        [&timer_manager](const PropertyList& properties) -> ReturnValue {
            auto name = properties["name"].value<std::string>();
            uint32_t delay_ms = properties["delay_ms"].value<int>();
            auto mcp_tool_name = properties["mcp_tool_name"].value<std::string>();
            auto mcp_tool_args = properties["mcp_tool_args"].value<std::string>();
            auto description = properties["description"].value<std::string>();
            
            std::string task_id = timer_manager.CreateDelayedMcpTask(
                name, delay_ms, mcp_tool_name, mcp_tool_args, description);
            
            return "{\"success\": true, \"task_id\": \"" + task_id + "\", \"message\": \"延时任务创建成功\"}";
        });


    AddTool("self.timer.start_task",
        "启动定时任务。\n"
        "参数:\n"
        "  `task_id`: 任务ID（必需）\n"
        "返回:\n"
        "  操作结果",
        PropertyList({
            Property("task_id", kPropertyTypeString)
        }),
        [&timer_manager](const PropertyList& properties) -> ReturnValue {
            auto task_id = properties["task_id"].value<std::string>();
            
            bool success = timer_manager.StartTask(task_id);
            
            if (success) {
                return "{\"success\": true, \"message\": \"任务启动成功\"}";
            } else {
                return "{\"success\": false, \"message\": \"任务启动失败\"}";
            }
        });

    AddTool("self.timer.stop_task",
        "停止定时任务。\n"
        "参数:\n"
        "  `task_id`: 任务ID（必需）\n"
        "返回:\n"
        "  操作结果",
        PropertyList({
            Property("task_id", kPropertyTypeString)
        }),
        [&timer_manager](const PropertyList& properties) -> ReturnValue {
            auto task_id = properties["task_id"].value<std::string>();
            
            bool success = timer_manager.StopTask(task_id);
            
            if (success) {
                return "{\"success\": true, \"message\": \"任务停止成功\"}";
            } else {
                return "{\"success\": false, \"message\": \"任务停止失败\"}";
            }
        });

    AddTool("self.timer.get_tasks",
        "获取所有定时任务列表。\n"
        "返回:\n"
        "  任务列表",
        PropertyList(),
        [&timer_manager](const PropertyList& properties) -> ReturnValue {
            std::string json_str = timer_manager.ExportToJson();
            return json_str;
        });

    AddTool("self.timer.get_statistics",
        "获取定时任务统计信息。\n"
        "返回:\n"
        "  统计信息",
        PropertyList(),
        [&timer_manager](const PropertyList& properties) -> ReturnValue {
            int total_tasks = timer_manager.GetTaskCount();
            
            cJSON* json = cJSON_CreateObject();
            cJSON_AddNumberToObject(json, "total_tasks", total_tasks);
            cJSON_AddBoolToObject(json, "success", true);
            
            return json;
        });
}

void McpServer::AddTool(McpTool* tool) {
    // Prevent adding duplicate tools
    if (std::find_if(tools_.begin(), tools_.end(), [tool](const McpTool* t) { return t->name() == tool->name(); }) != tools_.end()) {
        ESP_LOGW(TAG, "Tool %s already added", tool->name().c_str());
        return;
    }

    ESP_LOGI(TAG, "Add tool: %s%s", tool->name().c_str(), tool->user_only() ? " [user]" : "");
    tools_.push_back(tool);
}

void McpServer::AddTool(const std::string& name, const std::string& description, const PropertyList& properties, std::function<ReturnValue(const PropertyList&)> callback) {
    AddTool(new McpTool(name, description, properties, callback));
}

void McpServer::AddUserOnlyTool(const std::string& name, const std::string& description, const PropertyList& properties, std::function<ReturnValue(const PropertyList&)> callback) {
    auto tool = new McpTool(name, description, properties, callback);
    tool->set_user_only(true);
    AddTool(tool);
}

void McpServer::ParseMessage(const std::string& message) {
    cJSON* json = cJSON_Parse(message.c_str());
    if (json == nullptr) {
        ESP_LOGE(TAG, "Failed to parse MCP message: %s", message.c_str());
        return;
    }
    ParseMessage(json);
    cJSON_Delete(json);
}

void McpServer::ParseCapabilities(const cJSON* capabilities) {
    auto vision = cJSON_GetObjectItem(capabilities, "vision");
    if (cJSON_IsObject(vision)) {
        auto url = cJSON_GetObjectItem(vision, "url");
        auto token = cJSON_GetObjectItem(vision, "token");
        if (cJSON_IsString(url)) {
            auto camera = Board::GetInstance().GetCamera();
            if (camera) {
                std::string url_str = std::string(url->valuestring);
                std::string token_str;
                if (cJSON_IsString(token)) {
                    token_str = std::string(token->valuestring);
                }
                camera->SetExplainUrl(url_str, token_str);
            }
        }
    }
}

void McpServer::ParseMessage(const cJSON* json) {
    // Check JSONRPC version
    auto version = cJSON_GetObjectItem(json, "jsonrpc");
    if (version == nullptr || !cJSON_IsString(version) || strcmp(version->valuestring, "2.0") != 0) {
        ESP_LOGE(TAG, "Invalid JSONRPC version: %s", version ? version->valuestring : "null");
        return;
    }
    
    // Check method
    auto method = cJSON_GetObjectItem(json, "method");
    if (method == nullptr || !cJSON_IsString(method)) {
        ESP_LOGE(TAG, "Missing method");
        return;
    }
    
    auto method_str = std::string(method->valuestring);
    if (method_str.find("notifications") == 0) {
        return;
    }
    
    // Check params
    auto params = cJSON_GetObjectItem(json, "params");
    if (params != nullptr && !cJSON_IsObject(params)) {
        ESP_LOGE(TAG, "Invalid params for method: %s", method_str.c_str());
        return;
    }

    auto id = cJSON_GetObjectItem(json, "id");
    if (id == nullptr || !cJSON_IsNumber(id)) {
        ESP_LOGE(TAG, "Invalid id for method: %s", method_str.c_str());
        return;
    }
    auto id_int = id->valueint;
    
    if (method_str == "initialize") {
        if (cJSON_IsObject(params)) {
            auto capabilities = cJSON_GetObjectItem(params, "capabilities");
            if (cJSON_IsObject(capabilities)) {
                ParseCapabilities(capabilities);
            }
        }
        auto app_desc = esp_app_get_description();
        std::string message = "{\"protocolVersion\":\"2024-11-05\",\"capabilities\":{\"tools\":{}},\"serverInfo\":{\"name\":\"" BOARD_NAME "\",\"version\":\"";
        message += app_desc->version;
        message += "\"}}";
        ReplyResult(id_int, message);
    } else if (method_str == "tools/list") {
        std::string cursor_str = "";
        bool list_user_only_tools = false;
        if (params != nullptr) {
            auto cursor = cJSON_GetObjectItem(params, "cursor");
            if (cJSON_IsString(cursor)) {
                cursor_str = std::string(cursor->valuestring);
            }
            auto with_user_tools = cJSON_GetObjectItem(params, "withUserTools");
            if (cJSON_IsBool(with_user_tools)) {
                list_user_only_tools = with_user_tools->valueint == 1;
            }
        }
        GetToolsList(id_int, cursor_str, list_user_only_tools);
    } else if (method_str == "tools/call") {
        if (!cJSON_IsObject(params)) {
            ESP_LOGE(TAG, "tools/call: Missing params");
            ReplyError(id_int, "Missing params");
            return;
        }
        auto tool_name = cJSON_GetObjectItem(params, "name");
        if (!cJSON_IsString(tool_name)) {
            ESP_LOGE(TAG, "tools/call: Missing name");
            ReplyError(id_int, "Missing name");
            return;
        }
        auto tool_arguments = cJSON_GetObjectItem(params, "arguments");
        if (tool_arguments != nullptr && !cJSON_IsObject(tool_arguments)) {
            ESP_LOGE(TAG, "tools/call: Invalid arguments");
            ReplyError(id_int, "Invalid arguments");
            return;
        }
        auto stack_size = cJSON_GetObjectItem(params, "stackSize");
        if (stack_size != nullptr && !cJSON_IsNumber(stack_size)) {
            ESP_LOGE(TAG, "tools/call: Invalid stackSize");
            ReplyError(id_int, "Invalid stackSize");
            return;
        }
        DoToolCall(id_int, std::string(tool_name->valuestring), tool_arguments, stack_size ? stack_size->valueint : DEFAULT_TOOLCALL_STACK_SIZE);
    } else {
        ESP_LOGE(TAG, "Method not implemented: %s", method_str.c_str());
        ReplyError(id_int, "Method not implemented: " + method_str);
    }
}

void McpServer::ReplyResult(int id, const std::string& result) {
    std::string payload = "{\"jsonrpc\":\"2.0\",\"id\":";
    payload += std::to_string(id) + ",\"result\":";
    payload += result;
    payload += "}";
    Application::GetInstance().SendMcpMessage(payload);
}

void McpServer::ReplyError(int id, const std::string& message) {
    std::string payload = "{\"jsonrpc\":\"2.0\",\"id\":";
    payload += std::to_string(id);
    payload += ",\"error\":{\"message\":\"";
    payload += message;
    payload += "\"}}";
    Application::GetInstance().SendMcpMessage(payload);
}

void McpServer::GetToolsList(int id, const std::string& cursor, bool list_user_only_tools) {
    const int max_payload_size = 8000;
    std::string json = "{\"tools\":[";
    
    bool found_cursor = cursor.empty();
    auto it = tools_.begin();
    std::string next_cursor = "";
    
    while (it != tools_.end()) {
        // 如果我们还没有找到起始位置，继续搜索
        if (!found_cursor) {
            if ((*it)->name() == cursor) {
                found_cursor = true;
            } else {
                ++it;
                continue;
            }
        }

        if (!list_user_only_tools && (*it)->user_only()) {
            ESP_LOGD(TAG, "Skipping user-only tool: %s", (*it)->name().c_str());
            ++it;
            continue;
        }
        
        // 添加tool前检查大小
        std::string tool_json = (*it)->to_json() + ",";
        if (json.length() + tool_json.length() + 30 > max_payload_size) {
            // 如果添加这个tool会超出大小限制，设置next_cursor并退出循环
            next_cursor = (*it)->name();
            break;
        }
        
        json += tool_json;
        ++it;
    }
    
    if (json.back() == ',') {
        json.pop_back();
    }
    
    if (json.back() == '[' && !tools_.empty()) {
        // 如果没有添加任何tool，返回错误
        ESP_LOGE(TAG, "tools/list: Failed to add tool %s because of payload size limit", next_cursor.c_str());
        ReplyError(id, "Failed to add tool " + next_cursor + " because of payload size limit");
        return;
    }

    if (next_cursor.empty()) {
        json += "]}";
    } else {
        json += "],\"nextCursor\":\"" + next_cursor + "\"}";
    }
    
    ReplyResult(id, json);
}

void McpServer::DoToolCall(int id, const std::string& tool_name, const cJSON* tool_arguments, int stack_size) {
    auto tool_iter = std::find_if(tools_.begin(), tools_.end(), 
                                 [&tool_name](const McpTool* tool) { 
                                     return tool->name() == tool_name; 
                                 });
    
    if (tool_iter == tools_.end()) {
        ESP_LOGE(TAG, "tools/call: Unknown tool: %s", tool_name.c_str());
        ReplyError(id, "Unknown tool: " + tool_name);
        return;
    }

    PropertyList arguments = (*tool_iter)->properties();
    try {
        for (auto& argument : arguments) {
            bool found = false;
            if (cJSON_IsObject(tool_arguments)) {
                auto value = cJSON_GetObjectItem(tool_arguments, argument.name().c_str());
                if (argument.type() == kPropertyTypeBoolean && cJSON_IsBool(value)) {
                    argument.set_value<bool>(value->valueint == 1);
                    found = true;
                } else if (argument.type() == kPropertyTypeInteger && cJSON_IsNumber(value)) {
                    argument.set_value<int>(value->valueint);
                    found = true;
                } else if (argument.type() == kPropertyTypeString && cJSON_IsString(value)) {
                    argument.set_value<std::string>(value->valuestring);
                    found = true;
                }
            }

            if (!argument.has_default_value() && !found) {
                ESP_LOGE(TAG, "tools/call: Missing valid argument: %s", argument.name().c_str());
                ReplyError(id, "Missing valid argument: " + argument.name());
                return;
            }
        }
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "tools/call: %s", e.what());
        ReplyError(id, e.what());
        return;
    }

    // Start a task to receive data with stack size
    esp_pthread_cfg_t cfg = esp_pthread_get_default_config();
    cfg.thread_name = "tool_call";
    cfg.stack_size = stack_size;
    cfg.prio = 1;
    esp_pthread_set_cfg(&cfg);

    // Use a thread to call the tool to avoid blocking the main thread
    tool_call_thread_ = std::thread([this, id, tool_iter, arguments = std::move(arguments)]() {
        try {
            ReplyResult(id, (*tool_iter)->Call(arguments));
        } catch (const std::exception& e) {
            ESP_LOGE(TAG, "tools/call: %s", e.what());
            ReplyError(id, e.what());
        }
    });
    tool_call_thread_.detach();
}