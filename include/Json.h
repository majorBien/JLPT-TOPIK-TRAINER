// Minimal, dependency-free JSON value + parser + serializer.
// Supports everything the app needs: objects (insertion ordered), arrays,
// strings (UTF-8, \uXXXX escapes incl. surrogate pairs), numbers, bools, null.
#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace json {

class ParseError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class Value;
using Array = std::vector<Value>;
using Object = std::vector<std::pair<std::string, Value>>;

class Value {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Value() = default;
    Value(std::nullptr_t) {}
    Value(bool b) : data_(b) {}
    Value(double d) : data_(d) {}
    template <typename T,
              std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, int> = 0>
    Value(T n) : data_(static_cast<double>(n)) {}
    Value(const char* s) : data_(std::string(s)) {}
    Value(std::string s) : data_(std::move(s)) {}
    Value(Array a) : data_(std::move(a)) {}
    Value(Object o) : data_(std::move(o)) {}

    static Value array() { return Value(Array{}); }
    static Value object() { return Value(Object{}); }

    Type type() const { return static_cast<Type>(data_.index()); }
    bool isNull() const { return type() == Type::Null; }
    bool isBool() const { return type() == Type::Bool; }
    bool isNumber() const { return type() == Type::Number; }
    bool isString() const { return type() == Type::String; }
    bool isArray() const { return type() == Type::Array; }
    bool isObject() const { return type() == Type::Object; }

    // Lenient accessors: return a fallback instead of throwing on type mismatch.
    bool asBool(bool fallback = false) const;
    double asNumber(double fallback = 0.0) const;
    std::int64_t asInt(std::int64_t fallback = 0) const;
    const std::string& asString() const;
    const Array& asArray() const;
    const Object& asObject() const;

    // Object helpers.
    const Value* find(std::string_view key) const;
    const Value& operator[](std::string_view key) const;  // null value if missing
    Value& set(const std::string& key, Value v);           // insert or replace
    std::string getString(std::string_view key, const std::string& fallback = {}) const;
    std::int64_t getInt(std::string_view key, std::int64_t fallback = 0) const;
    double getNumber(std::string_view key, double fallback = 0.0) const;
    bool getBool(std::string_view key, bool fallback = false) const;
    std::vector<std::string> getStringArray(std::string_view key) const;

    // Array helpers.
    void push_back(Value v);
    std::size_t size() const;

    std::string dump(int indent = -1) const;
    static Value parse(std::string_view text);

private:
    void dumpTo(std::string& out, int indent, int depth) const;

    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> data_{nullptr};
};

// File helpers.
std::string readFile(const std::string& path);                       // throws std::runtime_error
Value parseFile(const std::string& path);                             // throws
bool writeFileAtomic(const std::string& path, const std::string& content);

}  // namespace json
