#pragma once

#include <QMetaType>

namespace Lvs::Engine::Enums {

enum class MSAA {
    Off = 0,
    X2 = 1,
    X4 = 2,
    X8 = 3
};

[[nodiscard]] constexpr int MsaaSampleCount(const MSAA value) {
    switch (value) {
        case MSAA::X2:
            return 2;
        case MSAA::X4:
            return 4;
        case MSAA::X8:
            return 8;
        case MSAA::Off:
        default:
            return 1;
    }
}

} // namespace Lvs::Engine::Enums

Q_DECLARE_METATYPE(Lvs::Engine::Enums::MSAA)
