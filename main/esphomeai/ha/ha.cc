#include "ha.h"
#include <esp_log.h>
#include <cJSON.h>
#include "eez_ui/ui/vars.h"

static const char* TAG = "HomeAssistant";

HomeAssistant::HomeAssistant(const std::string& broker_uri, const std::string& client_id,
                             const std::string& username, const std::string& password)
    : broker_uri_(broker_uri), client_id_(client_id), username_(username), 
      password_(password), temperature_(0.0), humidity_(0.0) {
    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.uri = broker_uri_.c_str();
    mqtt_cfg.credentials.client_id = client_id_.c_str();
    mqtt_cfg.credentials.username = username_.c_str();
    mqtt_cfg.credentials.authentication.password = password_.c_str();

    client_ = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client_, MQTT_EVENT_ANY, mqtt_event_handler, this);
}

HomeAssistant::~HomeAssistant() {
    esp_mqtt_client_destroy(client_);
}

bool HomeAssistant::connect() {
    return esp_mqtt_client_start(client_) == ESP_OK;
}

void HomeAssistant::disconnect() {
    esp_mqtt_client_stop(client_);
}

bool HomeAssistant::publish_state(const std::string& state) {
    std::string topic = "homeassistant/device/" + client_id_ + "/state";
    int msg_id = esp_mqtt_client_publish(client_, topic.c_str(), state.c_str(), 0, 1, 0);
    return msg_id != -1;
}

bool HomeAssistant::subscribe_weather() {
    std::string topic = "homeassistant/weather/home";
    int msg_id = esp_mqtt_client_subscribe(client_, topic.c_str(), 0);
    return msg_id != -1;
}

bool HomeAssistant::request_weather() {
    std::string topic = "xiaozhi/request";
    std::string message = "{\"action\": \"get_weather\"}";
    int msg_id = esp_mqtt_client_publish(client_, topic.c_str(), message.c_str(), 0, 1, 0);
    if (msg_id == -1) {
        ESP_LOGE(TAG, "Failed to publish weather request");
        return false;
    }
    ESP_LOGI(TAG, "Sent weather request to %s", topic.c_str());
    return true;
}

void HomeAssistant::on_message(const std::string& topic, const std::string& message) {
    if (topic == "homeassistant/weather/home") {
        cJSON* root = cJSON_Parse(message.c_str());
        if (root) {
            cJSON* temp = cJSON_GetObjectItem(root, "temperature");
            cJSON* hum = cJSON_GetObjectItem(root, "humidity");
            if (temp && hum) {
                temperature_ = static_cast<float>(atof(temp->valuestring));
                humidity_ = static_cast<float>(atof(hum->valuestring));
                set_var_temper_data(&temperature_);
                set_var_humidity_data(&humidity_);
                ESP_LOGI(TAG, "Received temperature: %.1f, humidity: %.1f", 
                        temperature_, humidity_);
            }
            cJSON_Delete(root);
        }
    }
}

float HomeAssistant::get_temperature() const {
    return temperature_;
}

float HomeAssistant::get_humidity() const {
    return humidity_;
}

void HomeAssistant::mqtt_event_handler(void* handler_args, esp_event_base_t base, 
                                     int32_t event_id, void* event_data) {
    HomeAssistant* ha = static_cast<HomeAssistant*>(handler_args);
    esp_mqtt_event_handle_t event = static_cast<esp_mqtt_event_handle_t>(event_data);

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected");
            ha->subscribe_weather();
            break;
        case MQTT_EVENT_DATA:
            ha->on_message(std::string(event->topic, event->topic_len), 
                         std::string(event->data, event->data_len));
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT disconnected");
            break;
        default:
            break;
    }
}