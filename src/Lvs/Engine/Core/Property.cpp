#include "Lvs/Engine/Core/Property.hpp"

#include <stdexcept>

namespace Lvs::Engine::Core {

Property::Property(PropertyDefinition definition)
    : definition_(std::move(definition)),
      value_(definition_.Default) {
}

const PropertyDefinition& Property::Definition() const {
    return definition_;
}

const QVariant& Property::Get() const {
    return value_;
}

void Property::Set(const QVariant& value) {
    if (value.isValid()) {
        if (definition_.IsInstanceReference) {
            if (value_ == value) {
                return;
            }

            const QVariant oldValue = value_;
            value_ = value;
            Changed.Fire(oldValue, value_);
            return;
        }

        QVariant converted = value;
        if (!converted.convert(definition_.Type)) {
            throw std::runtime_error(
                QString("Property '%1' expected '%2'")
                    .arg(definition_.Name, QString::fromUtf8(definition_.Type.name()))
                    .toStdString()
            );
        }

        if (value_ == converted) {
            return;
        }

        const QVariant oldValue = value_;
        value_ = converted;
        Changed.Fire(oldValue, value_);
        return;
    }

    if (!value_.isValid() && !value_.isNull()) {
        return;
    }

    const QVariant oldValue = value_;
    value_ = QVariant{};
    Changed.Fire(oldValue, value_);
}

} // namespace Lvs::Engine::Core
