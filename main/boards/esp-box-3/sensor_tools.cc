#include "sensor_tools.h"

#include <cJSON.h>
#include <cstring>
#include <esp_log.h>
#include <esp_timer.h>

#include "../../mcp_server.h"

#define TAG "SensorTools"

static std::string MakeErrorJson(const char* field) {
    cJSON* json = cJSON_CreateObject();
    cJSON_AddBoolToObject(json, "ok", false);
    cJSON_AddStringToObject(json, "error", field);
    char* str = cJSON_PrintUnformatted(json);
    std::string result(str);
    cJSON_free(str);
    cJSON_Delete(json);
    return result;
}

void InitializeSensorTools(Aht30* aht30, RadarMs58* radar, IrDriver* ir) {
    auto& mcp_server = McpServer::GetInstance();

    if (aht30 != nullptr) {
        mcp_server.AddTool(
            "self.env.temperature",
            "Read indoor temperature and humidity from the SENSOR sub-board's "
            "AHT30 sensor. Returns {\"temp_c\": float, \"humidity_pct\": float}. "
            "Use this for any 'how warm/cold/humid is the room' question; do "
            "not use for outdoor weather (no network).",
            PropertyList(),
            [aht30](const PropertyList& properties) -> ReturnValue {
                float temp_c = 0.0f;
                float humidity_pct = 0.0f;
                esp_err_t err = aht30->Read(&temp_c, &humidity_pct);
                if (err != ESP_OK) {
                    ESP_LOGW(TAG, "env.temperature read failed: %s",
                             esp_err_to_name(err));
                    return MakeErrorJson(esp_err_to_name(err));
                }
                cJSON* json = cJSON_CreateObject();
                cJSON_AddNumberToObject(json, "temp_c", temp_c);
                cJSON_AddNumberToObject(json, "humidity_pct", humidity_pct);
                char* str = cJSON_PrintUnformatted(json);
                std::string result(str);
                cJSON_free(str);
                cJSON_Delete(json);
                return result;
            });
    }

    if (ir != nullptr) {
        mcp_server.AddTool(
            "self.ir.emit",
            "Send a learned IR remote code via the SENSOR sub-board's IR LED. "
            "Pass `protocol` (currently only \"NEC\" is supported) and `code` "
            "as a 32-bit integer where the low 16 bits are the NEC address "
            "field and the high 16 bits are the NEC command field. Use this "
            "to control TVs, fans, ACs, etc. that have been learned via "
            "self.ir.learn_start.",
            PropertyList({
                Property("protocol", kPropertyTypeString),
                Property("code", kPropertyTypeInteger),
            }),
            [ir](const PropertyList& properties) -> ReturnValue {
                std::string protocol = properties["protocol"].value<std::string>();
                uint32_t code = static_cast<uint32_t>(properties["code"].value<int>());
                uint16_t address = code & 0xFFFFu;
                uint16_t command = (code >> 16) & 0xFFFFu;
                esp_err_t err = ir->Emit(protocol, address, command);
                if (err != ESP_OK) {
                    ESP_LOGW(TAG, "ir.emit failed: %s", esp_err_to_name(err));
                    return MakeErrorJson(esp_err_to_name(err));
                }
                cJSON* json = cJSON_CreateObject();
                cJSON_AddBoolToObject(json, "ok", true);
                char* str = cJSON_PrintUnformatted(json);
                std::string result(str);
                cJSON_free(str);
                cJSON_Delete(json);
                return result;
            });

        mcp_server.AddTool(
            "self.ir.learn_start",
            "Begin learning an IR remote code. Returns {\"handle\": str}. "
            "Within ~5 seconds, point the user's existing remote at the "
            "device and press the button. Call self.ir.learn_result with "
            "the handle to retrieve the captured (protocol, code).",
            PropertyList(),
            [ir](const PropertyList& properties) -> ReturnValue {
                std::string handle = ir->LearnStart();
                if (handle.empty()) {
                    return MakeErrorJson("learn_start_failed");
                }
                cJSON* json = cJSON_CreateObject();
                cJSON_AddStringToObject(json, "handle", handle.c_str());
                char* str = cJSON_PrintUnformatted(json);
                std::string result(str);
                cJSON_free(str);
                cJSON_Delete(json);
                return result;
            });

        mcp_server.AddTool(
            "self.ir.learn_result",
            "Retrieve the result of a learn session. Returns "
            "{\"protocol\": str, \"code\": uint32} on success, or "
            "{\"ready\": false} when no signal has been captured yet (caller "
            "may poll), or {\"ok\": false, \"error\": \"timeout\"} when the "
            "learn window expired without an IR signal.",
            PropertyList({
                Property("handle", kPropertyTypeString),
            }),
            [ir](const PropertyList& properties) -> ReturnValue {
                std::string handle = properties["handle"].value<std::string>();
                std::string protocol;
                uint16_t address = 0, command = 0;
                if (!ir->LearnResult(handle, &protocol, &address, &command)) {
                    cJSON* json = cJSON_CreateObject();
                    cJSON_AddBoolToObject(json, "ready", false);
                    char* str = cJSON_PrintUnformatted(json);
                    std::string result(str);
                    cJSON_free(str);
                    cJSON_Delete(json);
                    return result;
                }
                uint32_t code = static_cast<uint32_t>(address)
                              | (static_cast<uint32_t>(command) << 16);
                cJSON* json = cJSON_CreateObject();
                cJSON_AddStringToObject(json, "protocol", protocol.c_str());
                cJSON_AddNumberToObject(json, "code", code);
                char* str = cJSON_PrintUnformatted(json);
                std::string result(str);
                cJSON_free(str);
                cJSON_Delete(json);
                return result;
            });
    }

    if (radar != nullptr) {
        mcp_server.AddTool(
            "self.radar.presence",
            "Read room presence from the SENSOR sub-board's mmWave radar. "
            "Returns {\"present\": bool, \"last_motion_at_s\": float}. "
            "`present` is true when motion is currently detected (radar OUT pin "
            "high). `last_motion_at_s` is seconds since boot of the most recent "
            "detection (-1 if never observed since this boot).",
            PropertyList(),
            [radar](const PropertyList& properties) -> ReturnValue {
                bool present = false;
                int64_t last_motion_ms = -1;
                esp_err_t err = radar->Read(&present, &last_motion_ms);
                if (err != ESP_OK) {
                    ESP_LOGW(TAG, "radar.presence read failed: %s",
                             esp_err_to_name(err));
                    return MakeErrorJson(esp_err_to_name(err));
                }
                cJSON* json = cJSON_CreateObject();
                cJSON_AddBoolToObject(json, "present", present);
                if (last_motion_ms < 0) {
                    cJSON_AddNumberToObject(json, "last_motion_at_s", -1);
                } else {
                    int64_t now_ms = esp_timer_get_time() / 1000;
                    double age_s = (now_ms - last_motion_ms) / 1000.0;
                    cJSON_AddNumberToObject(json, "last_motion_at_s", age_s);
                }
                char* str = cJSON_PrintUnformatted(json);
                std::string result(str);
                cJSON_free(str);
                cJSON_Delete(json);
                return result;
            });
    }
}
