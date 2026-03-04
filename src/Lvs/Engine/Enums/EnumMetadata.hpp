#pragma once

#include <QList>
#include <QVariant>

namespace Lvs::Engine::Enums::Metadata {

struct EnumOption {
    const char* Name;
    int Value;
};

QList<EnumOption> OptionsForType(int typeId);
QVariant VariantFromInt(int typeId, int value);
int IntFromVariant(const QVariant& value);
bool IsRegisteredEnumType(int typeId);

} // namespace Lvs::Engine::Enums::Metadata
