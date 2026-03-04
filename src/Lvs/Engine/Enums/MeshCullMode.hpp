#pragma once

#include <QMetaType>

namespace Lvs::Engine::Enums {

enum class MeshCullMode {
    NoCull = 0,
    Back = 1,
    Front = 2
};

} // namespace Lvs::Engine::Enums

Q_DECLARE_METATYPE(Lvs::Engine::Enums::MeshCullMode)
