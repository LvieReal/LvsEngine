#pragma once

#include <QMetaType>

namespace Lvs::Engine::Enums {

enum class PartSurfaceType {
    Smooth = 0,
    Studs = 1,
    Inlets = 2
};

} // namespace Lvs::Engine::Enums

Q_DECLARE_METATYPE(Lvs::Engine::Enums::PartSurfaceType)
