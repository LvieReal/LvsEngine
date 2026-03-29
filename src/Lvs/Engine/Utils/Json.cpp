#include "Lvs/Engine/Utils/Json.hpp"

#include <cctype>
#include <charconv>
#include <optional>
#include <string>

namespace Lvs::Engine::Utils::Json {

bool Value::AsBool(const bool fallback) const {
    if (!IsBool()) {
        return fallback;
    }
    return std::get<bool>(Storage);
}

double Value::AsNumber(const double fallback) const {
    if (!IsNumber()) {
        return fallback;
    }
    return std::get<double>(Storage);
}

const Core::String& Value::AsString() const {
    if (!IsString()) {
        static const Core::String empty{};
        return empty;
    }
    return std::get<Core::String>(Storage);
}

const Object& Value::AsObject() const {
    if (!IsObject()) {
        static const Object empty{};
        return empty;
    }
    return std::get<Object>(Storage);
}

const Array& Value::AsArray() const {
    if (!IsArray()) {
        static const Array empty{};
        return empty;
    }
    return std::get<Array>(Storage);
}

namespace {

class Parser final {
public:
    explicit Parser(std::string_view input)
        : input_(input) {}

    [[nodiscard]] Value ParseValue() {
        SkipWhitespace();
        if (Eof()) {
            throw ParseError("Unexpected end of input");
        }

        const char ch = Peek();
        if (ch == 'n') {
            ConsumeLiteral("null");
            return Value{.Type = Kind::Null, .Storage = std::monostate{}};
        }
        if (ch == 't') {
            ConsumeLiteral("true");
            return Value{.Type = Kind::Bool, .Storage = true};
        }
        if (ch == 'f') {
            ConsumeLiteral("false");
            return Value{.Type = Kind::Bool, .Storage = false};
        }
        if (ch == '"') {
            return ParseString();
        }
        if (ch == '{') {
            return ParseObject();
        }
        if (ch == '[') {
            return ParseArray();
        }
        if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch)) != 0) {
            return ParseNumber();
        }

        throw ParseError(Core::String("Unexpected character: ") + ch);
    }

    void ExpectEof() {
        SkipWhitespace();
        if (!Eof()) {
            throw ParseError("Trailing characters after JSON value");
        }
    }

private:
    std::string_view input_;
    std::size_t pos_{0};

    [[nodiscard]] bool Eof() const { return pos_ >= input_.size(); }

    [[nodiscard]] char Peek() const { return input_[pos_]; }

    char Consume() { return input_[pos_++]; }

    void SkipWhitespace() {
        while (!Eof()) {
            const unsigned char ch = static_cast<unsigned char>(Peek());
            if (std::isspace(ch) == 0) {
                break;
            }
            ++pos_;
        }
    }

    void ConsumeLiteral(const char* literal) {
        const std::size_t len = std::char_traits<char>::length(literal);
        if (pos_ + len > input_.size()) {
            throw ParseError("Unexpected end of input");
        }
        if (input_.substr(pos_, len) != std::string_view(literal, len)) {
            throw ParseError("Invalid literal");
        }
        pos_ += len;
    }

    Value ParseString() {
        if (Consume() != '"') {
            throw ParseError("Expected string");
        }

        Core::String out;
        while (!Eof()) {
            const char ch = Consume();
            if (ch == '"') {
                return Value{.Type = Kind::String, .Storage = std::move(out)};
            }
            if (ch == '\\') {
                if (Eof()) {
                    throw ParseError("Invalid escape sequence");
                }
                const char esc = Consume();
                switch (esc) {
                case '"':
                case '\\':
                case '/':
                    out.push_back(esc);
                    break;
                case 'b':
                    out.push_back('\b');
                    break;
                case 'f':
                    out.push_back('\f');
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
                case 'u': {
                    // Minimal \uXXXX support for ASCII range only.
                    if (pos_ + 4 > input_.size()) {
                        throw ParseError("Invalid unicode escape");
                    }
                    unsigned int code = 0;
                    for (int i = 0; i < 4; ++i) {
                        const char hex = input_[pos_ + i];
                        code <<= 4U;
                        if (hex >= '0' && hex <= '9') {
                            code |= static_cast<unsigned int>(hex - '0');
                        } else if (hex >= 'a' && hex <= 'f') {
                            code |= static_cast<unsigned int>(hex - 'a' + 10);
                        } else if (hex >= 'A' && hex <= 'F') {
                            code |= static_cast<unsigned int>(hex - 'A' + 10);
                        } else {
                            throw ParseError("Invalid unicode escape");
                        }
                    }
                    pos_ += 4;
                    if (code <= 0x7F) {
                        out.push_back(static_cast<char>(code));
                    } else {
                        // Non-ASCII not supported in this minimal parser.
                        throw ParseError("Non-ASCII unicode escapes are not supported");
                    }
                    break;
                }
                default:
                    throw ParseError("Invalid escape sequence");
                }
                continue;
            }

            out.push_back(ch);
        }

        throw ParseError("Unterminated string");
    }

    Value ParseNumber() {
        const std::size_t start = pos_;
        if (Peek() == '-') {
            Consume();
        }
        if (Eof()) {
            throw ParseError("Invalid number");
        }

        if (Peek() == '0') {
            Consume();
        } else {
            if (std::isdigit(static_cast<unsigned char>(Peek())) == 0) {
                throw ParseError("Invalid number");
            }
            while (!Eof() && std::isdigit(static_cast<unsigned char>(Peek())) != 0) {
                Consume();
            }
        }

        if (!Eof() && Peek() == '.') {
            Consume();
            if (Eof() || std::isdigit(static_cast<unsigned char>(Peek())) == 0) {
                throw ParseError("Invalid number");
            }
            while (!Eof() && std::isdigit(static_cast<unsigned char>(Peek())) != 0) {
                Consume();
            }
        }

        if (!Eof() && (Peek() == 'e' || Peek() == 'E')) {
            Consume();
            if (!Eof() && (Peek() == '+' || Peek() == '-')) {
                Consume();
            }
            if (Eof() || std::isdigit(static_cast<unsigned char>(Peek())) == 0) {
                throw ParseError("Invalid number");
            }
            while (!Eof() && std::isdigit(static_cast<unsigned char>(Peek())) != 0) {
                Consume();
            }
        }

        const std::string_view sv = input_.substr(start, pos_ - start);

        double value = 0.0;
        // Use strtod for portability (from_chars for floating is not universally available).
        try {
            value = std::stod(std::string(sv));
        } catch (...) {
            throw ParseError("Invalid number");
        }

        return Value{.Type = Kind::Number, .Storage = value};
    }

    Value ParseArray() {
        if (Consume() != '[') {
            throw ParseError("Expected array");
        }

        Array arr;
        SkipWhitespace();
        if (!Eof() && Peek() == ']') {
            Consume();
            return Value{.Type = Kind::Array, .Storage = std::move(arr)};
        }

        while (true) {
            Value v = ParseValue();
            arr.push_back(std::move(v));
            SkipWhitespace();
            if (Eof()) {
                throw ParseError("Unterminated array");
            }
            const char ch = Consume();
            if (ch == ']') {
                return Value{.Type = Kind::Array, .Storage = std::move(arr)};
            }
            if (ch != ',') {
                throw ParseError("Expected ',' or ']'");
            }
            SkipWhitespace();
        }
    }

    Value ParseObject() {
        if (Consume() != '{') {
            throw ParseError("Expected object");
        }

        Object obj;
        SkipWhitespace();
        if (!Eof() && Peek() == '}') {
            Consume();
            return Value{.Type = Kind::Object, .Storage = std::move(obj)};
        }

        while (true) {
            SkipWhitespace();
            if (Eof() || Peek() != '"') {
                throw ParseError("Expected object key string");
            }
            Value key = ParseString();
            const Core::String& keyStr = std::get<Core::String>(key.Storage);

            SkipWhitespace();
            if (Eof() || Consume() != ':') {
                throw ParseError("Expected ':'");
            }

            Value val = ParseValue();
            obj.emplace_back(keyStr, std::move(val));

            SkipWhitespace();
            if (Eof()) {
                throw ParseError("Unterminated object");
            }
            const char ch = Consume();
            if (ch == '}') {
                return Value{.Type = Kind::Object, .Storage = std::move(obj)};
            }
            if (ch != ',') {
                throw ParseError("Expected ',' or '}'");
            }
            SkipWhitespace();
        }
    }
};

} // namespace

Value Parse(const std::string_view input) {
    Parser p(input);
    Value v = p.ParseValue();
    p.ExpectEof();
    return v;
}

const Value* Find(const Object& object, const std::string_view key) {
    for (const auto& [k, v] : object) {
        if (k == key) {
            return &v;
        }
    }
    return nullptr;
}

} // namespace Lvs::Engine::Utils::Json
