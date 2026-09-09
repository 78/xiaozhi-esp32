#ifndef MCP_SERVER_H
#define MCP_SERVER_H

#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include <esp_system.h>
#include <cJSON.h>
#include <mbedtls/base64.h>

#include "cjson_utils.h"

class ImageContent {
private:
    std::string encoded_data_;
    std::string mime_type_;

    static std::string Base64Encode(const std::string& data) {
        size_t dlen = 0, olen = 0;
        mbedtls_base64_encode((unsigned char*)nullptr, 0, &dlen, (const unsigned char*)data.data(),
                              data.size());
        std::string result(dlen, 0);
        mbedtls_base64_encode((unsigned char*)result.data(), result.size(), &olen,
                              (const unsigned char*)data.data(), data.size());
        return result;
    }

public:
    ImageContent(const std::string& mime_type, const std::string& data) {
        mime_type_ = mime_type;
        // base64 encode data
        encoded_data_ = Base64Encode(data);
    }

    std::string to_json() const {
        CJsonUniquePtr json(cJSON_CreateObject());
        if (json == nullptr) {
            return {};
        }
        cJSON_AddStringToObject(json.get(), "type", "image");
        cJSON_AddStringToObject(json.get(), "mimeType", mime_type_.c_str());
        cJSON_AddStringToObject(json.get(), "data", encoded_data_.c_str());
        CJsonStringUniquePtr json_str(cJSON_PrintUnformatted(json.get()));
        return json_str != nullptr ? std::string(json_str.get()) : std::string();
    }
};

class PropertyList;

// Pointer alternatives transfer ownership to McpTool::Call.
using ReturnValue = std::variant<bool, int, std::string, cJSON*, ImageContent*>;
using ToolResult = std::expected<ReturnValue, std::string>;
using ToolCallback = std::function<ToolResult(const PropertyList&)>;

enum PropertyType { kPropertyTypeBoolean, kPropertyTypeInteger, kPropertyTypeString };

class Property {
private:
    std::string name_;
    PropertyType type_;
    std::variant<bool, int, std::string> value_;
    bool has_default_value_;
    std::optional<int> min_value_;  // 新增：整数最小值
    std::optional<int> max_value_;  // 新增：整数最大值

public:
    // Required field constructor
    Property(const std::string& name, PropertyType type)
        : name_(name), type_(type), has_default_value_(false) {}

    // Optional field constructor with default value
    template <typename T>
    Property(const std::string& name, PropertyType type, const T& default_value)
        : name_(name), type_(type), has_default_value_(true) {
        value_ = default_value;
    }

    Property(const std::string& name, PropertyType type, int min_value, int max_value)
        : name_(name),
          type_(type),
          has_default_value_(false),
          min_value_(min_value),
          max_value_(max_value) {
        if (type != kPropertyTypeInteger) {
            esp_system_abort("MCP property range is only valid for integer properties");
        }
    }

    Property(const std::string& name, PropertyType type, int default_value, int min_value,
             int max_value)
        : name_(name),
          type_(type),
          has_default_value_(true),
          min_value_(min_value),
          max_value_(max_value) {
        if (type != kPropertyTypeInteger) {
            esp_system_abort("MCP property range is only valid for integer properties");
        }
        if (default_value < min_value || default_value > max_value) {
            esp_system_abort("MCP property default value is outside its declared range");
        }
        value_ = default_value;
    }

    inline const std::string& name() const { return name_; }
    inline PropertyType type() const { return type_; }
    inline bool has_default_value() const { return has_default_value_; }
    inline bool has_range() const { return min_value_.has_value() && max_value_.has_value(); }
    inline int min_value() const { return min_value_.value_or(0); }
    inline int max_value() const { return max_value_.value_or(0); }

    template <typename T>
    inline const T& value() const {
        auto value = std::get_if<T>(&value_);
        if (value == nullptr) {
            esp_system_abort("MCP property value type does not match its declaration");
        }
        return *value;
    }

    template <typename T>
    inline std::expected<void, std::string> set_value(const T& value) {
        // 添加对设置的整数值进行范围检查
        if constexpr (std::is_same_v<T, int>) {
            if (min_value_.has_value() && value < min_value_.value()) {
                return std::unexpected("Value is below minimum allowed: " +
                                       std::to_string(min_value_.value()));
            }
            if (max_value_.has_value() && value > max_value_.value()) {
                return std::unexpected("Value exceeds maximum allowed: " +
                                       std::to_string(max_value_.value()));
            }
        }
        value_ = value;
        return {};
    }

    std::string to_json() const {
        CJsonUniquePtr json(cJSON_CreateObject());
        if (json == nullptr) {
            return {};
        }

        if (type_ == kPropertyTypeBoolean) {
            cJSON_AddStringToObject(json.get(), "type", "boolean");
            if (has_default_value_) {
                cJSON_AddBoolToObject(json.get(), "default", value<bool>());
            }
        } else if (type_ == kPropertyTypeInteger) {
            cJSON_AddStringToObject(json.get(), "type", "integer");
            if (has_default_value_) {
                cJSON_AddNumberToObject(json.get(), "default", value<int>());
            }
            if (min_value_.has_value()) {
                cJSON_AddNumberToObject(json.get(), "minimum", min_value_.value());
            }
            if (max_value_.has_value()) {
                cJSON_AddNumberToObject(json.get(), "maximum", max_value_.value());
            }
        } else if (type_ == kPropertyTypeString) {
            cJSON_AddStringToObject(json.get(), "type", "string");
            if (has_default_value_) {
                cJSON_AddStringToObject(json.get(), "default", value<std::string>().c_str());
            }
        }

        CJsonStringUniquePtr json_str(cJSON_PrintUnformatted(json.get()));
        return json_str != nullptr ? std::string(json_str.get()) : std::string();
    }
};

class PropertyList {
private:
    std::vector<Property> properties_;

public:
    PropertyList() = default;
    PropertyList(const std::vector<Property>& properties) : properties_(properties) {}
    void AddProperty(const Property& property) { properties_.push_back(property); }

    const Property& operator[](const std::string& name) const {
        for (const auto& property : properties_) {
            if (property.name() == name) {
                return property;
            }
        }
        esp_system_abort("MCP property lookup failed because the property was not declared");
    }

    auto begin() { return properties_.begin(); }
    auto end() { return properties_.end(); }

    std::vector<std::string> GetRequired() const {
        std::vector<std::string> required;
        for (auto& property : properties_) {
            if (!property.has_default_value()) {
                required.push_back(property.name());
            }
        }
        return required;
    }

    std::string to_json() const {
        CJsonUniquePtr json(cJSON_CreateObject());
        if (json == nullptr) {
            return {};
        }

        for (const auto& property : properties_) {
            CJsonUniquePtr prop_json(cJSON_Parse(property.to_json().c_str()));
            if (prop_json != nullptr &&
                cJSON_AddItemToObject(json.get(), property.name().c_str(), prop_json.get())) {
                prop_json.release();
            }
        }

        CJsonStringUniquePtr json_str(cJSON_PrintUnformatted(json.get()));
        return json_str != nullptr ? std::string(json_str.get()) : std::string();
    }
};

class McpTool {
private:
    std::string name_;
    std::string description_;
    PropertyList properties_;
    ToolCallback callback_;
    bool user_only_ = false;

public:
    McpTool(const std::string& name, const std::string& description, const PropertyList& properties,
            ToolCallback callback)
        : name_(name), description_(description), properties_(properties), callback_(callback) {}

    void set_user_only(bool user_only) { user_only_ = user_only; }
    inline const std::string& name() const { return name_; }
    inline const std::string& description() const { return description_; }
    inline const PropertyList& properties() const { return properties_; }
    inline bool user_only() const { return user_only_; }

    std::string to_json() const {
        std::vector<std::string> required = properties_.GetRequired();

        CJsonUniquePtr json(cJSON_CreateObject());
        if (json == nullptr) {
            return {};
        }
        cJSON_AddStringToObject(json.get(), "name", name_.c_str());
        cJSON_AddStringToObject(json.get(), "description", description_.c_str());

        CJsonUniquePtr input_schema(cJSON_CreateObject());
        if (input_schema == nullptr) {
            return {};
        }
        cJSON_AddStringToObject(input_schema.get(), "type", "object");

        CJsonUniquePtr properties(cJSON_Parse(properties_.to_json().c_str()));
        if (properties == nullptr ||
            !cJSON_AddItemToObject(input_schema.get(), "properties", properties.get())) {
            return {};
        }
        properties.release();

        if (!required.empty()) {
            CJsonUniquePtr required_array(cJSON_CreateArray());
            if (required_array == nullptr) {
                return {};
            }
            for (const auto& property : required) {
                CJsonUniquePtr item(cJSON_CreateString(property.c_str()));
                if (item == nullptr || !cJSON_AddItemToArray(required_array.get(), item.get())) {
                    return {};
                }
                item.release();
            }
            if (!cJSON_AddItemToObject(input_schema.get(), "required", required_array.get())) {
                return {};
            }
            required_array.release();
        }

        if (!cJSON_AddItemToObject(json.get(), "inputSchema", input_schema.get())) {
            return {};
        }
        input_schema.release();

        // Add audience annotation if the tool is user only (invisible to AI)
        if (user_only_) {
            CJsonUniquePtr annotations(cJSON_CreateObject());
            CJsonUniquePtr audience(cJSON_CreateArray());
            CJsonUniquePtr user(cJSON_CreateString("user"));
            if (annotations == nullptr || audience == nullptr || user == nullptr ||
                !cJSON_AddItemToArray(audience.get(), user.get())) {
                return {};
            }
            user.release();
            if (!cJSON_AddItemToObject(annotations.get(), "audience", audience.get())) {
                return {};
            }
            audience.release();
            if (!cJSON_AddItemToObject(json.get(), "annotations", annotations.get())) {
                return {};
            }
            annotations.release();
        }

        CJsonStringUniquePtr json_str(cJSON_PrintUnformatted(json.get()));
        return json_str != nullptr ? std::string(json_str.get()) : std::string();
    }

    std::expected<std::string, std::string> Call(const PropertyList& properties) {
        auto callback_result = callback_(properties);
        if (!callback_result) {
            return std::unexpected(std::move(callback_result.error()));
        }
        ReturnValue return_value = std::move(*callback_result);
        std::unique_ptr<ImageContent> owned_image;
        CJsonUniquePtr owned_json;
        if (std::holds_alternative<ImageContent*>(return_value)) {
            owned_image.reset(std::get<ImageContent*>(return_value));
        } else if (std::holds_alternative<cJSON*>(return_value)) {
            owned_json.reset(std::get<cJSON*>(return_value));
        }

        // 返回结果
        CJsonUniquePtr result(cJSON_CreateObject());
        CJsonUniquePtr content(cJSON_CreateArray());
        if (result == nullptr || content == nullptr) {
            return std::unexpected("Failed to allocate MCP result");
        }

        if (std::holds_alternative<ImageContent*>(return_value)) {
            if (owned_image == nullptr) {
                return std::unexpected("MCP tool returned an empty image");
            }
            CJsonUniquePtr image(cJSON_CreateObject());
            if (image == nullptr) {
                return std::unexpected("Failed to allocate MCP image result");
            }
            cJSON_AddStringToObject(image.get(), "type", "image");
            cJSON_AddStringToObject(image.get(), "image", owned_image->to_json().c_str());
            if (!cJSON_AddItemToArray(content.get(), image.get())) {
                return std::unexpected("Failed to create MCP image result");
            }
            image.release();
        } else {
            CJsonUniquePtr text(cJSON_CreateObject());
            if (text == nullptr) {
                return std::unexpected("Failed to allocate MCP text result");
            }
            cJSON_AddStringToObject(text.get(), "type", "text");
            if (std::holds_alternative<std::string>(return_value)) {
                cJSON_AddStringToObject(text.get(), "text",
                                        std::get<std::string>(return_value).c_str());
            } else if (std::holds_alternative<bool>(return_value)) {
                cJSON_AddStringToObject(text.get(), "text",
                                        std::get<bool>(return_value) ? "true" : "false");
            } else if (std::holds_alternative<int>(return_value)) {
                cJSON_AddStringToObject(text.get(), "text",
                                        std::to_string(std::get<int>(return_value)).c_str());
            } else if (std::holds_alternative<cJSON*>(return_value)) {
                CJsonStringUniquePtr json_str(cJSON_PrintUnformatted(owned_json.get()));
                cJSON_AddStringToObject(text.get(), "text",
                                        json_str != nullptr ? json_str.get() : "{}");
            }
            if (!cJSON_AddItemToArray(content.get(), text.get())) {
                return std::unexpected("Failed to create MCP text result");
            }
            text.release();
        }
        if (!cJSON_AddItemToObject(result.get(), "content", content.get())) {
            return std::unexpected("Failed to create MCP result");
        }
        content.release();
        cJSON_AddBoolToObject(result.get(), "isError", false);

        CJsonStringUniquePtr json_str(cJSON_PrintUnformatted(result.get()));
        if (json_str == nullptr) {
            return std::unexpected("Failed to serialize MCP result");
        }
        return std::string(json_str.get());
    }
};

class McpServer {
public:
    static McpServer& GetInstance() {
        static McpServer instance;
        return instance;
    }

    void AddCommonTools();
    void AddUserOnlyTools();
    void AddTool(std::unique_ptr<McpTool> tool);
    void AddTool(const std::string& name, const std::string& description,
                 const PropertyList& properties, ToolCallback callback);
    void AddUserOnlyTool(const std::string& name, const std::string& description,
                         const PropertyList& properties, ToolCallback callback);
    void ParseMessage(const cJSON* json);
    void ParseMessage(const std::string& message);

private:
    McpServer();
    ~McpServer();

    void ParseCapabilities(const cJSON* capabilities);

    void ReplyResult(int id, const std::string& result);
    void ReplyError(int id, const std::string& message);

    void GetToolsList(int id, const std::string& cursor, bool list_user_only_tools);
    void DoToolCall(int id, const std::string& tool_name, const cJSON* tool_arguments);

    std::vector<std::unique_ptr<McpTool>> tools_;
};

#endif  // MCP_SERVER_H
