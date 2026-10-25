#include "touch_mcp_tool.h"

#include <esp_log.h>

static const char* TAG = "TouchMcpTool";

TouchMcpTool::TouchMcpTool(esp_lcd_touch_handle_t handle) : handle_(handle) {}

void TouchMcpTool::Initialize() {
    if (handle_ == nullptr) {
        ESP_LOGW(TAG, "no touch handle; tool not registered");
        return;
    }

    auto& mcp_server = McpServer::GetInstance();
    mcp_server.AddTool(
        "self.touch.get_state",
        "Whether the screen is being touched right now, and where.\n"
        "Returns `touched`, the number of contact `points`, the panel's coordinate space as\n"
        "`x_max` and `y_max`, and a `coords` array of {x, y} in that space - origin at the\n"
        "top-left as the panel is mounted, so it already accounts for any rotation or mirroring\n"
        "the board configured.\n"
        "This is an instantaneous reading, not a history: a tap that began and ended between two\n"
        "calls is not reported.",
        PropertyList(),
        [this](const PropertyList& properties) -> ToolResult { return HandleGetState(); });

    ESP_LOGI(TAG, "TouchMcpTool initialized, panel %ux%u", handle_->config.x_max,
             handle_->config.y_max);
}

ToolResult TouchMcpTool::HandleGetState() {
    esp_err_t err = esp_lcd_touch_read_data(handle_);
    if (err != ESP_OK) {
        return std::unexpected(std::string("could not read the touch controller: ") +
                               esp_err_to_name(err));
    }

    // Initialised here rather than trusted: esp_lcd_touch_get_data() returns ESP_OK for an
    // untouched panel and leaves the count to the driver's get_xy callback, so a zero of our own
    // is what makes "not touched" distinguishable from "nobody wrote the count".
    esp_lcd_touch_point_data_t points[CONFIG_ESP_LCD_TOUCH_MAX_POINTS] = {};
    uint8_t count = 0;
    err = esp_lcd_touch_get_data(handle_, points, &count, CONFIG_ESP_LCD_TOUCH_MAX_POINTS);
    if (err != ESP_OK) {
        return std::unexpected(std::string("could not read the touch points: ") +
                               esp_err_to_name(err));
    }
    if (count > CONFIG_ESP_LCD_TOUCH_MAX_POINTS) {
        count = CONFIG_ESP_LCD_TOUCH_MAX_POINTS;
    }

    cJSON* root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "touched", count > 0);
    cJSON_AddNumberToObject(root, "points", count);
    cJSON_AddNumberToObject(root, "x_max", handle_->config.x_max);
    cJSON_AddNumberToObject(root, "y_max", handle_->config.y_max);

    cJSON* coords = cJSON_AddArrayToObject(root, "coords");
    for (uint8_t i = 0; i < count; i++) {
        cJSON* point = cJSON_CreateObject();
        cJSON_AddNumberToObject(point, "x", points[i].x);
        cJSON_AddNumberToObject(point, "y", points[i].y);
        cJSON_AddItemToArray(coords, point);
    }

    return root;
}
