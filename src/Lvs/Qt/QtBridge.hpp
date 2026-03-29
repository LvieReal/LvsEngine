#pragma once

#include "Lvs/Engine/Core/Types.hpp"
#include "Lvs/Engine/Core/Variant.hpp"

class QString;
class QVariant;

namespace Lvs::Engine::Core::QtBridge {

String ToStdString(const QString& s);
QString ToQString(const String& s);

Variant ToStdVariant(const QVariant& v);
QVariant ToQVariant(const Variant& v);

} // namespace Lvs::Engine::Core::QtBridge
