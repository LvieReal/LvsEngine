#include "Lvs/Engine/Utils/Toml.hpp"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>

namespace Lvs::Engine::Utils::Toml {

namespace {

[[nodiscard]] bool IsBareKeyChar(const char ch) {
    return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-';
}

[[nodiscard]] std::string_view TrimLeft(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.remove_prefix(1);
    }
    return s;
}

[[nodiscard]] std::string_view TrimRight(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.remove_suffix(1);
    }
    return s;
}

[[nodiscard]] std::string_view Trim(std::string_view s) {
    return TrimRight(TrimLeft(s));
}

[[nodiscard]] Core::String UnescapeBasicString(std::string_view s) {
    Core::String out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        const char ch = s[i];
        if (ch != '\\') {
            out.push_back(ch);
            continue;
        }
        if (i + 1 >= s.size()) {
            break;
        }
        const char n = s[++i];
        switch (n) {
        case '\\':
            out.push_back('\\');
            break;
        case '"':
            out.push_back('"');
            break;
        case 'n':
            out.push_back('\n');
            break;
        case 'r':
            out.push_back('\r');
            break;
        case 't':
            out.push_back('\t');
            break;
        default:
            out.push_back(n);
            break;
        }
    }
    return out;
}

struct Cursor {
    std::string_view Input;
    std::size_t Pos{0};

    [[nodiscard]] bool Eof() const { return Pos >= Input.size(); }
    [[nodiscard]] char Peek() const { return Eof() ? '\0' : Input[Pos]; }

    void Advance() {
        if (!Eof()) {
            ++Pos;
        }
    }

    void SkipWs() {
        while (!Eof() && std::isspace(static_cast<unsigned char>(Peek()))) {
            Advance();
        }
    }
};

[[nodiscard]] Core::String ParseKey(Cursor& cur) {
    cur.SkipWs();
    if (cur.Eof()) {
        throw ParseError("Unexpected end while parsing key");
    }

    if (cur.Peek() == '"') {
        cur.Advance();
        const std::size_t start = cur.Pos;
        bool escaped = false;
        while (!cur.Eof()) {
            const char ch = cur.Peek();
            if (!escaped && ch == '"') {
                const std::size_t end = cur.Pos;
                cur.Advance();
                return UnescapeBasicString(cur.Input.substr(start, end - start));
            }
            escaped = (!escaped && ch == '\\');
            cur.Advance();
        }
        throw ParseError("Unterminated quoted key");
    }

    const std::size_t start = cur.Pos;
    while (!cur.Eof() && IsBareKeyChar(cur.Peek())) {
        cur.Advance();
    }
    if (cur.Pos == start) {
        throw ParseError("Invalid key");
    }
    const auto key = cur.Input.substr(start, cur.Pos - start);
    return Core::String(key.data(), key.size());
}

[[nodiscard]] Value ParseValue(Cursor& cur);

[[nodiscard]] Value ParseArray(Cursor& cur) {
    Value v;
    Array arr;

    cur.SkipWs();
    if (cur.Peek() != '[') {
        throw ParseError("Expected '[' to start array");
    }
    cur.Advance();

    for (;;) {
        cur.SkipWs();
        if (cur.Peek() == ']') {
            cur.Advance();
            break;
        }

        arr.push_back(ParseValue(cur));
        cur.SkipWs();
        if (cur.Peek() == ',') {
            cur.Advance();
            continue;
        }
        if (cur.Peek() == ']') {
            cur.Advance();
            break;
        }
        throw ParseError("Expected ',' or ']' in array");
    }

    v.Storage = std::move(arr);
    return v;
}

[[nodiscard]] Value ParseBasicString(Cursor& cur) {
    cur.SkipWs();
    if (cur.Peek() != '"') {
        throw ParseError("Expected string");
    }
    cur.Advance();
    const std::size_t start = cur.Pos;
    bool escaped = false;
    while (!cur.Eof()) {
        const char ch = cur.Peek();
        if (!escaped && ch == '"') {
            const std::size_t end = cur.Pos;
            cur.Advance();
            Value v;
            v.Storage = UnescapeBasicString(cur.Input.substr(start, end - start));
            return v;
        }
        escaped = (!escaped && ch == '\\');
        cur.Advance();
    }
    throw ParseError("Unterminated string");
}

[[nodiscard]] bool LooksLikeFloat(std::string_view s) {
    for (const char ch : s) {
        if (ch == '.' || ch == 'e' || ch == 'E') {
            return true;
        }
    }
    return false;
}

[[nodiscard]] Value ParseLiteral(Cursor& cur) {
    cur.SkipWs();
    const std::size_t start = cur.Pos;
    while (!cur.Eof()) {
        const char ch = cur.Peek();
        if (std::isspace(static_cast<unsigned char>(ch)) || ch == '#' || ch == ',' || ch == ']') {
            break;
        }
        cur.Advance();
    }
    const auto raw = cur.Input.substr(start, cur.Pos - start);
    const auto token = Trim(raw);
    if (token.empty()) {
        return {};
    }

    if (token == "true") {
        Value v;
        v.Storage = true;
        return v;
    }
    if (token == "false") {
        Value v;
        v.Storage = false;
        return v;
    }

    std::string stripped;
    stripped.reserve(token.size());
    for (const char ch : token) {
        if (ch != '_') {
            stripped.push_back(ch);
        }
    }

    if (LooksLikeFloat(stripped)) {
        char* endPtr = nullptr;
        const double d = std::strtod(stripped.c_str(), &endPtr);
        if (endPtr != nullptr && endPtr != stripped.c_str() && *endPtr == '\0' && std::isfinite(d)) {
            Value v;
            v.Storage = d;
            return v;
        }
    } else {
        std::int64_t out = 0;
        const auto* begin = stripped.data();
        const auto* end = stripped.data() + stripped.size();
        const auto res = std::from_chars(begin, end, out);
        if (res.ec == std::errc{} && res.ptr == end) {
            Value v;
            v.Storage = out;
            return v;
        }
    }

    throw ParseError("Invalid value token (use quotes for strings)");
}

[[nodiscard]] Value ParseValue(Cursor& cur) {
    cur.SkipWs();
    const char ch = cur.Peek();
    if (ch == '"') {
        return ParseBasicString(cur);
    }
    if (ch == '[') {
        return ParseArray(cur);
    }
    return ParseLiteral(cur);
}

[[nodiscard]] std::vector<Core::String> ParseDottedPath(std::string_view text) {
    std::vector<Core::String> parts;
    Cursor cur{.Input = text, .Pos = 0};
    for (;;) {
        cur.SkipWs();
        if (cur.Eof()) {
            break;
        }
        parts.push_back(ParseKey(cur));
        cur.SkipWs();
        if (cur.Eof()) {
            break;
        }
        if (cur.Peek() != '.') {
            throw ParseError("Expected '.' in dotted path");
        }
        cur.Advance();
    }
    return parts;
}

void SetValueOrdered(Table& table, const Core::String& key, Value value) {
    for (auto& [k, v] : table.Values) {
        if (k == key) {
            v = std::move(value);
            return;
        }
    }
    table.Values.emplace_back(key, std::move(value));
}

[[nodiscard]] bool IsBareKey(std::string_view key) {
    if (key.empty()) {
        return false;
    }
    for (const char ch : key) {
        if (!IsBareKeyChar(ch)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] Core::String EscapeBasicString(std::string_view s) {
    Core::String out;
    out.reserve(s.size() + 8);
    for (const char ch : s) {
        switch (ch) {
        case '\\':
            out += "\\\\";
            break;
        case '"':
            out += "\\\"";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            out.push_back(ch);
            break;
        }
    }
    return out;
}

[[nodiscard]] Core::String FormatKey(const Core::String& key) {
    if (IsBareKey(key)) {
        return key;
    }
    return "\"" + EscapeBasicString(key) + "\"";
}

[[nodiscard]] Core::String FormatDouble(const double value) {
    std::ostringstream oss;
    oss.setf(std::ios::fmtflags(0), std::ios::floatfield);
    oss << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
    return oss.str();
}

Core::String SerializeValueInternal(const Value& value);

Core::String SerializeArrayInternal(const Array& arr) {
    Core::String out = "[";
    for (std::size_t i = 0; i < arr.size(); ++i) {
        if (i != 0) {
            out += ", ";
        }
        out += SerializeValueInternal(arr[i]);
    }
    out += "]";
    return out;
}

Core::String SerializeValueInternal(const Value& value) {
    if (const auto* b = std::get_if<bool>(&value.Storage)) {
        return *b ? "true" : "false";
    }
    if (const auto* i = std::get_if<std::int64_t>(&value.Storage)) {
        return std::to_string(*i);
    }
    if (const auto* d = std::get_if<double>(&value.Storage)) {
        return FormatDouble(*d);
    }
    if (const auto* s = std::get_if<Core::String>(&value.Storage)) {
        return "\"" + EscapeBasicString(*s) + "\"";
    }
    if (const auto* a = std::get_if<Array>(&value.Storage)) {
        return SerializeArrayInternal(*a);
    }
    return "\"\"";
}

void SerializeTableRecursive(
    std::ostringstream& oss,
    const Table& table,
    std::vector<Core::String>& path,
    const bool writeHeader
) {
    if (writeHeader) {
        oss << "[";
        for (std::size_t i = 0; i < path.size(); ++i) {
            if (i != 0) {
                oss << ".";
            }
            oss << FormatKey(path[i]);
        }
        oss << "]\n";
    }

    for (const auto& [k, v] : table.Values) {
        oss << FormatKey(k) << " = " << SerializeValueInternal(v) << "\n";
    }
    if (writeHeader || !table.Values.empty()) {
        oss << "\n";
    }

    for (const auto& [childKey, child] : table.Children) {
        path.push_back(childKey);
        SerializeTableRecursive(oss, child, path, true);
        path.pop_back();
    }
}

} // namespace

bool Value::AsBool(const bool fallback) const {
    if (const auto* v = std::get_if<bool>(&Storage)) {
        return *v;
    }
    return fallback;
}

std::int64_t Value::AsInt(const std::int64_t fallback) const {
    if (const auto* v = std::get_if<std::int64_t>(&Storage)) {
        return *v;
    }
    return fallback;
}

double Value::AsDouble(const double fallback) const {
    if (const auto* v = std::get_if<double>(&Storage)) {
        return *v;
    }
    if (const auto* v = std::get_if<std::int64_t>(&Storage)) {
        return static_cast<double>(*v);
    }
    return fallback;
}

const Core::String& Value::AsString() const {
    static const Core::String empty{};
    if (const auto* v = std::get_if<Core::String>(&Storage)) {
        return *v;
    }
    return empty;
}

const Array& Value::AsArray() const {
    static const Array empty{};
    if (const auto* v = std::get_if<Array>(&Storage)) {
        return *v;
    }
    return empty;
}

Document Parse(std::string_view input) {
    if (input.size() >= 3 &&
        static_cast<unsigned char>(input[0]) == 0xEF &&
        static_cast<unsigned char>(input[1]) == 0xBB &&
        static_cast<unsigned char>(input[2]) == 0xBF) {
        input.remove_prefix(3);
    }

    auto stripComments = [](std::string_view text) -> std::string {
        std::string out;
        out.reserve(text.size());
        bool inString = false;
        bool escaped = false;
        for (std::size_t i = 0; i < text.size(); ++i) {
            const char ch = text[i];
            if (inString) {
                out.push_back(ch);
                if (!escaped && ch == '"') {
                    inString = false;
                }
                escaped = (!escaped && ch == '\\');
                continue;
            }

            if (ch == '"') {
                inString = true;
                escaped = false;
                out.push_back(ch);
                continue;
            }

            if (ch == '#') {
                while (i < text.size() && text[i] != '\n') {
                    ++i;
                }
                if (i < text.size() && text[i] == '\n') {
                    out.push_back('\n');
                }
                continue;
            }
            out.push_back(ch);
        }
        return out;
    };

    auto updateArrayDepth = [](std::string_view text, bool& inString, bool& escaped, int& arrayDepth) {
        for (std::size_t i = 0; i < text.size(); ++i) {
            const char ch = text[i];
            if (inString) {
                if (!escaped && ch == '"') {
                    inString = false;
                }
                escaped = (!escaped && ch == '\\');
                continue;
            }

            if (ch == '"') {
                inString = true;
                escaped = false;
                continue;
            }

            if (ch == '#') {
                while (i < text.size() && text[i] != '\n') {
                    ++i;
                }
                continue;
            }

            if (ch == '[') {
                ++arrayDepth;
            } else if (ch == ']') {
                arrayDepth = std::max(0, arrayDepth - 1);
            }
        }
    };

    Document doc;
    Table* current = &doc.Root;

    std::size_t lineStart = 0;
    while (lineStart <= input.size()) {
        std::size_t lineEnd = input.find('\n', lineStart);
        if (lineEnd == std::string_view::npos) {
            lineEnd = input.size();
        }
        std::string_view line = input.substr(lineStart, lineEnd - lineStart);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }

        line = Trim(line);
        if (line.empty() || line.front() == '#') {
            lineStart = lineEnd + 1;
            continue;
        }

        // Table headers: [a.b]
        if (line.front() == '[') {
            if (line.size() < 2 || line.back() != ']') {
                throw ParseError("Invalid table header");
            }
            std::string_view inner = line.substr(1, line.size() - 2);
            inner = Trim(inner);
            const auto path = ParseDottedPath(inner);
            current = &GetOrCreateTablePath(doc.Root, path);
            lineStart = lineEnd + 1;
            continue;
        }

        // Key/value: key = value
        const std::size_t eq = line.find('=');
        if (eq == std::string_view::npos) {
            throw ParseError("Expected '=' in key/value");
        }
        std::string_view left = TrimRight(line.substr(0, eq));
        std::string_view right = TrimLeft(line.substr(eq + 1));

        Cursor curKey{.Input = left, .Pos = 0};
        const Core::String key = ParseKey(curKey);
        curKey.SkipWs();
        if (!curKey.Eof()) {
            throw ParseError("Trailing characters after key");
        }

        std::string valueText(right.data(), right.size());
        {
            const std::string_view trimmed = TrimLeft(std::string_view(valueText));
            if (!trimmed.empty() && trimmed.front() == '[') {
                bool inString = false;
                bool escaped = false;
                int arrayDepth = 0;
                updateArrayDepth(valueText, inString, escaped, arrayDepth);
                while (arrayDepth > 0) {
                    if (lineEnd == input.size()) {
                        throw ParseError("Unterminated array");
                    }
                    lineStart = lineEnd + 1;
                    lineEnd = input.find('\n', lineStart);
                    if (lineEnd == std::string_view::npos) {
                        lineEnd = input.size();
                    }
                    std::string_view extra = input.substr(lineStart, lineEnd - lineStart);
                    if (!extra.empty() && extra.back() == '\r') {
                        extra.remove_suffix(1);
                    }
                    valueText.push_back('\n');
                    valueText.append(extra.data(), extra.size());
                    updateArrayDepth(extra, inString, escaped, arrayDepth);
                }
            }
        }
        valueText = stripComments(valueText);

        Cursor curVal{.Input = valueText, .Pos = 0};
        Value value = ParseValue(curVal);
        curVal.SkipWs();
        if (!curVal.Eof()) {
            throw ParseError("Trailing characters after value");
        }

        SetValueOrdered(*current, key, std::move(value));

        lineStart = lineEnd + 1;
    }

    return doc;
}

Core::String Serialize(const Document& doc) {
    std::ostringstream oss;
    std::vector<Core::String> path;
    SerializeTableRecursive(oss, doc.Root, path, false);
    const std::string s = oss.str();
    return Core::String(s.data(), s.size());
}

const Value* FindValue(const Table& table, std::string_view key) {
    for (const auto& [k, v] : table.Values) {
        if (k == key) {
            return &v;
        }
    }
    return nullptr;
}

const Table* FindChild(const Table& table, std::string_view key) {
    for (const auto& [k, t] : table.Children) {
        if (k == key) {
            return &t;
        }
    }
    return nullptr;
}

Table& GetOrCreateChild(Table& table, const Core::String& key) {
    for (auto& [k, t] : table.Children) {
        if (k == key) {
            return t;
        }
    }
    table.Children.emplace_back(key, Table{});
    return table.Children.back().second;
}

Table& GetOrCreateTablePath(Table& root, const std::vector<Core::String>& path) {
    Table* current = &root;
    for (const auto& part : path) {
        current = &GetOrCreateChild(*current, part);
    }
    return *current;
}

} // namespace Lvs::Engine::Utils::Toml
