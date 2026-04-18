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

void InitializeSensorTools(Aht30* aht30, RadarMs58* radar) {
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
