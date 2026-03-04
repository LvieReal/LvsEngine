#pragma once

#include "Lvs/Engine/Core/PropertyDefinition.hpp"
#include "Lvs/Engine/Utils/Signal.hpp"

#include <QVariant>

namespace Lvs::Engine::Core {

class Property {
public:
    explicit Property(PropertyDefinition definition);

    [[nodiscard]] const PropertyDefinition& Definition() const;
    [[nodiscard]] const QVariant& Get() const;
    void Set(const QVariant& value);

    Utils::Signal<const QVariant&, const QVariant&> Changed;

private:
    PropertyDefinition definition_;
    QVariant value_;
};

} // namespace Lvs::Engine::Core
