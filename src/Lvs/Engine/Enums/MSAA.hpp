#pragma once

#include <QMetaType>

namespace Lvs::Engine::Enums {

enum class MSAA {
    Off = 0,
    X2 = 2,
    X4 = 4,
    X8 = 8
};

} // namespace Lvs::Engine::Enums

Q_DECLARE_METATYPE(Lvs::Engine::Enums::MSAA)
