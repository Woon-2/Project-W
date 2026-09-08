#include "simpleJson.hpp"

#include <charconv>

namespace json {
namespace {

const std::string kEmptyString{};
const Value::Array kEmptyArray{};
const Value::Object kEmptyObject{};

class Parser {
public:
    explicit Parser(std::string_view text) : text_(text) {}

    bool parse(Value& out, std::string* error) {
        skipWhitespace();
        if (!parseValue(out))
            return fail(error, "invalid JSON value");

        skipWhitespace();
        if (pos_ != text_.size())
            return fail(error, "trailing characters after JSON root");

        return true;
    }

private:
    bool parseValue(Value& out) {
        skipWhitespace();
        if (pos_ >= text_.size())
            return false;

        const char c = text_[pos_];
        if (c == '{') return parseObject(out);
        if (c == '[') return parseArray(out);
        if (c == '"') {
            std::string s;
            if (!parseString(s))
                return false;
            out = Value(std::move(s));
            return true;
        }
        if (c == '-' || (c >= '0' && c <= '9'))
            return parseNumber(out);
        if (consumeLiteral("true")) {
            out = Value(true);
            return true;
        }
        if (consumeLiteral("false")) {
            out = Value(false);
            return true;
        }
        if (consumeLiteral("null")) {
            out = Value(nullptr);
            return true;
        }
        return false;
    }

    bool parseObject(Value& out) {
        if (!consume('{'))
            return false;

        Value::Object object;
        skipWhitespace();
        if (consume('}')) {
            out = Value(std::move(object));
            return true;
        }

        while (pos_ < text_.size()) {
            std::string key;
            if (!parseString(key))
                return false;

            skipWhitespace();
            if (!consume(':'))
                return false;

            Value value;
            if (!parseValue(value))
                return false;

            object.emplace_back(std::move(key), std::move(value));

            skipWhitespace();
            if (consume('}')) {
                out = Value(std::move(object));
                return true;
            }
            if (!consume(','))
                return false;
            skipWhitespace();
        }

        return false;
    }

    bool parseArray(Value& out) {
        if (!consume('['))
            return false;

        Value::Array array;
        skipWhitespace();
        if (consume(']')) {
            out = Value(std::move(array));
            return true;
        }

        while (pos_ < text_.size()) {
            Value value;
            if (!parseValue(value))
                return false;
            array.push_back(std::move(value));

            skipWhitespace();
            if (consume(']')) {
                out = Value(std::move(array));
                return true;
            }
            if (!consume(','))
                return false;
            skipWhitespace();
        }

        return false;
    }

    bool parseString(std::string& out) {
        if (!consume('"'))
            return false;

        out.clear();
        while (pos_ < text_.size()) {
            const char c = text_[pos_++];
            if (c == '"')
                return true;

            if (c != '\\') {
                out.push_back(c);
                continue;
            }

            if (pos_ >= text_.size())
                return false;

            const char escaped = text_[pos_++];
            switch (escaped) {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u':
                if (pos_ + 4 > text_.size())
                    return false;
                out.push_back('?');
                pos_ += 4;
                break;
            default:
                return false;
            }
        }

        return false;
    }

    bool parseNumber(Value& out) {
        const std::size_t begin = pos_;

        if (pos_ < text_.size() && text_[pos_] == '-')
            ++pos_;
        while (pos_ < text_.size() && isDigit(text_[pos_]))
            ++pos_;
        if (pos_ < text_.size() && text_[pos_] == '.') {
            ++pos_;
            while (pos_ < text_.size() && isDigit(text_[pos_]))
                ++pos_;
        }
        if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-'))
                ++pos_;
            while (pos_ < text_.size() && isDigit(text_[pos_]))
                ++pos_;
        }

        double value = 0.0;
        const char* first = text_.data() + begin;
        const char* last = text_.data() + pos_;
        const auto result = std::from_chars(first, last, value);
        if (result.ec != std::errc{})
            return false;

        out = Value(value);
        return true;
    }

    bool consume(char expected) {
        if (pos_ >= text_.size() || text_[pos_] != expected)
            return false;
        ++pos_;
        return true;
    }

    bool consumeLiteral(std::string_view literal) {
        if (text_.substr(pos_, literal.size()) != literal)
            return false;
        pos_ += literal.size();
        return true;
    }

    static bool isDigit(char c) { return c >= '0' && c <= '9'; }

    static bool isSpace(char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
    }

    void skipWhitespace() {
        while (pos_ < text_.size() && isSpace(text_[pos_]))
            ++pos_;
    }

    bool fail(std::string* error, const char* message) const {
        if (error)
            *error = std::string(message) + " at byte " + std::to_string(pos_);
        return false;
    }

    std::string_view text_;
    std::size_t pos_ = 0;
};

}  // namespace

bool Value::asBool(bool fallback) const {
    if (const auto* value = std::get_if<bool>(&data_))
        return *value;
    return fallback;
}

double Value::asNumber(double fallback) const {
    if (const auto* value = std::get_if<double>(&data_))
        return *value;
    return fallback;
}

const std::string& Value::asString() const {
    if (const auto* value = std::get_if<std::string>(&data_))
        return *value;
    return kEmptyString;
}

const Value::Array& Value::asArray() const {
    if (const auto* value = std::get_if<Array>(&data_))
        return *value;
    return kEmptyArray;
}

const Value::Object& Value::asObject() const {
    if (const auto* value = std::get_if<Object>(&data_))
        return *value;
    return kEmptyObject;
}

const Value* Value::find(std::string_view key) const {
    const auto* object = std::get_if<Object>(&data_);
    if (!object)
        return nullptr;

    for (const auto& [k, v] : *object)
        if (k == key)
            return &v;
    return nullptr;
}

const Value* Value::findPath(std::string_view dottedPath) const {
    const Value* current = this;
    std::size_t begin = 0;

    while (begin <= dottedPath.size()) {
        const std::size_t dot = dottedPath.find('.', begin);
        const std::string_view part = dottedPath.substr(
            begin,
            dot == std::string_view::npos ? std::string_view::npos : dot - begin
        );
        if (part.empty())
            return nullptr;
        current = current->find(part);
        if (!current)
            return nullptr;
        if (dot == std::string_view::npos)
            return current;
        begin = dot + 1;
    }

    return nullptr;
}

bool parse(std::string_view text, Value& out, std::string* error) {
    return Parser(text).parse(out, error);
}

}  // namespace json
