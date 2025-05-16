#ifndef HA_H
#define HA_H

#include <string>
#include <mqtt_client.h>

class HomeAssistant {
public:
    HomeAssistant(const std::string& broker_uri, const std::string& client_id,
                  const std::string& username, const std::string& password);
    ~HomeAssistant();

    bool connect();
    void disconnect();
    bool publish_state(const std::string& state);
    bool subscribe_weather();
    bool request_weather(); // New method to request weather

    // Getters for temperature and humidity
    float get_temperature() const;
    float get_humidity() const;

private:
    // MQTT event handler
    static void mqtt_event_handler(void* handler_args, esp_event_base_t base, 
                                 int32_t event_id, void* event_data);
    void on_message(const std::string& topic, const std::string& message);

    esp_mqtt_client_handle_t client_;
    std::string broker_uri_;
    std::string client_id_;
    std::string username_;
    std::string password_;
    float temperature_;
    float humidity_;
};

#endif // HA_H