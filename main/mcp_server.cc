/*
 * MCP Server Implementation
 * Revisions: 2024-11-05 (legacy) and 2026-07-28 (per-request metadata)
 */

static std::string g_answer_mode = "simple";

ToolResult SetAnswerMode(const PropertyList& props)
{
    std::string mode = props["mode"].value<std::string>();
    if (mode == "professional" || mode == "simple")
    {
        g_answer_mode = mode;
        ESP_LOGI("MY_MODE", "模式已切换为：%s", g_answer_mode.c_str());
        return "切换成功，当前模式:" + g_answer_mode;
    }
    return std::unexpected("模式错误，仅支持 professional / simple");
}
#include "mcp_server.h"
#include <esp_app_desc.h>
#include <esp_log.h>
#include <algorithm>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string_view>

#include "application.h"
#include "board.h"

#define TAG "MCP"

McpServer::McpServer() {}

McpServer::~McpServer() = default;

void McpServer::LogToolInventory() const {
    std::lock_guard lock(tools_mutex_);
    size_t user_only = 0;
    for (const auto& tool : tools_) {
        if (tool->user_only()) {
            ++user_only;
        }
    }
    ESP_LOGI(TAG, "%u tools registered: %u visible to the model, %u user-only",
             (unsigned)tools_.size(), (unsigned)(tools_.size() - user_only), (unsigned)user_only);
}

void McpServer::AddTool(std::unique_ptr<McpTool> tool) {
    std::lock_guard lock(tools_mutex_);
    // Prevent adding duplicate tools
    if (std::find_if(tools_.begin(), tools_.end(), [&tool](const auto& existing) {
            return existing->name() == tool->name();
        }) != tools_.end()) {
        ESP_LOGW(TAG, "Tool %s already added", tool->name().c_str());
        return;
    }

    ESP_LOGI(TAG, "Add tool: %s%s", tool->name().c_str(), tool->user_only() ? " [user]" : "");
    tools_.push_back(std::move(tool));
}

void McpServer::RemoveTool(std::string_view name) {
    std::lock_guard lock(tools_mutex_);
    std::erase_if(tools_, [name](const auto& tool) { return tool->name() == name; });
}

std::shared_ptr<McpTool> McpServer::FindTool(std::string_view name) const {
    std::lock_guard lock(tools_mutex_);
    auto tool = std::find_if(tools_.begin(), tools_.end(),
                             [name](const auto& item) { return item->name() == name; });
    return tool == tools_.end() ? nullptr : *tool;
}

void McpServer::AddTool(const std::string& name, const std::string& description,
                        const PropertyList& properties, ToolCallback callback) {
    AddTool(std::make_unique<McpTool>(name, description, properties, std::move(callback)));
}

void McpServer::AddTool(const std::string& name, const std::string& description,
                        const PropertyList& properties, ContextToolCallback callback) {
    AddTool(std::make_unique<McpTool>(name, description, properties, callback));
}

void McpServer::AddUserOnlyTool(const std::string& name, const std::string& description,
                                const PropertyList& properties, ToolCallback callback) {
    auto tool = std::make_unique<McpTool>(name, description, properties, std::move(callback));
    tool->set_user_only(true);
    AddTool(std::move(tool));
}

namespace {
constexpr const char* kProtocolVersion = "io.modelcontextprotocol/protocolVersion";
constexpr const char* kClientCapabilities = "io.modelcontextprotocol/clientCapabilities";
constexpr const char* kVisionExtension = "me.xiaozhi/vision";
constexpr const char* kModernVersion = "2026-07-28";
constexpr const char* kSupportedVersions = R"(["2026-07-28","2024-11-05"])";

std::string JsonString(const char* value) {
    CJsonUniquePtr string(cJSON_CreateString(value));
    CJsonStringUniquePtr encoded(string ? cJSON_PrintUnformatted(string.get()) : nullptr);
    return encoded ? std::string(encoded.get()) : std::string();
}

void AppendServerInfo(std::string& output) {
    output += "{\"name\":";
    output += JsonString(BOARD_NAME);
    output += ",\"version\":";
    output += JsonString(esp_app_get_description()->version);
    output += "}";
}

std::string ResultPrefix(const McpRequestContext& context) {
    std::string prefix = "{\"jsonrpc\":\"2.0\",\"id\":";
    prefix += context.id_json;
    prefix += ",\"result\":";
    if (context.modern) {
        prefix += "{\"resultType\":\"complete\",\"_meta\":{\"io.modelcontextprotocol/serverInfo\":";
        AppendServerInfo(prefix);
        prefix += "}}";
    }
    return prefix;
}

std::optional<McpRequestContext::VisionConfig> ReadVisionConfig(const cJSON* vision) {
    auto url = cJSON_GetObjectItemCaseSensitive(vision, "url");
    auto token = cJSON_GetObjectItemCaseSensitive(vision, "token");
    if (!cJSON_IsString(url)) {
        return std::nullopt;
    }
    return McpRequestContext::VisionConfig{url->valuestring,
                                           cJSON_IsString(token) ? token->valuestring : ""};
}

bool ValidVisionConfig(const McpRequestContext::VisionConfig& vision) {
    const auto& url = vision.url;
    const auto scheme_size = url.starts_with("https://")  ? 8u
                             : url.starts_with("http://") ? 7u
                                                          : 0u;
    if (scheme_size == 0 || url.size() <= scheme_size || url[scheme_size] == '/' ||
        url[scheme_size] == '?' || url[scheme_size] == '#') {
        return false;
    }
    return std::none_of(url.begin(), url.end(),
                        [](unsigned char c) { return std::isspace(c) || std::iscntrl(c); }) &&
           std::none_of(vision.token.begin(), vision.token.end(),
                        [](unsigned char c) { return c == '\r' || c == '\n'; });
}
}  // namespace

void McpServer::ParseMessage(const std::string& message, ResponseSender response_sender) {
    CJsonUniquePtr json(cJSON_ParseWithOpts(message.c_str(), nullptr, true));
    if (json == nullptr) {
        // Requests may contain credentials. Never log the original message.
        ReplyError({}, -32700, "Parse error", response_sender);
        return;
    }
    ParseMessage(json.get(), std::move(response_sender));
}

void McpServer::ParseCapabilities(const cJSON* capabilities) {
    auto config = ReadVisionConfig(cJSON_GetObjectItem(capabilities, "vision"));
    if (config) {
        if (auto camera = Board::GetInstance().GetCamera()) {
            camera->SetExplainUrl(config->url, config->token);
        }
    }
}

void McpServer::ParseMessage(const cJSON* json, ResponseSender response_sender) {
    McpRequestContext context;
    auto id = cJSON_GetObjectItemCaseSensitive(json, "id");
    if (cJSON_IsString(id)) {
        context.id_json = JsonString(id->valuestring);
    } else if (cJSON_IsNumber(id) && std::isfinite(id->valuedouble) &&
               std::trunc(id->valuedouble) == id->valuedouble &&
               std::abs(id->valuedouble) <= 9007199254740991.0) {
        // cJSON's floating formatter can round large safe integers. IDs must round-trip exactly.
        context.id_json = std::to_string(static_cast<int64_t>(id->valuedouble));
    }
    auto version = cJSON_GetObjectItemCaseSensitive(json, "jsonrpc");
    auto method = cJSON_GetObjectItemCaseSensitive(json, "method");
    if (!cJSON_IsObject(json) || !cJSON_IsString(version) ||
        strcmp(version->valuestring, "2.0") != 0 || !cJSON_IsString(method)) {
        ReplyError(context, -32600, "Invalid Request", response_sender);
        return;
    }
    if (id == nullptr) {
        return;  // Notifications never execute request-only tools or receive a response.
    }
    if (context.id_json.empty()) {
        ReplyError(context, -32600, "Invalid request ID", response_sender);
        return;
    }

    const std::string_view method_name(method->valuestring);
    if (method_name.starts_with("notifications/")) {
        return;  // Retain compatibility with notifications/initialized.
    }
    auto params = cJSON_GetObjectItemCaseSensitive(json, "params");
    if (params != nullptr && !cJSON_IsObject(params)) {
        ReplyError(context, -32602, "Invalid params", response_sender);
        return;
    }
    auto meta = cJSON_GetObjectItemCaseSensitive(params, "_meta");
    auto protocol_version = cJSON_GetObjectItemCaseSensitive(meta, kProtocolVersion);
    auto capabilities = cJSON_GetObjectItemCaseSensitive(meta, kClientCapabilities);
    context.modern = method_name == "server/discover" || protocol_version || capabilities ||
                     cJSON_GetObjectItemCaseSensitive(meta, "io.modelcontextprotocol/clientInfo") ||
                     cJSON_GetObjectItemCaseSensitive(meta, "io.modelcontextprotocol/logLevel");
    if ((meta && !cJSON_IsObject(meta)) ||
        (context.modern && (!cJSON_IsString(protocol_version) || !cJSON_IsObject(capabilities)))) {
        ReplyError(context, -32602, "Missing or invalid MCP request metadata", response_sender);
        return;
    }
    if (context.modern && strcmp(protocol_version->valuestring, kModernVersion) != 0) {
        std::string data = "{\"supported\":";
        data += kSupportedVersions;
        data += ",\"requested\":";
        data += JsonString(protocol_version->valuestring);
        data += "}";
        ReplyError(context, -32022, "Unsupported protocol version", response_sender, data);
        return;
    }
    auto extensions = cJSON_GetObjectItemCaseSensitive(capabilities, "extensions");
    auto client_info = cJSON_GetObjectItemCaseSensitive(meta, "io.modelcontextprotocol/clientInfo");
    if (context.modern &&
        ((extensions && !cJSON_IsObject(extensions)) ||
         (client_info &&
          (!cJSON_IsObject(client_info) ||
           !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(client_info, "name")) ||
           !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(client_info, "version")))))) {
        ReplyError(context, -32602, "Invalid MCP client capabilities or identity", response_sender);
        return;
    }

    if (method_name == "initialize" && !context.modern) {
        ParseCapabilities(cJSON_GetObjectItem(params, "capabilities"));
        std::string result =
            "{\"protocolVersion\":\"2024-11-05\",\"capabilities\":{\"tools\":{}},\"serverInfo\":";
        AppendServerInfo(result);
        result += "}";
        ReplyResult(context, result, response_sender);
    } else if (method_name == "server/discover") {
        std::string result = "{\"supportedVersions\":";
        result += kSupportedVersions;
        result += ",\"capabilities\":{\"tools\":{}";
        if (FindTool("self.camera.take_photo")) {
            result += ",\"extensions\":{\"me.xiaozhi/vision\":{}}";
        }
        result += "},\"ttlMs\":60000,\"cacheScope\":\"private\"}";
        ReplyResult(context, result, response_sender);
    } else if (method_name == "tools/list") {
        auto cursor = cJSON_GetObjectItemCaseSensitive(params, "cursor");
        auto with_user_tools = cJSON_GetObjectItemCaseSensitive(params, "withUserTools");
        if (context.modern && ((cursor && !cJSON_IsString(cursor)) ||
                               (with_user_tools && !cJSON_IsBool(with_user_tools)))) {
            ReplyError(context, -32602, "Invalid tool-list parameters", response_sender);
            return;
        }
        GetToolsList(context, cJSON_IsString(cursor) ? cursor->valuestring : "",
                     cJSON_IsTrue(with_user_tools), response_sender);
    } else if (method_name == "tools/call") {
        auto name = cJSON_GetObjectItemCaseSensitive(params, "name");
        auto arguments = cJSON_GetObjectItemCaseSensitive(params, "arguments");
        if (!cJSON_IsString(name) || (arguments && !cJSON_IsObject(arguments))) {
            ReplyError(context, -32602, "Invalid tool name or arguments", response_sender);
            return;
        }
        auto tool = FindTool(name->valuestring);
        if (!tool) {
            std::string error = "Unknown tool: ";
            error += name->valuestring;
            ReplyError(context, -32602, error, response_sender);
            return;
        }
        if (context.modern && tool->name() == "self.camera.take_photo") {
            auto vision = cJSON_GetObjectItemCaseSensitive(extensions, kVisionExtension);
            if (!vision) {
                ReplyError(context, -32021, "Missing required client capability", response_sender,
                           R"({"requiredCapabilities":{"extensions":{"me.xiaozhi/vision":{}}}})");
                return;
            }
            auto token = cJSON_GetObjectItemCaseSensitive(vision, "token");
            context.vision = ReadVisionConfig(vision);
            if (!cJSON_IsObject(vision) || !context.vision || (token && !cJSON_IsString(token)) ||
                !ValidVisionConfig(*context.vision)) {
                ReplyError(context, -32602, "Invalid vision configuration", response_sender);
                return;
            }
        }
        DoToolCall(std::move(context), std::move(tool), arguments, std::move(response_sender));
    } else {
        ReplyError(context, -32601, "Method not implemented: " + std::string(method_name),
                   response_sender);
    }
}

void McpServer::SendResponse(const std::string& payload, const ResponseSender& response_sender) {
    if (response_sender) {
        response_sender(payload);
    } else {
        Application::GetInstance().SendMcpMessage(payload);
    }
}

void McpServer::ReplyResult(const McpRequestContext& context, const std::string& result,
                            const ResponseSender& response_sender) {
    auto payload = ResultPrefix(context);
    // Include the final brace so appending it cannot double a large response buffer.
    payload.reserve(payload.size() + result.size() + 1);
    if (context.modern) {
        // Merge known result objects into the envelope without parsing/copying a second JSON tree.
        payload.pop_back();
        if (result.size() > 2) {
            payload += ",";
        }
        payload.append(result, 1, std::string::npos);
    } else {
        payload += result;
    }
    payload += "}";
    SendResponse(payload, response_sender);
}

void McpServer::ReplyError(const McpRequestContext& context, int code, const std::string& message,
                           const ResponseSender& response_sender, const std::string& data) {
    auto encoded = JsonString(message.c_str());
    if (encoded.empty()) {
        return;
    }
    std::string payload;
    payload.reserve(context.id_json.size() + encoded.size() + data.size() + 80);
    payload += "{\"jsonrpc\":\"2.0\"";
    if (!context.id_json.empty()) {
        payload += ",\"id\":";
        payload += context.id_json;
    }
    payload += ",\"error\":{\"code\":";
    payload += std::to_string(code);
    payload += ",\"message\":";
    payload += encoded;
    if (!data.empty()) {
        payload += ",\"data\":";
        payload += data;
    }
    payload += "}}";
    SendResponse(payload, response_sender);
}

void McpServer::ReplyError(const McpRequestContext& context, const std::string& message,
                           const ResponseSender& response_sender) {
    ReplyError(context, -32603, message, response_sender);
}

void McpServer::GetToolsList(const McpRequestContext& context, const std::string& cursor,
                             bool list_user_only_tools, const ResponseSender& response_sender) {
    constexpr size_t kMaxPayloadSize = 8000;
    const size_t envelope_size = ResultPrefix(context).size() + 2;
    const std::string suffix = context.modern ? R"(],"ttlMs":60000,"cacheScope":"private")" : "]";
    std::vector<std::shared_ptr<McpTool>> tools;
    {
        std::lock_guard lock(tools_mutex_);
        tools = tools_;
    }
    auto it = tools.begin();
    if (!cursor.empty()) {
        it =
            std::find_if(it, tools.end(), [&](const auto& tool) { return tool->name() == cursor; });
        if (it == tools.end()) {
            ReplyError(context, -32602, "Invalid cursor", response_sender);
            return;
        }
    }
    auto visible = [&](auto position) {
        while (position != tools.end() && !list_user_only_tools && (*position)->user_only()) {
            ++position;
        }
        return position;
    };
    it = visible(it);
    std::string json = "{\"tools\":[";
    std::string next_cursor;
    bool added = false;
    for (; it != tools.end();) {
        auto next = visible(it + 1);
        auto tool_json = (*it)->to_json();
        const auto cursor_json = next == tools.end() ? "" : JsonString((*next)->name().c_str());
        const size_t reserved = cursor_json.empty() ? 0 : cursor_json.size() + 14;
        if (tool_json.empty() ||
            json.size() + tool_json.size() + suffix.size() + reserved + envelope_size + 2 >
                kMaxPayloadSize) {
            if (!added) {
                ReplyError(context, "Tool definition exceeds payload limit", response_sender);
                return;
            }
            next_cursor = JsonString((*it)->name().c_str());
            break;
        }
        if (added) {
            json += ",";
        }
        json += tool_json;
        added = true;
        it = next;
    }
    json += suffix;
    if (!next_cursor.empty()) {
        json += ",\"nextCursor\":";
        json += next_cursor;
    }
    json += "}";
    ReplyResult(context, json, response_sender);
}

void McpServer::DoToolCall(McpRequestContext context, std::shared_ptr<McpTool> tool,
                           const cJSON* tool_arguments, ResponseSender response_sender) {
    PropertyList arguments = tool->properties();
    for (auto& argument : arguments) {
        bool found = false;
        std::expected<void, std::string> validation;
        if (cJSON_IsObject(tool_arguments)) {
            auto value = cJSON_GetObjectItem(tool_arguments, argument.name().c_str());
            if (argument.type() == kPropertyTypeBoolean && cJSON_IsBool(value)) {
                validation = argument.set_value<bool>(value->valueint == 1);
                found = true;
            } else if (argument.type() == kPropertyTypeInteger && cJSON_IsNumber(value) &&
                       (!context.modern ||
                        (std::isfinite(value->valuedouble) &&
                         std::trunc(value->valuedouble) == value->valuedouble &&
                         value->valuedouble >= INT_MIN && value->valuedouble <= INT_MAX))) {
                validation = argument.set_value<int>(value->valueint);
                found = true;
            } else if (argument.type() == kPropertyTypeString && cJSON_IsString(value)) {
                validation = argument.set_value<std::string>(value->valuestring);
                found = true;
            } else if (value != nullptr) {
                ReplyError(context, -32602, "Invalid type for argument: " + argument.name(),
                           response_sender);
                return;
            }
        }
        if (found && !validation) {
            ReplyError(context, -32602, validation.error(), response_sender);
            return;
        }
        if (!argument.has_default_value() && !found) {
            ReplyError(context, -32602, "Missing required argument: " + argument.name(),
                       response_sender);
            return;
        }
    }
    Application::GetInstance().Schedule([this, context = std::move(context), tool = std::move(tool),
                                         arguments = std::move(arguments),
                                         response_sender = std::move(response_sender)]() {
        auto result = tool->Call(arguments, context);
        if (!result) {
            if (context.modern) {
                std::string error = "{\"isError\":true,\"content\":[{\"type\":\"text\",\"text\":";
                error += JsonString(result.error().c_str());
                error += "}]}";
                ReplyResult(context, error, response_sender);
            } else {
                ReplyError(context, result.error(), response_sender);
            }
            return;
        }
        ReplyResult(context, *result, response_sender);
    });
}
