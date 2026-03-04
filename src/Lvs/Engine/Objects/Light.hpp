#pragma once

#include "Lvs/Engine/Core/Instance.hpp"

namespace Lvs::Engine::Objects {

inline constexpr int LIGHT_TYPE_DIRECTIONAL = 0;
inline constexpr int LIGHT_TYPE_POINT = 1;

class Light : public Core::Instance {
public:
    Light();
    ~Light() override = default;

    static Core::ClassDescriptor& Descriptor();

    [[nodiscard]] virtual int GetLightType() const;

protected:
    explicit Light(const Core::ClassDescriptor& descriptor);
};

} // namespace Lvs::Engine::Objects
