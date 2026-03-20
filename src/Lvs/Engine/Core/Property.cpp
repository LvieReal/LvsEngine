#include "Lvs/Engine/Core/Property.hpp"

#include <stdexcept>
#include <string>

namespace Lvs::Engine::Core {

Property::Property(PropertyDefinition definition)
    : definition_(std::move(definition)),
      value_(definition_.Default) {
}

const PropertyDefinition& Property::Definition() const {
    return definition_;
}

const Variant& Property::Get() const {
    return value_;
}

void Property::Set(const Variant& value) {
    if (value.IsValid()) {
        if (definition_.IsInstanceReference) {
            if (value_ == value) {
                return;
            }

            const Variant oldValue = value_;
            value_ = value;
            Changed.Fire(oldValue, value_);
            return;
        }

        Variant converted = value;
        if (!converted.Convert(definition_.Type)) {
            throw std::runtime_error(
                "Property '" + definition_.Name + "' had incompatible type."
            );
        }

        if (value_ == converted) {
            return;
        }

        const Variant oldValue = value_;
        value_ = converted;
        Changed.Fire(oldValue, value_);
        return;
    }

    if (!value_.IsValid() && !value_.IsNull()) {
        return;
    }

    const Variant oldValue = value_;
    value_ = Variant{};
    Changed.Fire(oldValue, value_);
}

} // namespace Lvs::Engine::Core
