#pragma once

#include <QMetaType>

namespace Lvs::Engine::Enums {

enum class PartShape {
    Cube = 0,
    Sphere = 1,
    Cylinder = 2,
    Cone = 3
};

} // namespace Lvs::Engine::Enums

Q_DECLARE_METATYPE(Lvs::Engine::Enums::PartShape)
