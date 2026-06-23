#ifndef _DEVICE_API_CLIENT_H_
#define _DEVICE_API_CLIENT_H_

#include <string>
#include <functional>

/**
 * Device API Client for Agent Studio pairing and conversation management.
 * 
 * Lifecycle:
 *   1. Pairing: POST /devices/pair-codes → get pair_code + pair_token
 *   2. Claim:   Poll GET /devices/{device_id}/binding-status → get device_token
 *   3. Runtime: POST /devices/{device_id}/conversations/start → get RTC params
 *              POST /devices/{device_id}/conversations/stop → end conversation
 */

struct RtcConfig {
    std::string app_id;
    std::string channel;
    std::string token;
    std::string uid;
    std::string agent_uid;
};

struct ConversationInfo {
    std::string conversation_id;
    std::string agent_id;
    RtcConfig rtc;
};

enum class DeviceApiError {
    kNone = 0,
    kNetworkError,
    kValidationError,
    kUnauthenticated,
    kForbidden,
    kNotBound,
    kTokenRevoked,
    kRateLimited,
    kServerError,
    kExpired,
};

class DeviceApiClient {
public:
    DeviceApiClient();
    ~DeviceApiClient() = default;

    // Get device_id (AG-XXXXXXXXXXXX format)
    const std::string& GetDeviceId() const { return device_id_; }

    // Check if device has a valid device_token persisted in NVS
    bool HasDeviceToken() const;

    // Phase 1: Request pair code. Returns the 6-digit code on success, empty on failure.
    std::string RequestPairCode();

    // Phase 2: Poll binding status. Returns true when bound (device_token saved to NVS).
    // Returns false if still pending. Sets error on expiry/failure.
    // poll_after_seconds is set to the recommended poll interval.
    enum class PollResult {
        kPending,
        kBound,
        kExpired,
        kUnbound,   // Device has been unbound from agent (clear token, re-pair)
        kError,
    };
    PollResult PollBindingStatus(int& poll_after_seconds);

    // Phase 3: Start a conversation. Returns true on success with info populated.
    bool StartConversation(ConversationInfo& info);

    // Phase 3: Stop a conversation.
    bool StopConversation(const std::string& conversation_id, const std::string& reason = "device_hangup");

    // Clear all credentials (device_token, pair_token) and return to unprovisioned state.
    void ClearCredentials();

    // Get last error
    DeviceApiError GetLastError() const { return last_error_; }

private:
    std::string device_id_;
    std::string pair_token_;       // Temporary, memory only
    std::string device_token_;     // Persisted in NVS
    DeviceApiError last_error_ = DeviceApiError::kNone;

    void LoadDeviceToken();
    void SaveDeviceToken(const std::string& token);
    std::string BuildUrl(const std::string& path) const;
    DeviceApiError ParseErrorResponse(int status_code, const std::string& body);
};

#endif // _DEVICE_API_CLIENT_H_
