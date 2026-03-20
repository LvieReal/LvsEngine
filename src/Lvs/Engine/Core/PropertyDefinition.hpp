#pragma once

#include "Lvs/Engine/Core/TypeId.hpp"
#include "Lvs/Engine/Core/Types.hpp"
#include "Lvs/Engine/Core/Variant.hpp"

namespace Lvs::Engine::Core {

struct PropertyDefinition {
    String Name;
    TypeId Type{TypeId::Invalid};
    Variant Default;
    bool Serializable{true};
    String Category{"Data"};
    String Description;
    bool ReadOnly{false};
    StringList CustomTags;
    HashMap<String, Variant> CustomAttributes;
    bool IsInstanceReference{false};
    int RegistrationOrder{-1};
};

} // namespace Lvs::Engine::Core
