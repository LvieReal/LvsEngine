#pragma once

#include <QMetaType>
#include <QString>

namespace Lvs::Engine::Math {

struct Color3 {
    double r{1.0};
    double g{1.0};
    double b{1.0};

    [[nodiscard]] QString ToString() const;
    [[nodiscard]] bool operator==(const Color3& other) const;
};

} // namespace Lvs::Engine::Math

Q_DECLARE_METATYPE(Lvs::Engine::Math::Color3)
