#include "Lvs/Qt/QtBridge.hpp"

#include "Lvs/Qt/QtMetaTypes.hpp"

#include "Lvs/Engine/Core/Instance.hpp"
#include "Lvs/Engine/Math/CFrame.hpp"
#include "Lvs/Engine/Math/Color3.hpp"
#include "Lvs/Engine/Math/Vector3.hpp"

#include <QMetaType>
#include <QString>
#include <QVariant>

#include <cstdint>
#include <memory>

namespace Lvs::Engine::Core::QtBridge {

String ToStdString(const QString& s) {
    return s.toUtf8().toStdString();
}

QString ToQString(const String& s) {
    return QString::fromUtf8(s.c_str());
}

Variant ToStdVariant(const QVariant& v) {
    if (!v.isValid() || v.isNull()) {
        return Variant{};
    }

    const int typeId = v.typeId();
    if (typeId == QMetaType::Bool) {
        return Variant::From(v.toBool());
    }
    if (typeId == QMetaType::Int) {
        return Variant::From(static_cast<int64_t>(v.toInt()));
    }
    if (typeId == QMetaType::LongLong) {
        return Variant::From(static_cast<int64_t>(v.toLongLong()));
    }
    if (typeId == QMetaType::Double) {
        return Variant::From(v.toDouble());
    }
    if (typeId == QMetaType::QString) {
        return Variant::From(ToStdString(v.toString()));
    }
    if (typeId == QMetaType::fromType<Math::Vector3>().id()) {
        return Variant::From(v.value<Math::Vector3>());
    }
    if (typeId == QMetaType::fromType<Math::Color3>().id()) {
        return Variant::From(v.value<Math::Color3>());
    }
    if (typeId == QMetaType::fromType<Math::CFrame>().id()) {
        return Variant::From(v.value<Math::CFrame>());
    }
    if (v.canConvert<std::shared_ptr<Core::Instance>>()) {
        const auto inst = v.value<std::shared_ptr<Core::Instance>>();
        return Variant::From(Variant::InstanceRef{inst});
    }

    if (v.canConvert<int>()) {
        return Variant::From(v.toInt());
    }
    if (v.canConvert<double>()) {
        return Variant::From(v.toDouble());
    }

    return Variant::From(ToStdString(v.toString()));
}

QVariant ToQVariant(const Variant& v) {
    if (!v.IsValid() || v.IsNull()) {
        return QVariant{};
    }

    switch (v.GetTypeId()) {
    case TypeId::Invalid:
        return QVariant{};
    case TypeId::Bool:
        return QVariant(v.Get<bool>());
    case TypeId::Int:
        return QVariant::fromValue<qint64>(static_cast<qint64>(v.Get<int64_t>()));
    case TypeId::Double:
        return QVariant(v.Get<double>());
    case TypeId::String:
        return QVariant(ToQString(v.Get<String>()));
    case TypeId::Vector3:
        return QVariant::fromValue(v.Get<Math::Vector3>());
    case TypeId::Color3:
        return QVariant::fromValue(v.Get<Math::Color3>());
    case TypeId::CFrame:
        return QVariant::fromValue(v.Get<Math::CFrame>());
    case TypeId::Enum:
        return QVariant(v.Get<int>());
    case TypeId::InstanceRef: {
        const auto locked = v.Get<Variant::InstanceRef>().lock();
        if (!locked) {
            return QVariant{};
        }
        return QVariant::fromValue(locked);
    }
    case TypeId::ByteArray:
        return QVariant{};
    }
    return QVariant{};
}

} // namespace Lvs::Engine::Core::QtBridge

