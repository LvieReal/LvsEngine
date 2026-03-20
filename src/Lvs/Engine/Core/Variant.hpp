#pragma once

#include "Lvs/Engine/Core/TypeId.hpp"
#include "Lvs/Engine/Core/Types.hpp"
#include "Lvs/Engine/Math/CFrame.hpp"
#include "Lvs/Engine/Math/Color3.hpp"
#include "Lvs/Engine/Math/Vector3.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace Lvs::Engine::Core {

class Instance;

class Variant {
public:
    using InstanceRef = std::weak_ptr<Instance>;
    using ByteArray = std::vector<std::byte>;
    using Storage = std::variant<
        std::monostate,
        bool,
        int64_t,
        double,
        String,
        Math::Vector3,
        Math::Color3,
        Math::CFrame,
        InstanceRef,
        int,
        ByteArray>;

    Variant() = default;
    Variant(const Variant&) = default;
    Variant& operator=(const Variant&) = default;
    Variant(Variant&&) noexcept = default;
    Variant& operator=(Variant&&) noexcept = default;

    template <class T>
    static Variant From(T&& value) {
        using U = std::remove_cvref_t<T>;

        if constexpr (std::is_same_v<U, Variant>) {
            return std::forward<T>(value);
        } else if constexpr (std::is_same_v<U, StringView>) {
            return Variant(String(value));
        } else if constexpr (std::is_same_v<U, const char*> || std::is_same_v<U, char*>) {
            return Variant(String(value));
        } else if constexpr (
            std::is_integral_v<U> && !std::is_same_v<U, bool> && !std::is_same_v<U, int64_t> && !std::is_same_v<U, int>
        ) {
            return Variant(static_cast<int64_t>(value));
        } else if constexpr (std::is_floating_point_v<U> && !std::is_same_v<U, double>) {
            return Variant(static_cast<double>(value));
        } else if constexpr (std::is_enum_v<U>) {
            return Variant(static_cast<int>(value));
        } else {
            return Variant(std::forward<T>(value));
        }
    }

    template <class T>
    [[nodiscard]] bool Is() const {
        using U = std::remove_cvref_t<T>;
        return std::holds_alternative<U>(value_);
    }

    [[nodiscard]] bool IsValid() const { return !std::holds_alternative<std::monostate>(value_); }
    [[nodiscard]] bool IsNull() const { return !IsValid(); }

    template <class T>
    [[nodiscard]] const T& Get() const {
        return std::get<T>(value_);
    }

    [[nodiscard]] TypeId GetTypeId() const {
        return std::visit([](const auto& v) { return TypeIdOf<decltype(v)>(); }, value_);
    }

    friend bool operator==(const Variant& a, const Variant& b) {
        if (a.value_.index() != b.value_.index()) {
            return false;
        }
        return std::visit(
            [](const auto& av, const auto& bv) -> bool {
                using A = std::remove_cvref_t<decltype(av)>;
                using B = std::remove_cvref_t<decltype(bv)>;
                if constexpr (!std::is_same_v<A, B>) {
                    return false;
                } else if constexpr (std::is_same_v<A, InstanceRef>) {
                    return !av.owner_before(bv) && !bv.owner_before(av);
                } else {
                    return av == bv;
                }
            },
            a.value_,
            b.value_
        );
    }
    friend bool operator!=(const Variant& a, const Variant& b) { return !(a == b); }

    // Compatibility helpers for old QVariant-style call sites.
    [[nodiscard]] bool isValid() const { return IsValid(); }
    [[nodiscard]] bool isNull() const { return IsNull(); }
    [[nodiscard]] TypeId typeId() const { return GetTypeId(); }

    template <class T>
    [[nodiscard]] T value() const {
        if constexpr (std::is_enum_v<T>) {
            return static_cast<T>(toInt());
        } else {
            return Get<T>();
        }
    }

    [[nodiscard]] bool toBool() const {
        Variant tmp = *this;
        if (!tmp.Convert(TypeId::Bool) || !tmp.Is<bool>()) {
            return false;
        }
        return tmp.Get<bool>();
    }

    [[nodiscard]] int toInt(const int fallback = 0) const {
        if (Is<int>()) {
            return Get<int>();
        }
        Variant tmp = *this;
        if (!tmp.Convert(TypeId::Int) || !tmp.Is<int64_t>()) {
            return fallback;
        }
        const int64_t v = tmp.Get<int64_t>();
        if (v < static_cast<int64_t>(std::numeric_limits<int>::min())) {
            return std::numeric_limits<int>::min();
        }
        if (v > static_cast<int64_t>(std::numeric_limits<int>::max())) {
            return std::numeric_limits<int>::max();
        }
        return static_cast<int>(v);
    }

    [[nodiscard]] int64_t toLongLong(const int64_t fallback = 0) const {
        if (Is<int64_t>()) {
            return Get<int64_t>();
        }
        if (Is<int>()) {
            return static_cast<int64_t>(Get<int>());
        }
        Variant tmp = *this;
        if (!tmp.Convert(TypeId::Int) || !tmp.Is<int64_t>()) {
            return fallback;
        }
        return tmp.Get<int64_t>();
    }

    [[nodiscard]] double toDouble(const double fallback = 0.0) const {
        Variant tmp = *this;
        if (!tmp.Convert(TypeId::Double) || !tmp.Is<double>()) {
            return fallback;
        }
        return tmp.Get<double>();
    }

    [[nodiscard]] String toString() const {
        if (Is<String>()) {
            return Get<String>();
        }
        Variant tmp = *this;
        if (!tmp.Convert(TypeId::String) || !tmp.Is<String>()) {
            return {};
        }
        return tmp.Get<String>();
    }

    bool Convert(TypeId target) {
        if (target == GetTypeId()) {
            return true;
        }
        if (!IsValid()) {
            return false;
        }

        switch (target) {
        case TypeId::Invalid:
            value_ = std::monostate{};
            return true;
        case TypeId::Bool:
            return ConvertToBool();
        case TypeId::Int:
            return ConvertToInt();
        case TypeId::Double:
            return ConvertToDouble();
        case TypeId::String:
            return ConvertToString();
        case TypeId::Enum:
            return ConvertToEnum();
        case TypeId::Vector3:
        case TypeId::Color3:
        case TypeId::CFrame:
        case TypeId::InstanceRef:
        case TypeId::ByteArray:
            return false;
        }
        return false;
    }

private:
    Storage value_{std::monostate{}};

    explicit Variant(Storage v) : value_(std::move(v)) {}

    template <class T>
        requires(!std::is_same_v<std::remove_cvref_t<T>, Variant>)
    explicit Variant(T&& v) : value_(std::forward<T>(v)) {}

    static String TrimCopy(StringView input) {
        size_t start = 0;
        size_t end = input.size();

        while (start < end && std::isspace(static_cast<unsigned char>(input[start])) != 0) {
            ++start;
        }
        while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1])) != 0) {
            --end;
        }
        return String(input.substr(start, end - start));
    }

    static String ToLowerAsciiCopy(StringView input) {
        String out;
        out.reserve(input.size());
        for (char ch : input) {
            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        }
        return out;
    }

    static std::optional<int64_t> ParseInt64Strict(StringView input) {
        const String trimmed = TrimCopy(input);
        if (trimmed.empty()) {
            return std::nullopt;
        }
        size_t pos = 0;
        try {
            long long v = std::stoll(trimmed, &pos, 10);
            if (pos != trimmed.size()) {
                return std::nullopt;
            }
            return static_cast<int64_t>(v);
        } catch (...) {
            return std::nullopt;
        }
    }

    static std::optional<double> ParseDoubleStrict(StringView input) {
        const String trimmed = TrimCopy(input);
        if (trimmed.empty()) {
            return std::nullopt;
        }
        size_t pos = 0;
        try {
            double v = std::stod(trimmed, &pos);
            if (pos != trimmed.size()) {
                return std::nullopt;
            }
            return v;
        } catch (...) {
            return std::nullopt;
        }
    }

    bool ConvertToBool() {
        if (std::holds_alternative<bool>(value_)) {
            return true;
        }
        if (std::holds_alternative<int64_t>(value_)) {
            value_ = (std::get<int64_t>(value_) != 0);
            return true;
        }
        if (std::holds_alternative<double>(value_)) {
            value_ = (std::get<double>(value_) != 0.0);
            return true;
        }
        if (std::holds_alternative<int>(value_)) {
            value_ = (std::get<int>(value_) != 0);
            return true;
        }
        if (std::holds_alternative<String>(value_)) {
            const String lowered = ToLowerAsciiCopy(TrimCopy(std::get<String>(value_)));
            value_ = (lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on");
            return true;
        }
        return false;
    }

    bool ConvertToInt() {
        if (std::holds_alternative<int64_t>(value_)) {
            return true;
        }
        if (std::holds_alternative<bool>(value_)) {
            value_ = static_cast<int64_t>(std::get<bool>(value_) ? 1 : 0);
            return true;
        }
        if (std::holds_alternative<double>(value_)) {
            value_ = static_cast<int64_t>(std::get<double>(value_));
            return true;
        }
        if (std::holds_alternative<int>(value_)) {
            value_ = static_cast<int64_t>(std::get<int>(value_));
            return true;
        }
        if (std::holds_alternative<String>(value_)) {
            const auto parsed = ParseInt64Strict(std::get<String>(value_));
            if (!parsed) {
                return false;
            }
            value_ = *parsed;
            return true;
        }
        return false;
    }

    bool ConvertToDouble() {
        if (std::holds_alternative<double>(value_)) {
            return true;
        }
        if (std::holds_alternative<bool>(value_)) {
            value_ = static_cast<double>(std::get<bool>(value_) ? 1.0 : 0.0);
            return true;
        }
        if (std::holds_alternative<int64_t>(value_)) {
            value_ = static_cast<double>(std::get<int64_t>(value_));
            return true;
        }
        if (std::holds_alternative<int>(value_)) {
            value_ = static_cast<double>(std::get<int>(value_));
            return true;
        }
        if (std::holds_alternative<String>(value_)) {
            const auto parsed = ParseDoubleStrict(std::get<String>(value_));
            if (!parsed) {
                return false;
            }
            value_ = *parsed;
            return true;
        }
        return false;
    }

    bool ConvertToEnum() {
        if (std::holds_alternative<int>(value_)) {
            return true;
        }
        if (std::holds_alternative<bool>(value_)) {
            value_ = static_cast<int>(std::get<bool>(value_) ? 1 : 0);
            return true;
        }
        if (std::holds_alternative<int64_t>(value_)) {
            const int64_t v = std::get<int64_t>(value_);
            if (v < static_cast<int64_t>(std::numeric_limits<int>::min())
                || v > static_cast<int64_t>(std::numeric_limits<int>::max())) {
                return false;
            }
            value_ = static_cast<int>(v);
            return true;
        }
        if (std::holds_alternative<double>(value_)) {
            const double v = std::get<double>(value_);
            if (v < static_cast<double>(std::numeric_limits<int>::min())
                || v > static_cast<double>(std::numeric_limits<int>::max())) {
                return false;
            }
            value_ = static_cast<int>(v);
            return true;
        }
        if (std::holds_alternative<String>(value_)) {
            const auto parsed = ParseInt64Strict(std::get<String>(value_));
            if (!parsed) {
                return false;
            }
            if (*parsed < static_cast<int64_t>(std::numeric_limits<int>::min())
                || *parsed > static_cast<int64_t>(std::numeric_limits<int>::max())) {
                return false;
            }
            value_ = static_cast<int>(*parsed);
            return true;
        }
        return false;
    }

    bool ConvertToString() {
        if (std::holds_alternative<String>(value_)) {
            return true;
        }
        if (std::holds_alternative<bool>(value_)) {
            value_ = String(std::get<bool>(value_) ? "true" : "false");
            return true;
        }
        if (std::holds_alternative<int64_t>(value_)) {
            value_ = std::to_string(std::get<int64_t>(value_));
            return true;
        }
        if (std::holds_alternative<double>(value_)) {
            value_ = std::to_string(std::get<double>(value_));
            return true;
        }
        if (std::holds_alternative<int>(value_)) {
            value_ = std::to_string(std::get<int>(value_));
            return true;
        }
        return false;
    }
};

} // namespace Lvs::Engine::Core
