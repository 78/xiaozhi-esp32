#ifndef MCP_SERVER_H
#define MCP_SERVER_H

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <variant>
#include <optional>
#include <stdexcept>

#include <cJSON.h>

// 添加类型别名
using ReturnValue = std::variant<bool, int, std::string>;

enum PropertyType {
    kPropertyTypeBoolean,
    kPropertyTypeInteger,
    kPropertyTypeString
};

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
    template<typename T>
    Property(const std::string& name, PropertyType type, const T& default_value)
        : name_(name), type_(type), has_default_value_(true) {
        value_ = default_value;
    }

    Property(const std::string& name, PropertyType type, int min_value, int max_value)
        : name_(name), type_(type), has_default_value_(false), min_value_(min_value), max_value_(max_value) {
        if (type != kPropertyTypeInteger) {
            throw std::invalid_argument("Range limits only apply to integer properties");
        }
    }

    Property(const std::string& name, PropertyType type, int default_value, int min_value, int max_value)
        : name_(name), type_(type), has_default_value_(true), min_value_(min_value), max_value_(max_value) {
        if (type != kPropertyTypeInteger) {
            throw std::invalid_argument("Range limits only apply to integer properties");
        }
        if (default_value < min_value || default_value > max_value) {
            throw std::invalid_argument("Default value must be within the specified range");
        }
        value_ = default_value;
    }

    inline const std::string& name() const { return name_; }
    inline PropertyType type() const { return type_; }
    inline bool has_default_value() const { return has_default_value_; }
    inline bool has_range() const { return min_value_.has_value() && max_value_.has_value(); }
    inline int min_value() const { return min_value_.value_or(0); }
    inline int max_value() const { return max_value_.value_or(0); }

    template<typename T>
    inline T value() const {
        return std::get<T>(value_);
    }

    template<typename T>
    inline void set_value(const T& value) {
        // 添加对设置的整数值进行范围检查
        if constexpr (std::is_same_v<T, int>) {
            if (min_value_.has_value() && value < min_value_.value()) {
                throw std::invalid_argument("Value is below minimum allowed: " + std::to_string(min_value_.value()));
            }
            if (max_value_.has_value() && value > max_value_.value()) {
                throw std::invalid_argument("Value exceeds maximum allowed: " + std::to_string(max_value_.value()));
            }
        }
        value_ = value;
    }

    std::string to_json() const {
        cJSON *json = cJSON_CreateObject();
        
        if (type_ == kPropertyTypeBoolean) {
            cJSON_AddStringToObject(json, "type", "boolean");
            if (has_default_value_) {
                cJSON_AddBoolToObject(json, "default", value<bool>());
            }
        } else if (type_ == kPropertyTypeInteger) {
            cJSON_AddStringToObject(json, "type", "integer");
            if (has_default_value_) {
                cJSON_AddNumberToObject(json, "default", value<int>());
            }
            if (min_value_.has_value()) {
                cJSON_AddNumberToObject(json, "minimum", min_value_.value());
            }
            if (max_value_.has_value()) {
                cJSON_AddNumberToObject(json, "maximum", max_value_.value());
            }
        } else if (type_ == kPropertyTypeString) {
            cJSON_AddStringToObject(json, "type", "string");
            if (has_default_value_) {
                cJSON_AddStringToObject(json, "default", value<std::string>().c_str());
            }
        }
        
        char *json_str = cJSON_PrintUnformatted(json);
        std::string result(json_str);
        cJSON_free(json_str);
        cJSON_Delete(json);
        
        return result;
    }
};

class PropertyList {
private:
    std::vector<Property> properties_;

public:
    PropertyList() = default;
    PropertyList(const std::vector<Property>& properties) : properties_(properties) {}
    void AddProperty(const Property& property) {
        properties_.push_back(property);
    }

    const Property& operator[](const std::string& name) const {
        for (const auto& property : properties_) {
            if (property.name() == name) {
                return property;
            }
        }
        throw std::runtime_error("Property not found: " + name);
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
        cJSON *json = cJSON_CreateObject();
        
        for (const auto& property : properties_) {
            cJSON *prop_json = cJSON_Parse(property.to_json().c_str());
            cJSON_AddItemToObject(json, property.name().c_str(), prop_json);
        }
        
        char *json_str = cJSON_PrintUnformatted(json);
        std::string result(json_str);
        cJSON_free(json_str);
        cJSON_Delete(json);
        
        return result;
    }
};

class McpTool {
private:
    std::string name_;
    std::string description_;
    PropertyList properties_;
    std::function<ReturnValue(const PropertyList&)> callback_;

public:
    McpTool(const std::string& name, 
            const std::string& description, 
            const PropertyList& properties, 
            std::function<ReturnValue(const PropertyList&)> callback)
        : name_(name), 
        description_(description), 
        properties_(properties), 
        callback_(callback) {}

    inline const std::string& name() const { return name_; }
    inline const std::string& description() const { return description_; }
    inline const PropertyList& properties() const { return properties_; }

    std::string to_json() const {
        std::vector<std::string> required = properties_.GetRequired();
        
        cJSON *json = cJSON_CreateObject();
        cJSON_AddStringToObject(json, "name", name_.c_str());
        cJSON_AddStringToObject(json, "description", description_.c_str());
        
        cJSON *input_schema = cJSON_CreateObject();
        cJSON_AddStringToObject(input_schema, "type", "object");
        
        cJSON *properties = cJSON_Parse(properties_.to_json().c_str());
        cJSON_AddItemToObject(input_schema, "properties", properties);
        
        if (!required.empty()) {
            cJSON *required_array = cJSON_CreateArray();
            for (const auto& property : required) {
                cJSON_AddItemToArray(required_array, cJSON_CreateString(property.c_str()));
            }
            cJSON_AddItemToObject(input_schema, "required", required_array);
        }
        
        cJSON_AddItemToObject(json, "inputSchema", input_schema);
        
        char *json_str = cJSON_PrintUnformatted(json);
        std::string result(json_str);
        cJSON_free(json_str);
        cJSON_Delete(json);
        
        return result;
    }

    std::string Call(const PropertyList& properties) {
        ReturnValue return_value = callback_(properties);
        // 返回结果
        cJSON* result = cJSON_CreateObject();
        cJSON* content = cJSON_CreateArray();
        cJSON* text = cJSON_CreateObject();
        cJSON_AddStringToObject(text, "type", "text");
        if (std::holds_alternative<std::string>(return_value)) {
            cJSON_AddStringToObject(text, "text", std::get<std::string>(return_value).c_str());
        } else if (std::holds_alternative<bool>(return_value)) {
            cJSON_AddStringToObject(text, "text", std::get<bool>(return_value) ? "true" : "false");
        } else if (std::holds_alternative<int>(return_value)) {
            cJSON_AddStringToObject(text, "text", std::to_string(std::get<int>(return_value)).c_str());
        }
        cJSON_AddItemToArray(content, text);
        cJSON_AddItemToObject(result, "content", content);
        cJSON_AddBoolToObject(result, "isError", false);

        auto json_str = cJSON_PrintUnformatted(result);
        std::string result_str(json_str);
        cJSON_free(json_str);
        cJSON_Delete(result);
        return result_str;
    }
};

class McpServer {
public:
    static McpServer& GetInstance() {
        static McpServer instance;
        return instance;
    }

    void AddTool(McpTool* tool);
    void AddTool(const std::string& name, const std::string& description, const PropertyList& properties, std::function<ReturnValue(const PropertyList&)> callback);
    void ParseMessage(const cJSON* json);
    void ParseMessage(const std::string& message);

private:
    McpServer();
    ~McpServer();

    void AddCommonTools();
    void ParseCapabilities(const cJSON* capabilities);

    void ReplyResult(int id, const std::string& result);
    void ReplyError(int id, const std::string& message);

    void GetToolsList(int id, const std::string& cursor);
    void DoToolCall(int id, const std::string& tool_name, const cJSON* tool_arguments);

    std::vector<McpTool*> tools_;
};

#endif // MCP_SERVER_H
