#pragma once

#include <QList>
#include <QString>
#include <QVariant>

namespace Lvs::Engine::Enums::Metadata {

struct EnumOption {
    const char* Name;
    int Value;
};

QList<EnumOption> OptionsForType(int typeId);
QVariant VariantFromInt(int typeId, int value);
int IntFromVariant(const QVariant& value);
QString NameFromInt(int typeId, int value);
QString NameFromVariant(const QVariant& value);
QVariant VariantFromName(int typeId, const QString& nameOrNumber);
QVariant CoerceVariant(int typeId, const QVariant& value);
bool IsRegisteredEnumType(int typeId);

} // namespace Lvs::Engine::Enums::Metadata
