#pragma once

#include "Lvs/Engine/Core/Types.hpp"
#include "Lvs/Engine/Core/Variant.hpp"

#if defined(LVS_WITH_QT_BRIDGE)

class QString;
class QVariant;

namespace Lvs::Engine::Core::QtBridge {

String ToStdString(const QString& s);
QString ToQString(const String& s);

Variant ToStdVariant(const QVariant& v);
QVariant ToQVariant(const Variant& v);

} // namespace Lvs::Engine::Core::QtBridge

#endif // defined(LVS_WITH_QT_BRIDGE)

