#pragma once

#include <QMetaType>

namespace Lvs::Engine::Enums {

enum class PartSurface {
    RightSurface = 0,
    LeftSurface = 1,
    TopSurface = 2,
    BottomSurface = 3,
    FrontSurface = 4,
    BackSurface = 5
};

} // namespace Lvs::Engine::Enums

Q_DECLARE_METATYPE(Lvs::Engine::Enums::PartSurface)
