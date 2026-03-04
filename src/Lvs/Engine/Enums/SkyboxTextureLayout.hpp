#pragma once

#include <QMetaType>

namespace Lvs::Engine::Enums {

enum class SkyboxTextureLayout {
    Individual = 0,
    Cross = 1
};

} // namespace Lvs::Engine::Enums

Q_DECLARE_METATYPE(Lvs::Engine::Enums::SkyboxTextureLayout)
