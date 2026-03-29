#pragma once

#include "Lvs/Engine/Core/PropertyDefinition.hpp"
#include "Lvs/Engine/Core/Variant.hpp"
#include "Lvs/Engine/Utils/Signal.hpp"

namespace Lvs::Engine::Core {

class Property {
public:
    explicit Property(PropertyDefinition definition);

    [[nodiscard]] const PropertyDefinition& Definition() const;
    [[nodiscard]] const Variant& Get() const;
    void Set(const Variant& value);
    void ReplaceDefinition(PropertyDefinition definition, bool preserveValue = true);

    Utils::Signal<const Variant&, const Variant&> Changed;

private:
    PropertyDefinition definition_;
    Variant value_;
};

} // namespace Lvs::Engine::Core
