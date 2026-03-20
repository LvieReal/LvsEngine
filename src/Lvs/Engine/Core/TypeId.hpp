#pragma once

#include "Lvs/Engine/Core/Types.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <variant>
#include <vector>

namespace Lvs::Engine::Math {
struct Vector3;
struct Color3;
class CFrame;
}

namespace Lvs::Engine::Core {

class Instance;

enum class TypeId {
    Invalid,
    Bool,
    Int,
    Double,
    String,
    Vector3,
    Color3,
    CFrame,
    InstanceRef,
    Enum,
    ByteArray,
};

template <class T>
struct TypeIdOfT {
    static constexpr TypeId value = TypeId::Invalid;
};

template <>
struct TypeIdOfT<std::monostate> {
    static constexpr TypeId value = TypeId::Invalid;
};

template <>
struct TypeIdOfT<bool> {
    static constexpr TypeId value = TypeId::Bool;
};

template <>
struct TypeIdOfT<int64_t> {
    static constexpr TypeId value = TypeId::Int;
};

template <>
struct TypeIdOfT<double> {
    static constexpr TypeId value = TypeId::Double;
};

template <>
struct TypeIdOfT<String> {
    static constexpr TypeId value = TypeId::String;
};

template <>
struct TypeIdOfT<Math::Vector3> {
    static constexpr TypeId value = TypeId::Vector3;
};

template <>
struct TypeIdOfT<Math::Color3> {
    static constexpr TypeId value = TypeId::Color3;
};

template <>
struct TypeIdOfT<Math::CFrame> {
    static constexpr TypeId value = TypeId::CFrame;
};

template <>
struct TypeIdOfT<std::weak_ptr<Instance>> {
    static constexpr TypeId value = TypeId::InstanceRef;
};

template <>
struct TypeIdOfT<int> {
    static constexpr TypeId value = TypeId::Enum;
};

template <>
struct TypeIdOfT<std::vector<std::byte>> {
    static constexpr TypeId value = TypeId::ByteArray;
};

template <class T>
constexpr TypeId TypeIdOf() {
    using U = std::remove_cvref_t<T>;
    return TypeIdOfT<U>::value;
}

} // namespace Lvs::Engine::Core
