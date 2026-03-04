#pragma once

#include "Lvs/Engine/Math/CFrame.hpp"
#include "Lvs/Engine/Math/Color3.hpp"
#include "Lvs/Engine/Math/Vector3.hpp"

#include <QColor>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QVariant>

namespace Lvs::Studio::Widgets::Properties::ValueUtils {

struct EnumOption {
    QString Name;
    int Value;
};

QList<EnumOption> EnumOptionsForType(int typeId);
QString EnumNameFromTypeAndInt(int typeId, int value);
QVariant EnumVariantFromTypeAndInt(int typeId, int value);

QString FormatVector3(const Lvs::Engine::Math::Vector3& value);
bool TryParseVector3(const QString& text, Lvs::Engine::Math::Vector3& out);

QString FormatColor3(const Lvs::Engine::Math::Color3& value);

QString FormatCFrame(const Lvs::Engine::Math::CFrame& value);
bool TryParseCFrame(const QString& text, Lvs::Engine::Math::CFrame& out);

QColor ToQColor(const Lvs::Engine::Math::Color3& value);
Lvs::Engine::Math::Color3 FromQColor(const QColor& value);

} // namespace Lvs::Studio::Widgets::Properties::ValueUtils

