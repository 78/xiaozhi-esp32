#include "stepper_polling.h"
#include <esp_log.h>
#include <cstring>
#include <cstdio>
#include <esp_timer.h>
#include "board.h"
#include "settings.h"
#include <mqtt.h>

StepperPolling::StepperPolling() : last_update_time_us_(0), last_trigger_time_us_(0) {
    ESP_LOGI(TAG, "StepperPolling initialized");
    // HiveMQ initialization is deferred to InitializeHiveMqttAsync()
    // to ensure network stack is ready
}

StepperPolling::~StepperPolling() {
    hivemq_mqtt_.reset();
    ESP_LOGI(TAG, "StepperPolling destroyed");
}

void StepperPolling::OnMqttMessage(const std::string& payload) {
    // Parse plain integer string (e.g., "50")
    ESP_LOGD(TAG, "OnMqttMessage: payload=%s", payload.c_str());
    try {
        int32_t count = std::stoi(payload);
        int32_t prev_count = current_count_.load();
        
        current_count_.store(count);
        last_update_time_us_ = esp_timer_get_time();
        ESP_LOGD(TAG, "Parsed step count: %ld (prev: %ld)", count, prev_count);
        
        if (count != prev_count) {
            ESP_LOGI(TAG, "Step count updated: %ld -> %ld", prev_count, count);
        }
    } catch (const std::exception& e) {
        ESP_LOGW(TAG, "Failed to parse step count from payload: %s, error: %s",
                 payload.c_str(), e.what());
    }
}

bool StepperPolling::CheckAndTrigger() {
    int32_t current = current_count_.load();
    int32_t last_triggered = last_triggered_count_.load();
    int64_t now = esp_timer_get_time();
    
    // Calculate delta (absolute change)
    int32_t delta = current > last_triggered ? 
                    (current - last_triggered) : 
                    (last_triggered - current);
    
    // Log every check for debugging (verbose)
    if (delta >= MIN_DELTA) {
        ESP_LOGD(TAG, "CheckAndTrigger: current=%ld, last_triggered=%ld, delta=%ld (meets threshold)",
                 current, last_triggered, delta);
    }
    
    // Check if delta exceeds threshold and debounce time has passed
    if (delta >= MIN_DELTA) {
        uint32_t time_since_last_trigger_ms = 
            (now - last_trigger_time_us_) / 1000;
        
        if (time_since_last_trigger_ms >= DEBOUNCE_MS) {
            // Update last triggered count
            last_triggered_count_.store(current);
            last_trigger_time_us_ = now;
            
            ESP_LOGI(TAG, "Trigger announcement: count=%ld, delta=%ld",
                     current, delta);
            
            // Invoke callback if registered
            if (trigger_callback_) {
                trigger_callback_(current, delta);
            }
            
            return true;
        }
    }
    
    return false;
}

void StepperPolling::Reset() {
    current_count_.store(0);
    last_triggered_count_.store(0);
    last_update_time_us_ = 0;
    last_trigger_time_us_ = 0;
    ESP_LOGI(TAG, "StepperPolling state reset");
}

void StepperPolling::InitializeHiveMqttAsync() {
    if (hivemq_mqtt_ != nullptr) {
        ESP_LOGW(TAG, "HiveMQ MQTT already initialized");
        return;
    }
    
    try {
        // Create MQTT client for HiveMQ Cloud connection
        auto network = Board::GetInstance().GetNetwork();
        if (network == nullptr) {
            ESP_LOGW(TAG, "Network not available, skipping HiveMQ MQTT initialization");
            return;
        }
        
        hivemq_mqtt_ = network->CreateMqtt(1);  // Use ID 1 to distinguish from main protocol
        if (hivemq_mqtt_ == nullptr) {
            ESP_LOGW(TAG, "Failed to create HiveMQ MQTT client");
            return;
        }

        // HiveMQ Cloud configuration
        const char* broker_address = "ecec39fe74d148a88be3eef240c88164.s1.eu.hivemq.cloud";
        int broker_port = 8883;
        // Generate unique client_id with timestamp to avoid collisions
        static char client_id_buf[64];
        snprintf(client_id_buf, sizeof(client_id_buf), "pico_%lld", (long long)esp_timer_get_time() / 1000000);
        const char* client_id = client_id_buf;
        const char* username = "IFF_team";
        const char* password = "Ab1234567.";

        // Use aggressive keep-alive to maintain connection
        hivemq_mqtt_->SetKeepAlive(30);  // Reduced from 60 to 30 seconds

        // Set up message callback to handle stepper/count topic
        hivemq_mqtt_->OnMessage([this](const std::string& topic, const std::string& payload) {
            if (topic == "stepper/count") {
                ESP_LOGI(TAG, "Received stepper/count from HiveMQ: %s", payload.c_str());
                this->OnMqttMessage(payload);
            }
        });

        // Set up connection callbacks
        hivemq_mqtt_->OnConnected([this]() {
            ESP_LOGI(TAG, "HiveMQ MQTT connected, subscribing to stepper/count");
            // Subscribe to stepper/count topic with QoS 1
            if (this->hivemq_mqtt_) {
                int msg_id = this->hivemq_mqtt_->Subscribe("stepper/count", 1);
                if (msg_id > 0) {
                    ESP_LOGI(TAG, "Subscribe request sent (msg_id=%d) for stepper/count", msg_id);
                } else {
                    ESP_LOGW(TAG, "Subscribe failed (msg_id=%d) for stepper/count", msg_id);
                }
            }
        });

        hivemq_mqtt_->OnDisconnected([this]() {
            ESP_LOGI(TAG, "HiveMQ MQTT disconnected");
        });

        // Connect to HiveMQ Cloud
        ESP_LOGI(TAG, "Connecting to HiveMQ Cloud at %s:%d", broker_address, broker_port);
        if (hivemq_mqtt_->Connect(broker_address, broker_port, client_id, username, password)) {
            ESP_LOGI(TAG, "HiveMQ MQTT connect initiated");
        } else {
            ESP_LOGW(TAG, "Failed to initiate HiveMQ MQTT connection");
        }
    } catch (const std::exception& e) {
        ESP_LOGW(TAG, "Exception during HiveMQ MQTT initialization: %s", e.what());
    }
}
