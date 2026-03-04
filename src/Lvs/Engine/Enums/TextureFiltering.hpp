#pragma once

#include <QMetaType>

namespace Lvs::Engine::Enums {

enum class TextureFiltering {
    Linear = 0,
    Nearest = 1
};

} // namespace Lvs::Engine::Enums

Q_DECLARE_METATYPE(Lvs::Engine::Enums::TextureFiltering)
