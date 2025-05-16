#include "esphomeai.h"
#include "eez_ui/ui/ui.h"
#include "eez_ui/ui/screens.h"
#include <esp_log.h>

static const char* TAG = "ESPHomeAI";

ESPHomeAI::ESPHomeAI() {
    // Initialize HomeAssistant with MQTT broker details
    ha_ = new HomeAssistant(
        "mqtt://192.168.1.114:1883", // Replace with your MQTT broker URI
        "xiaozhi-ai",          // Unique client ID
        "ha",              // Replace with your MQTT username
        "sora"               // Replace with your MQTT password
    );

    if (ha_->connect()) {
        ha_->request_weather(); // Request weather upon connection
        ESP_LOGI(TAG, "HomeAssistant connected successfully");
    } else {
        ESP_LOGE(TAG, "Failed to connect to HomeAssistant MQTT");
    }
}

ESPHomeAI::~ESPHomeAI() {
    if (ha_) {
        ha_->disconnect();
        delete ha_;
        ha_ = nullptr;
    }
}

void ESPHomeAI::SetupUI(lv_obj_t **screen, lv_obj_t **container_, lv_obj_t **status_bar_, lv_obj_t **content_)
{
    ui_init();
    *screen = objects.main;
    *container_ = objects.container_;
    *status_bar_ = objects.status_bar;
    *content_ = objects.content_;
}

void ESPHomeAI::UpdateUI()
{
    ui_tick();
}

void ESPHomeAI::publish_device_state(const std::string& state) {
    if (ha_ && !ha_->publish_state(state)) {
        ESP_LOGE(TAG, "Failed to publish device state: %s", state.c_str());
    }
}
