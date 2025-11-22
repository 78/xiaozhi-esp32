#ifndef ERA_IOT_CLIENT_H
#define ERA_IOT_CLIENT_H

#include <string>
#include <memory>

/**
 * E-Ra IoT API Client
 * Provides interface to communicate with E-Ra IoT platform
 *
 * API Documentation: https://e-ra-iot-wiki.gitbook.io/documentation/x.-public-e-ra-api/api
 * Base URL: https://backend.eoh.io
 */
class EraIotClient
{
public:
    EraIotClient();
    ~EraIotClient();

    /**
     * Initialize the E-Ra client with authentication token
     * @param auth_token Authentication token with "Token " prefix
     * @param base_url Base URL for E-Ra API (default: https://backend.eoh.io)
     */
    void Initialize(const std::string &auth_token, const std::string &base_url = "https://backend.eoh.io");

    /**
     * Get current value of a config
     * API: GET /api/chip_manager/configs/{id}/current_value/
     * @param config_id Config ID
     * @return Current value as string, empty if failed
     */
    std::string GetCurrentValue(const std::string &config_id);

    /**
     * Trigger an action
     * API: POST /api/chip_manager/trigger_action/
     * @param action_key Action key to trigger
     * @return true if successful, false otherwise
     */
    bool TriggerAction(const std::string &action_key);

    /**
     * Turn device ON
     * Uses predefined action_on key
     */
    bool TurnDeviceOn();

    /**
     * Turn device OFF
     * Uses predefined action_off key
     */
    bool TurnDeviceOff();

    /**
     * Check if client is initialized
     */
    bool IsInitialized() const { return initialized_; }

private:
    std::string auth_token_;
    std::string base_url_;
    std::string config_id_;
    std::string action_on_key_;
    std::string action_off_key_;
    bool initialized_;

    /**
     * Make HTTP request to E-Ra API
     * @param method HTTP method (GET, POST)
     * @param endpoint API endpoint
     * @param payload JSON payload for POST requests
     * @return Response body or empty if failed
     */
    std::string MakeRequest(const std::string &method, const std::string &endpoint, const std::string &payload = "");
};

#endif // ERA_IOT_CLIENT_H