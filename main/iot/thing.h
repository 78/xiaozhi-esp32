#ifndef THING_H
#define THING_H

#include <string>
#include <map>
#include <functional>
#include <vector>
#include <stdexcept>
#include <cJSON.h>

namespace iot {

enum ValueType {
    kValueTypeBoolean,
    kValueTypeNumber,
    kValueTypeString
};

class Property {
private:
    std::string name_;
    std::string description_;
    ValueType type_;
    std::function<bool()> boolean_getter_;
    std::function<int()> number_getter_;
    std::function<std::string()> string_getter_;

public:
    Property(const std::string& name, const std::string& description, std::function<bool()> getter) :
        name_(name), description_(description), type_(kValueTypeBoolean), boolean_getter_(getter) {}
    Property(const std::string& name, const std::string& description, std::function<int()> getter) :
        name_(name), description_(description), type_(kValueTypeNumber), number_getter_(getter) {}
    Property(const std::string& name, const std::string& description, std::function<std::string()> getter) :
        name_(name), description_(description), type_(kValueTypeString), string_getter_(getter) {}

    const std::string& name() const { return name_; }
    const std::string& description() const { return description_; }
    ValueType type() const { return type_; }

    bool boolean() const { return boolean_getter_(); }
    int number() const { return number_getter_(); }
    std::string string() const { return string_getter_(); }

    std::string GetDescriptorJson() {
        std::string json_str = "{";
        json_str += "\"description\":\"" + description_ + "\",";
        if (type_ == kValueTypeBoolean) {
            json_str += "\"type\":\"boolean\"";
        } else if (type_ == kValueTypeNumber) {
            json_str += "\"type\":\"number\"";
        } else if (type_ == kValueTypeString) {
            json_str += "\"type\":\"string\"";
        }
        json_str += "}";
        return json_str;
    }

    std::string GetStateJson() {
        if (type_ == kValueTypeBoolean) {
            return boolean_getter_() ? "true" : "false";
        } else if (type_ == kValueTypeNumber) {
            return std::to_string(number_getter_());
        } else if (type_ == kValueTypeString) {
            return "\"" + string_getter_() + "\"";
        }
        return "null";
    }
};

class PropertyList {
private:
    std::vector<Property> properties_;

public:
    PropertyList() = default;
    PropertyList(const std::vector<Property>& properties) : properties_(properties) {}

    void AddBooleanProperty(const std::string& name, const std::string& description, std::function<bool()> getter) {
        properties_.push_back(Property(name, description, getter));
    }
    void AddNumberProperty(const std::string& name, const std::string& description, std::function<int()> getter) {
        properties_.push_back(Property(name, description, getter));
    }
    void AddStringProperty(const std::string& name, const std::string& description, std::function<std::string()> getter) {
        properties_.push_back(Property(name, description, getter));
    }

    const Property& operator[](const std::string& name) const {
        for (auto& property : properties_) {
            if (property.name() == name) {
                return property;
            }
        }
        throw std::runtime_error("Property not found: " + name);
    }

    std::string GetDescriptorJson() {
        std::string json_str = "{";
        for (auto& property : properties_) {
            json_str += "\"" + property.name() + "\":" + property.GetDescriptorJson() + ",";
        }
        if (json_str.back() == ',') {
            json_str.pop_back();
        }
        json_str += "}";
        return json_str;
    }

    std::string GetStateJson() {
        std::string json_str = "{";
        for (auto& property : properties_) {
            json_str += "\"" + property.name() + "\":" + property.GetStateJson() + ",";
        }
        if (json_str.back() == ',') {
            json_str.pop_back();
        }
        json_str += "}";
        return json_str;
    }
};

class Parameter {
private:
    std::string name_;
    std::string description_;
    ValueType type_;
    bool required_;
    bool boolean_;
    int number_;
    std::string string_;

public:
    Parameter(const std::string& name, const std::string& description, ValueType type, bool required = true) :
        name_(name), description_(description), type_(type), required_(required) {}

    const std::string& name() const { return name_; }
    const std::string& description() const { return description_; }
    ValueType type() const { return type_; }
    bool required() const { return required_; }

    bool boolean() const { return boolean_; }
    int number() const { return number_; }
    const std::string& string() const { return string_; }

    void set_boolean(bool value) { boolean_ = value; }
    void set_number(int value) { number_ = value; }
    void set_string(const std::string& value) { string_ = value; }

    std::string GetDescriptorJson() {
        std::string json_str = "{";
        json_str += "\"description\":\"" + description_ + "\",";
        if (type_ == kValueTypeBoolean) {
            json_str += "\"type\":\"boolean\"";
        } else if (type_ == kValueTypeNumber) {
            json_str += "\"type\":\"number\"";
        } else if (type_ == kValueTypeString) {
            json_str += "\"type\":\"string\"";
        }
        json_str += "}";
        return json_str;
    }
};

class ParameterList {
private:
    std::vector<Parameter> parameters_;

public:
    ParameterList() = default;
    ParameterList(const std::vector<Parameter>& parameters) : parameters_(parameters) {}
    void AddParameter(const Parameter& parameter) {
        parameters_.push_back(parameter);
    }

    const Parameter& operator[](const std::string& name) const {
        for (auto& parameter : parameters_) {
            if (parameter.name() == name) {
                return parameter;
            }
        }
        throw std::runtime_error("Parameter not found: " + name);
    }

    // iterator
    auto begin() { return parameters_.begin(); }
    auto end() { return parameters_.end(); }

    std::string GetDescriptorJson() {
        std::string json_str = "{";
        for (auto& parameter : parameters_) {
            json_str += "\"" + parameter.name() + "\":" + parameter.GetDescriptorJson() + ",";
        }
        if (json_str.back() == ',') {
            json_str.pop_back();
        }
        json_str += "}";
        return json_str;
    }
};

class Method {
private:
    std::string name_;
    std::string description_;
    ParameterList parameters_;
    std::function<void(const ParameterList&)> callback_;

public:
    Method(const std::string& name, const std::string& description, const ParameterList& parameters, std::function<void(const ParameterList&)> callback) :
        name_(name), description_(description), parameters_(parameters), callback_(callback) {}

    const std::string& name() const { return name_; }
    const std::string& description() const { return description_; }
    ParameterList& parameters() { return parameters_; }

    std::string GetDescriptorJson() {
        std::string json_str = "{";
        json_str += "\"description\":\"" + description_ + "\",";
        json_str += "\"parameters\":" + parameters_.GetDescriptorJson();
        json_str += "}";
        return json_str;
    }

    void Invoke() {
        callback_(parameters_);
    }
};

class MethodList {
private:
    std::vector<Method> methods_;

public:
    MethodList() = default;
    MethodList(const std::vector<Method>& methods) : methods_(methods) {}

    void AddMethod(const std::string& name, const std::string& description, const ParameterList& parameters, std::function<void(const ParameterList&)> callback) {
        methods_.push_back(Method(name, description, parameters, callback));
    }

    Method& operator[](const std::string& name) {
        for (auto& method : methods_) {
            if (method.name() == name) {
                return method;
            }
        }
        throw std::runtime_error("Method not found: " + name);
    }

    std::string GetDescriptorJson() {
        std::string json_str = "{";
        for (auto& method : methods_) {
            json_str += "\"" + method.name() + "\":" + method.GetDescriptorJson() + ",";
        }
        if (json_str.back() == ',') {
            json_str.pop_back();
        }
        json_str += "}";
        return json_str;
    }
};

class Thing {
public:
    Thing(const std::string& name, const std::string& description) :
        name_(name), description_(description) {}
    virtual ~Thing() = default;

    virtual std::string GetDescriptorJson();
    virtual std::string GetStateJson();
    virtual void Invoke(const cJSON* command);

    const std::string& name() const { return name_; }
    const std::string& description() const { return description_; }

protected:
    PropertyList properties_;
    MethodList methods_;

private:
    std::string name_;
    std::string description_;
};


void RegisterThing(const std::string& type, std::function<Thing*()> creator);
Thing* CreateThing(const std::string& type);

#define DECLARE_THING(TypeName) \
    static iot::Thing* Create##TypeName() { \
        return new iot::TypeName(); \
    } \
    static bool Register##TypeNameHelper = []() { \
        RegisterThing(#TypeName, Create##TypeName); \
        return true; \
    }();

} // namespace iot

#endif // THING_H
