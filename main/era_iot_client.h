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
     * @param value Value to send (1 for ON, 0 for OFF)
     * @return true if successful, false otherwise
     */
    bool TriggerAction(const std::string &action_key, int value);

    /**
     * Get status of a specific switch (1-3)
     * @param index Switch index (1-3)
     * @return Status string
     */
    std::string GetSwitchStatus(int index);

    /**
     * Turn a specific switch ON (1-3)
     * @param index Switch index (1-3)
     * @return true if successful
     */
    bool TurnSwitchOn(int index);

    /**
     * Turn a specific switch OFF (1-3)
     * @param index Switch index (1-3)
     * @return true if successful
     */
    bool TurnSwitchOff(int index);

    /**
     * Check if client is initialized
     */
    bool IsInitialized() const { return initialized_; }

private:
    std::string auth_token_;
    std::string base_url_;
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