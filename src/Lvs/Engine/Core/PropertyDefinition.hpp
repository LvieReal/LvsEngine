#pragma once

#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

namespace Lvs::Engine::Core {

struct PropertyDefinition {
    QString Name;
    QMetaType Type;
    QVariant Default;
    bool Serializable{true};
    QString Category{"Data"};
    QString Description;
    bool ReadOnly{false};
    QStringList CustomTags;
    QVariantMap CustomAttributes;
    bool IsInstanceReference{false};
    int RegistrationOrder{-1};
};

} // namespace Lvs::Engine::Core
