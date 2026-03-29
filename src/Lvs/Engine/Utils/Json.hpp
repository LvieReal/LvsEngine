#pragma once

#include "Lvs/Engine/Core/Types.hpp"

#include <cstddef>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace Lvs::Engine::Utils::Json {

struct Value;

using Object = std::vector<std::pair<Core::String, Value>>;
using Array = std::vector<Value>;

enum class Kind {
    Null,
    Bool,
    Number,
    String,
    Object,
    Array
};

struct Value {
    Kind Type{Kind::Null};
    std::variant<std::monostate, bool, double, Core::String, Object, Array> Storage{};

    [[nodiscard]] bool IsNull() const { return Type == Kind::Null; }
    [[nodiscard]] bool IsBool() const { return Type == Kind::Bool; }
    [[nodiscard]] bool IsNumber() const { return Type == Kind::Number; }
    [[nodiscard]] bool IsString() const { return Type == Kind::String; }
    [[nodiscard]] bool IsObject() const { return Type == Kind::Object; }
    [[nodiscard]] bool IsArray() const { return Type == Kind::Array; }

    [[nodiscard]] bool AsBool(bool fallback = false) const;
    [[nodiscard]] double AsNumber(double fallback = 0.0) const;
    [[nodiscard]] const Core::String& AsString() const;
    [[nodiscard]] const Object& AsObject() const;
    [[nodiscard]] const Array& AsArray() const;
};

class ParseError : public std::runtime_error {
public:
    explicit ParseError(const Core::String& message)
        : std::runtime_error(message) {}
};

[[nodiscard]] Value Parse(std::string_view input);
[[nodiscard]] const Value* Find(const Object& object, std::string_view key);

} // namespace Lvs::Engine::Utils::Json

