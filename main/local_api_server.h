#ifndef LOCAL_API_SERVER_H
#define LOCAL_API_SERVER_H

#include <esp_http_server.h>

#include <functional>
#include <optional>
#include <string>

/**
 * Local HTTP API served by the device itself (enabled with CONFIG_ENABLE_LOCAL_API).
 *
 * Endpoints (all JSON):
 *   GET  /api/info              firmware, board, network and heap information
 *   GET  /api/status            Board::GetDeviceStatusJson()
 *   GET  /api/tools             every MCP tool (including user-only tools)
 *   POST /api/tools/<name>      call an MCP tool; body = arguments object (may be empty)
 *   POST /api/mcp               raw MCP JSON-RPC 2.0 request, raw response
 *   POST /api/chat/start        start a conversation; optional body {"text":"..."}
 *   POST /api/chat/toggle       same as pressing the chat button
 *   POST /api/chat/abort        interrupt the assistant while it is speaking
 *
 * If CONFIG_LOCAL_API_TOKEN is not empty, every request must send
 * "Authorization: Bearer <token>" (or "X-Api-Token: <token>").
 *
 * Anything touching application state runs on the main task through
 * Application::Schedule(); the HTTP task only waits for the result.
 */
class LocalApiServer {
public:
    static LocalApiServer& GetInstance() {
        static LocalApiServer instance;
        return instance;
    }
    LocalApiServer(const LocalApiServer&) = delete;
    LocalApiServer& operator=(const LocalApiServer&) = delete;

    // Idempotent: safe to call on every "network connected" event.
    bool Start();
    void Stop();
    bool IsRunning() const { return server_ != nullptr; }

private:
    LocalApiServer() = default;
    ~LocalApiServer() { Stop(); }

    // Runs `fn` on the main task and waits for the string it reports through the callback.
    // Returns std::nullopt on timeout.
    static std::optional<std::string> RunOnMainTask(
        std::function<void(std::function<void(const std::string&)>)> fn, int timeout_ms);
    static std::optional<std::string> CallMcp(const std::string& request);

    static bool CheckAuth(httpd_req_t* req);
    static void SetCommonHeaders(httpd_req_t* req);
    static esp_err_t SendJson(httpd_req_t* req, const char* status, const std::string& body);
    static esp_err_t SendError(httpd_req_t* req, const char* status, const std::string& message);
    static std::optional<std::string> ReadBody(httpd_req_t* req, std::string& error_status);

    static esp_err_t HandleInfo(httpd_req_t* req);
    static esp_err_t HandleStatus(httpd_req_t* req);
    static esp_err_t HandleToolsList(httpd_req_t* req);
    static esp_err_t HandleToolCall(httpd_req_t* req);
    static esp_err_t HandleMcp(httpd_req_t* req);
    static esp_err_t HandleChatStart(httpd_req_t* req);
    static esp_err_t HandleChatToggle(httpd_req_t* req);
    static esp_err_t HandleChatAbort(httpd_req_t* req);
    static esp_err_t HandleOptions(httpd_req_t* req);

    httpd_handle_t server_ = nullptr;
};

#endif  // LOCAL_API_SERVER_H
