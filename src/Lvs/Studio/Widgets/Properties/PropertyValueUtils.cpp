#include "Lvs/Studio/Widgets/Properties/PropertyValueUtils.hpp"

#include "Lvs/Engine/Enums/EnumMetadata.hpp"
#include "Lvs/Qt/QtBridge.hpp"

#include <QRegularExpression>

#include <cmath>

namespace Lvs::Studio::Widgets::Properties::ValueUtils {

QList<EnumOption> EnumOptionsForEnum(const QString& enumType) {
    QList<EnumOption> options;
    const auto enumTypeStd = Lvs::Engine::Core::QtBridge::ToStdString(enumType);
    for (const auto& option : Lvs::Engine::Enums::Metadata::OptionsForEnum(enumTypeStd)) {
        options.push_back({QString::fromUtf8(option.Name), option.Value});
    }
    return options;
}

QString EnumNameFromEnumAndInt(const QString& enumType, const int value) {
    const QList<EnumOption> options = EnumOptionsForEnum(enumType);
    for (const auto& option : options) {
        if (option.Value == value) {
            return option.Name;
        }
    }
    return QString::number(value);
}

QVariant EnumVariantFromEnumAndInt(const QString& enumType, const int value) {
    const auto enumTypeStd = Lvs::Engine::Core::QtBridge::ToStdString(enumType);
    return Lvs::Engine::Core::QtBridge::ToQVariant(Lvs::Engine::Enums::Metadata::VariantFromInt(enumTypeStd, value));
}

QString FormatVector3(const Lvs::Engine::Math::Vector3& value) {
    return QString("%1, %2, %3").arg(value.x, 0, 'g', 10).arg(value.y, 0, 'g', 10).arg(value.z, 0, 'g', 10);
}

bool TryParseVector3(const QString& text, Lvs::Engine::Math::Vector3& out) {
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    const QRegularExpression separator("[,\\s]+");
    const QStringList parts = trimmed.split(separator, Qt::SkipEmptyParts);
    if (parts.size() == 1) {
        bool ok = false;
        const double value = parts[0].toDouble(&ok);
        if (!ok) {
            return false;
        }
        out = {value, value, value};
        return true;
    }
    if (parts.size() != 3) {
        return false;
    }

    bool okX = false;
    bool okY = false;
    bool okZ = false;
    const double x = parts[0].trimmed().toDouble(&okX);
    const double y = parts[1].trimmed().toDouble(&okY);
    const double z = parts[2].trimmed().toDouble(&okZ);
    if (!okX || !okY || !okZ) {
        return false;
    }
    out = {x, y, z};
    return true;
}

QString FormatColor3(const Lvs::Engine::Math::Color3& value) {
    return QString("%1, %2, %3").arg(value.r, 0, 'g', 10).arg(value.g, 0, 'g', 10).arg(value.b, 0, 'g', 10);
}

bool TryParseColor3(const QString& text, Lvs::Engine::Math::Color3& out) {
    Lvs::Engine::Math::Vector3 vec;
    if (!TryParseVector3(text, vec)) {
        return false;
    }
    out = {vec.x, vec.y, vec.z};
    return true;
}

QString FormatCFrame(const Lvs::Engine::Math::CFrame& value) {
    const auto rotation = value.ToEulerXYZ();
    return QString("%1, %2, %3 | %4, %5, %6")
        .arg(value.Position.x, 0, 'g', 10)
        .arg(value.Position.y, 0, 'g', 10)
        .arg(value.Position.z, 0, 'g', 10)
        .arg(rotation.x, 0, 'g', 10)
        .arg(rotation.y, 0, 'g', 10)
        .arg(rotation.z, 0, 'g', 10);
}

bool TryParseCFrame(const QString& text, Lvs::Engine::Math::CFrame& out) {
    const QStringList split = text.split('|');
    if (split.size() != 2) {
        return false;
    }

    Lvs::Engine::Math::Vector3 position;
    Lvs::Engine::Math::Vector3 rotation;
    if (!TryParseVector3(split[0], position) || !TryParseVector3(split[1], rotation)) {
        return false;
    }
    out = Lvs::Engine::Math::CFrame::FromPositionRotation(position, rotation);
    return true;
}

QColor ToQColor(const Lvs::Engine::Math::Color3& value) {
    return QColor(
        static_cast<int>(std::round(value.r * 255.0)),
        static_cast<int>(std::round(value.g * 255.0)),
        static_cast<int>(std::round(value.b * 255.0))
    );
}

Lvs::Engine::Math::Color3 FromQColor(const QColor& value) {
    return {value.red() / 255.0, value.green() / 255.0, value.blue() / 255.0};
}

} // namespace Lvs::Studio::Widgets::Properties::ValueUtils
