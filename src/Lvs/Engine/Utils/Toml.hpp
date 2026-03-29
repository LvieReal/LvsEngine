#pragma once

#include "Lvs/Engine/Core/Types.hpp"

#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace Lvs::Engine::Utils::Toml {

struct Value;
struct Table;

using Array = std::vector<Value>;

struct Value {
    std::variant<std::monostate, bool, std::int64_t, double, Core::String, Array> Storage{};

    [[nodiscard]] bool IsBool() const { return std::holds_alternative<bool>(Storage); }
    [[nodiscard]] bool IsInt() const { return std::holds_alternative<std::int64_t>(Storage); }
    [[nodiscard]] bool IsDouble() const { return std::holds_alternative<double>(Storage); }
    [[nodiscard]] bool IsString() const { return std::holds_alternative<Core::String>(Storage); }
    [[nodiscard]] bool IsArray() const { return std::holds_alternative<Array>(Storage); }

    [[nodiscard]] bool AsBool(bool fallback = false) const;
    [[nodiscard]] std::int64_t AsInt(std::int64_t fallback = 0) const;
    [[nodiscard]] double AsDouble(double fallback = 0.0) const;
    [[nodiscard]] const Core::String& AsString() const;
    [[nodiscard]] const Array& AsArray() const;
};

struct Table {
    std::vector<std::pair<Core::String, Value>> Values{};
    std::vector<std::pair<Core::String, Table>> Children{};
};

struct Document {
    Table Root{};
};

class ParseError : public std::runtime_error {
public:
    explicit ParseError(const Core::String& message)
        : std::runtime_error(message) {}
};

[[nodiscard]] Document Parse(std::string_view input);
[[nodiscard]] Core::String Serialize(const Document& doc);

[[nodiscard]] const Value* FindValue(const Table& table, std::string_view key);
[[nodiscard]] const Table* FindChild(const Table& table, std::string_view key);

[[nodiscard]] Table& GetOrCreateChild(Table& table, const Core::String& key);
[[nodiscard]] Table& GetOrCreateTablePath(Table& root, const std::vector<Core::String>& path);

} // namespace Lvs::Engine::Utils::Toml
