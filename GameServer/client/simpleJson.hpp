#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace json {

class Value {
public:
    using Array = std::vector<Value>;
    using Object = std::map<std::string, Value>;

    Value() = default;
    Value(std::nullptr_t) : data_(nullptr) {}
    Value(bool value) : data_(value) {}
    Value(double value) : data_(value) {}
    Value(std::string value) : data_(std::move(value)) {}
    Value(Array value) : data_(std::move(value)) {}
    Value(Object value) : data_(std::move(value)) {}

    bool isNull() const { return std::holds_alternative<std::nullptr_t>(data_); }
    bool isBool() const { return std::holds_alternative<bool>(data_); }
    bool isNumber() const { return std::holds_alternative<double>(data_); }
    bool isString() const { return std::holds_alternative<std::string>(data_); }
    bool isArray() const { return std::holds_alternative<Array>(data_); }
    bool isObject() const { return std::holds_alternative<Object>(data_); }

    bool asBool(bool fallback = false) const;
    double asNumber(double fallback = 0.0) const;
    const std::string& asString() const;
    const Array& asArray() const;
    const Object& asObject() const;

    const Value* find(std::string_view key) const;
    const Value* findPath(std::string_view dottedPath) const;

private:
    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> data_{ nullptr };
};

bool parse(std::string_view text, Value& out, std::string* error = nullptr);

}  // namespace json
