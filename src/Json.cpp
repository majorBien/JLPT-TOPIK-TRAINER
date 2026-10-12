#include "Json.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace json {

namespace {

const std::string kEmptyString;
const Array kEmptyArray;
const Object kEmptyObject;
const Value kNullValue;

void appendUtf8(std::string& out, std::uint32_t cp) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

void escapeString(std::string& out, const std::string& s) {
    out += '"';
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    out += '"';
}

class Parser {
public:
    explicit Parser(std::string_view text) : s_(text) {}

    Value parseDocument() {
        skipWhitespace();
        Value v = parseValue(0);
        skipWhitespace();
        if (pos_ != s_.size()) fail("unexpected trailing characters");
        return v;
    }

private:
    static constexpr int kMaxDepth = 256;

    [[noreturn]] void fail(const std::string& msg) const {
        throw ParseError("JSON parse error at offset " + std::to_string(pos_) + ": " + msg);
    }

    void skipWhitespace() {
        while (pos_ < s_.size() &&
               (s_[pos_] == ' ' || s_[pos_] == '\n' || s_[pos_] == '\r' || s_[pos_] == '\t'))
            ++pos_;
    }

    char peek() const { return pos_ < s_.size() ? s_[pos_] : '\0'; }

    void expect(char c) {
        if (peek() != c) fail(std::string("expected '") + c + "'");
        ++pos_;
    }

    bool consumeLiteral(std::string_view lit) {
        if (s_.substr(pos_, lit.size()) == lit) {
            pos_ += lit.size();
            return true;
        }
        return false;
    }

    Value parseValue(int depth) {
        if (depth > kMaxDepth) fail("nesting too deep");
        switch (peek()) {
            case '{': return parseObject(depth);
            case '[': return parseArray(depth);
            case '"': return Value(parseString());
            case 't': if (consumeLiteral("true")) return Value(true); break;
            case 'f': if (consumeLiteral("false")) return Value(false); break;
            case 'n': if (consumeLiteral("null")) return Value(); break;
            default:
                if (peek() == '-' || (peek() >= '0' && peek() <= '9')) return parseNumber();
        }
        fail("unexpected character");
    }

    Value parseObject(int depth) {
        expect('{');
        Object obj;
        skipWhitespace();
        if (peek() == '}') { ++pos_; return Value(std::move(obj)); }
        for (;;) {
            skipWhitespace();
            if (peek() != '"') fail("expected object key");
            std::string key = parseString();
            skipWhitespace();
            expect(':');
            skipWhitespace();
            obj.emplace_back(std::move(key), parseValue(depth + 1));
            skipWhitespace();
            if (peek() == ',') { ++pos_; continue; }
            expect('}');
            return Value(std::move(obj));
        }
    }

    Value parseArray(int depth) {
        expect('[');
        Array arr;
        skipWhitespace();
        if (peek() == ']') { ++pos_; return Value(std::move(arr)); }
        for (;;) {
            skipWhitespace();
            arr.push_back(parseValue(depth + 1));
            skipWhitespace();
            if (peek() == ',') { ++pos_; continue; }
            expect(']');
            return Value(std::move(arr));
        }
    }

    std::uint32_t parseHex4() {
        if (pos_ + 4 > s_.size()) fail("truncated \\u escape");
        std::uint32_t v = 0;
        for (int i = 0; i < 4; ++i) {
            char c = s_[pos_++];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= static_cast<std::uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') v |= static_cast<std::uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= static_cast<std::uint32_t>(c - 'A' + 10);
            else fail("invalid hex digit");
        }
        return v;
    }

    std::string parseString() {
        expect('"');
        std::string out;
        for (;;) {
            if (pos_ >= s_.size()) fail("unterminated string");
            char c = s_[pos_++];
            if (c == '"') return out;
            if (c != '\\') { out += c; continue; }
            if (pos_ >= s_.size()) fail("unterminated escape");
            char e = s_[pos_++];
            switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    std::uint32_t cp = parseHex4();
                    if (cp >= 0xD800 && cp <= 0xDBFF && consumeLiteral("\\u")) {
                        std::uint32_t low = parseHex4();
                        if (low >= 0xDC00 && low <= 0xDFFF)
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                    }
                    appendUtf8(out, cp);
                    break;
                }
                default: fail("invalid escape");
            }
        }
    }

    Value parseNumber() {
        std::size_t start = pos_;
        if (peek() == '-') ++pos_;
        while (pos_ < s_.size() && std::string_view("0123456789.eE+-").find(s_[pos_]) != std::string_view::npos)
            ++pos_;
        std::string token(s_.substr(start, pos_ - start));
        try {
            std::size_t used = 0;
            double d = std::stod(token, &used);
            if (used != token.size()) fail("invalid number");
            return Value(d);
        } catch (const std::logic_error&) {
            fail("invalid number");
        }
    }

    std::string_view s_;
    std::size_t pos_ = 0;
};

}  // namespace

bool Value::asBool(bool fallback) const {
    return isBool() ? std::get<bool>(data_) : fallback;
}

double Value::asNumber(double fallback) const {
    return isNumber() ? std::get<double>(data_) : fallback;
}

std::int64_t Value::asInt(std::int64_t fallback) const {
    return isNumber() ? static_cast<std::int64_t>(std::llround(std::get<double>(data_))) : fallback;
}

const std::string& Value::asString() const {
    return isString() ? std::get<std::string>(data_) : kEmptyString;
}

const Array& Value::asArray() const {
    return isArray() ? std::get<Array>(data_) : kEmptyArray;
}

const Object& Value::asObject() const {
    return isObject() ? std::get<Object>(data_) : kEmptyObject;
}

const Value* Value::find(std::string_view key) const {
    if (!isObject()) return nullptr;
    for (const auto& [k, v] : std::get<Object>(data_))
        if (k == key) return &v;
    return nullptr;
}

const Value& Value::operator[](std::string_view key) const {
    const Value* v = find(key);
    return v ? *v : kNullValue;
}

Value& Value::set(const std::string& key, Value v) {
    if (!isObject()) data_ = Object{};
    auto& obj = std::get<Object>(data_);
    for (auto& [k, existing] : obj) {
        if (k == key) {
            existing = std::move(v);
            return existing;
        }
    }
    obj.emplace_back(key, std::move(v));
    return obj.back().second;
}

std::string Value::getString(std::string_view key, const std::string& fallback) const {
    const Value* v = find(key);
    return v && v->isString() ? v->asString() : fallback;
}

std::int64_t Value::getInt(std::string_view key, std::int64_t fallback) const {
    const Value* v = find(key);
    return v ? v->asInt(fallback) : fallback;
}

double Value::getNumber(std::string_view key, double fallback) const {
    const Value* v = find(key);
    return v ? v->asNumber(fallback) : fallback;
}

bool Value::getBool(std::string_view key, bool fallback) const {
    const Value* v = find(key);
    return v ? v->asBool(fallback) : fallback;
}

std::vector<std::string> Value::getStringArray(std::string_view key) const {
    std::vector<std::string> out;
    for (const auto& item : (*this)[key].asArray())
        if (item.isString()) out.push_back(item.asString());
    return out;
}

void Value::push_back(Value v) {
    if (!isArray()) data_ = Array{};
    std::get<Array>(data_).push_back(std::move(v));
}

std::size_t Value::size() const {
    if (isArray()) return std::get<Array>(data_).size();
    if (isObject()) return std::get<Object>(data_).size();
    return 0;
}

std::string Value::dump(int indent) const {
    std::string out;
    dumpTo(out, indent, 0);
    return out;
}

void Value::dumpTo(std::string& out, int indent, int depth) const {
    auto newline = [&](int d) {
        if (indent < 0) return;
        out += '\n';
        out.append(static_cast<std::size_t>(indent * d), ' ');
    };

    switch (type()) {
        case Type::Null: out += "null"; break;
        case Type::Bool: out += std::get<bool>(data_) ? "true" : "false"; break;
        case Type::Number: {
            double d = std::get<double>(data_);
            char buf[32];
            if (!std::isfinite(d)) {
                out += "null";
            } else if (std::floor(d) == d && std::fabs(d) < 1e15) {
                std::snprintf(buf, sizeof buf, "%lld", static_cast<long long>(d));
                out += buf;
            } else {
                std::snprintf(buf, sizeof buf, "%.12g", d);
                out += buf;
            }
            break;
        }
        case Type::String: escapeString(out, std::get<std::string>(data_)); break;
        case Type::Array: {
            const auto& arr = std::get<Array>(data_);
            if (arr.empty()) { out += "[]"; break; }
            out += '[';
            for (std::size_t i = 0; i < arr.size(); ++i) {
                if (i) out += ',';
                newline(depth + 1);
                arr[i].dumpTo(out, indent, depth + 1);
            }
            newline(depth);
            out += ']';
            break;
        }
        case Type::Object: {
            const auto& obj = std::get<Object>(data_);
            if (obj.empty()) { out += "{}"; break; }
            out += '{';
            for (std::size_t i = 0; i < obj.size(); ++i) {
                if (i) out += ',';
                newline(depth + 1);
                escapeString(out, obj[i].first);
                out += indent < 0 ? ":" : ": ";
                obj[i].second.dumpTo(out, indent, depth + 1);
            }
            newline(depth);
            out += '}';
            break;
        }
    }
}

Value Value::parse(std::string_view text) {
    return Parser(text).parseDocument();
}

std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open file: " + path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

Value parseFile(const std::string& path) {
    try {
        return Value::parse(readFile(path));
    } catch (const ParseError& e) {
        throw ParseError(path + ": " + e.what());
    }
}

bool writeFileAtomic(const std::string& path, const std::string& content) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path target(path);
    if (target.has_parent_path()) fs::create_directories(target.parent_path(), ec);

    const std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out << content;
        if (!out.good()) return false;
    }
    fs::rename(tmp, target, ec);
    return !ec;
}

}  // namespace json
